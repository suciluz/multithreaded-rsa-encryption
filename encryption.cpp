#include <iostream>
#include <condition_variable>
#include <mutex>
#include <future>
#include <thread>
#include <atomic>
#include <fstream>
#include <cstdlib>
#include <cmath>
#include <utility>
#include <map>
#include <string>
#include <queue>
#include <vector>
#include <chrono>
#include "circular_buffer.hpp"

using namespace std;
#define RANGE 256

int E, N;

struct ThreadControl {
    mutex m;
    condition_variable ready;
    queue<pair<int, vector<string>>> workQ;
    atomic<bool> quit {false};
    map<int, vector<int>> encrypted;
    
    ThreadControl() = default;
    ThreadControl(const ThreadControl&) = delete;
    ThreadControl& operator=(const ThreadControl&) = delete;
};

bool isPrime (int n) {
    if(n <= 1) return false;
    if(n == 2) return true;
    if(n % 2 == 0) return false;
    for(int i=3; i <= sqrt(n); i+=2) {
        if(n % i ==0) return false;
    }
    return true;
}

std::pair<int,int> generatePrime () {
    int p = rand() % RANGE, q = rand() % RANGE;
    while(!isPrime(p)) {
        p = rand() % RANGE;
    }
    while(!isPrime(q)) {
        q = rand() % RANGE;
    }
    return std::make_pair(p,q);
}

//Extended Euclidean Algorithm
int extended_gcd(int a, int b) {
    int x = 0, lastX = 1, y = 1, lastY = 0;
    while(b != 0) {
        int tmp = b;
        int q = a/b;
        b = a%b;
        a = tmp;
        tmp = x;
        x = lastX - q*x;
        lastX = tmp;
        tmp = y;
        y = lastY - q*y;
        lastY = tmp;
    }
    return a;
}

int calculateE (int t) {
    int e = rand() % t;
    while((extended_gcd(e,t) != 1) || e == 0) e = rand() % t;
    return e;
}

int calculateD (int t, int e) {
    bool eligible = 0;
    while (!eligible) {
        int k = rand() % t;
        int d = (int) (1 + k * t) / e;
        if(k == (d * e - 1) / t) {
            eligible = 1;
            return d;
        }
    }
    return -1;
}

//fast modular exponentiation - calculate b^e mod m efficiently in O(log e)
int encrypt (int b, int e, int m) {
    int result = 1;
    while (e > 0) {
        if((e & 1) == 1) {
            result = (result * b) % m;
        }
        e >>= 1;
        b = (b * b) % m;
    }
    return result;
}

void encryptProcessor (ThreadControl& tc) {
    while(1) {
        pair <int, vector<string>> workToDo;
        {
            unique_lock lock(tc.m);
            if(tc.quit && tc.workQ.empty()) {
                break;
            }
            else if(tc.workQ.empty()) {
                cout << "(" << this_thread::get_id()  << ") No work, waiting for text to encrypt" << endl;
                tc.ready.wait(lock, [&tc]{return !tc.workQ.empty();});
            }
            
            workToDo = tc.workQ.front();
            cout << "(" << this_thread::get_id() << ") Encrypting txt #" << workToDo.first << endl;
            tc.workQ.pop();
        }
        
        vector <int> tmp;
        for(auto line : workToDo.second) {
            for(int i=0; i<line.length(); i++) {
                int M = line.at(i);
                tmp.push_back(encrypt(M, E, N));
            }
        }
        
        {
            unique_lock lock(tc.m);
            tc.encrypted[workToDo.first] = tmp;
        }
    }
}

void outputEncryption (ThreadControl& tc, string fileName) {
    ofstream os(fileName);
    for(int i=0; i<tc.encrypted.size(); ++i) {
        vector <int> tmp = tc.encrypted[i];
        for(int j=0; j<tmp.size(); ++j) {
            
            os << tmp[j] << '\n';
        }
    }
    os.close();
}

int main() {
    pair<int, int> primes = generatePrime();
    int p = primes.first, q = primes.second;
    N = p*q;
    int t = (p-1) * (q-1);
    E = calculateE(t);
    //Private key
    int d = calculateD(t, E);
    queue<string> lines;
    
    
    string fileName = "", fileName2 = "";
    cout << "Enter the name of the text file that you want to encrypt: " << endl;
    cin >> fileName;
    ifstream is(fileName);
     
    cout << "Enter the name of the text file where the encryption will be stored: " << endl;
    cin >> fileName2;
     
    int lineNumber = 0;
    string line = "";
    
    if(!is.is_open()) {
        cout << "Error: could not find file" << endl;
        return 1;
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    
    while(!is.eof()) {
        lineNumber++;
        getline(is, line);
        lines.push(line);
        /*
         for(int i = 0; i < line.length(); ++i) {
             int M = line.at(i);
             os << encrypt(M, e, n) << '\n';
         }
         os << encrypt(10, e, n) << '\n';*/
    }
    is.close();
    unsigned int divy = lineNumber / thread::hardware_concurrency();
    cout << "divy: " << divy << endl;
    cout << lineNumber << endl;
    
    ThreadControl tc;
    
    /*
    CircularBuffer<thread, 12> buffer;
    auto bufInsert = back_insert_iterator<CircularBuffer<thread,12>>(buffer);
    for(int i=0; i< 13; i++) {
        *bufInsert.join();
    }
    */
    
    vector<thread> threads;
    //initialize threads
    for (unsigned int i = 0; i < thread::hardware_concurrency(); ++i) {
      thread t(&encryptProcessor, std::ref(tc));
      threads.push_back(std::move(t));
    }
     
    
    int offset = 0;
    while(!lines.empty()) {
        vector<string> tmp;
        
        for(int i=0; i < divy; i++) {
            if(!lines.empty()) {
                tmp.push_back(lines.front());
                lines.pop();
            } else {
                break;
            }
        }
        
        if(!tmp.empty()) {
            {
                unique_lock lock(tc.m);
                tc.workQ.push({offset, tmp});
                cout << "pushing text chunk #" << offset++ << endl;
            }
            tc.ready.notify_all();
        } else {
            break;
        }
        
    }
    
    
    tc.quit.store(true);
    for_each(begin(threads), end(threads), [](thread& t) {t.join();});
     
    outputEncryption(tc, fileName2);
     
    cout << "Your message was successfully encrypted. Your public key is: (" << E << "," << N << "), and your private key is: (" << d << "," << N << ")." << endl;
    
    auto stop = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - start);
    cout << "The program took " << duration.count() <<" microseconds to execute!" << endl;
    return 0;
}
//22712566
//30424946
