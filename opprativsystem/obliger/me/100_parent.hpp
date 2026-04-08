#pragma once

#include "200_ringbuffer.hpp"

#include <thread>
#include <atomic> // stop_flag: which is an atomic boolean to signal threads to stop
#include <iostream>
#include <chrono> // For sleep

class parent
{
private:
    std::thread keyboard_thread;
    std::thread generator_thread;
    std::thread consumer_thread;

    std::atomic<bool> stop_flag;
    ringbuffer buf;

    static void keyboard_worker(parent *p);
    static void generator_worker(parent *p);
    static void consumer_worker(parent *p);

public:
    parent();
    ~parent();
    void run();
};