#pragma once

#include <iostream>
#include <condition_variable> // Used to synchronize access to the buffer
#include <mutex> // Used to protect access to the buffer
#include <vector>

class ringbuffer
{
private:
    int capacity;
    std::vector<char> buffer;
    int head;
    int tail;
    int count;
    std::mutex mtx;
    std::condition_variable not_full_cv;
    std::condition_variable not_empty_cv;

public:
    ringbuffer(int capacity);
    ~ringbuffer() = default;

    void push(char value);
    char pop();
};
