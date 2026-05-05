#include <iostream>
#include <condition_variable>
#include <chrono>
#include <vector>
#include <mutex>

template <typename T>
class ringbuffer{
private:
    int size; 
    int count;
    int head;
    int tail;
    std::vector<T> buffer;

    std::mutex mtx;

    std::condition_variable not_full;
    std::condition_variable not_empty;

public:
    ringbuffer(int size) : size(size), count(0), buffer(size), head(0), tail(0) {};
    
    T pop(){
        std::unique_lock<std::mutex> lock(mtx);
        not_empty.wait(lock, [this](){return size > 0;});
        T print;
        buffer[tail] = print;
        tail = (tail + 1) % size;
        count--;
        not_full.notify_one();
        return print;
    }

    void push(T x){}

};