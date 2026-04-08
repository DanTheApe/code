#include "300_buffer.hpp"

ringbuffer::ringbuffer(int capacity) : capacity(capacity) {}

void ringbuffer::push(char value) {
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [this]() { return static_cast<int>(buffer.size()) < capacity; });
    buffer.push_back(value);
    cv.notify_all();
}

char ringbuffer::pop() {
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [this]() { return !buffer.empty(); });
    char value = buffer.front();
    buffer.pop_front();
    cv.notify_all();
    return value;
}