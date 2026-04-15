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
    std::unique_lock<std::mutex> lock(mtx);
    not_full_cv.wait(lock, [this]()
                     { return count < capacity; });

    buffer[tail] = value;
    tail = (tail + 1) % capacity;
    ++count;

    not_empty_cv.notify_one();
}

char ringbuffer::pop()
{
    std::unique_lock<std::mutex> lock(mtx); 
    not_empty_cv.wait(lock, [this]()
                      { return count > 0; });

    char value = buffer[head];
    head = (head + 1) % capacity;
    --count;

    not_full_cv.notify_one();
    return value;
}
