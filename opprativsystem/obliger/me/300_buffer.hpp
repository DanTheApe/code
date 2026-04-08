#pragma once

#include <condition_variable>
#include <mutex>
#include <deque>

#include <iostream>
#include <string>
#include <chrono> // For sleep
#include <iostream>
#include <condition_variable>
#include <mutex>
#include <vector>

class ringbuffer{
private:
    int capacity;
    std::deque<char> buffer;
    std::mutex mtx;
    std::condition_variable cv;

public:
    ringbuffer(int capacity);
    ~ringbuffer() = default;

    void push(char value);
    char pop();
};
