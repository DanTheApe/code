#include "200_ringbuffer.hpp"

ringbuffer::ringbuffer(int capacity)
    : capacity(capacity),
      buffer(capacity),
      head(0),
      tail(0),
      count(0)
{
    if (capacity <= 0)
    {
        std::cout << "Yo what are you doing dummy?" << std::endl;
        exit(1);
    }
}

void ringbuffer::push(char value)
{
    std::unique_lock<std::mutex> lock(mtx); // Acquire the lock before modifying the buffer
    not_full_cv.wait(lock, [this]()
                     { return count < capacity; }); // Wait until there is space in the buffer

    buffer[tail] = value;
    tail = (tail + 1) % capacity;
    ++count;

    not_empty_cv.notify_one(); // Notify one waiting consumer that there is now data in the buffer
}

char ringbuffer::pop()
{
    std::unique_lock<std::mutex> lock(mtx); // Acquire the lock before modifying the buffer
    not_empty_cv.wait(lock, [this]()
                      { return count > 0; }); // Wait until there is data in the buffer

    char value = buffer[head];
    head = (head + 1) % capacity;
    --count;

    not_full_cv.notify_one(); // Notify one waiting producer that there is now space in the buffer
    return value;
}