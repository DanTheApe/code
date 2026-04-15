#include "100_parent.hpp"

parent::parent() : stop_flag(false), buf(10) {}

parent::~parent()
{
    stop_flag = true;
    if (keyboard_thread.joinable())
        keyboard_thread.join();
    if (generator_thread.joinable())
        generator_thread.join();
    if (consumer_thread.joinable())
        consumer_thread.join();
}

void parent::keyboard_worker(parent *p)
{
    std::cout << "Keyboard producer: type text, finish with Ctrl-D\n";
    char ch;
    while (std::cin.get(ch))
    {
        p->buf.push(ch);
    }
    p->buf.push('\x1d');
}

void parent::generator_worker(parent *p)
{
    for (int i = 1; i <= 10; ++i)
    {
        std::string msg = "Generator " + std::to_string(i) + ": ringbuffer activity\n";
        for (char ch : msg)
        {
            p->buf.push(ch);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    p->buf.push('|');
}

void parent::consumer_worker(parent *p)
{
    int eof_count = 0;
    while (eof_count < 2)
    {
        char ch = p->buf.pop();
        if (ch == '\x1d' || ch == '|')
        {
            ++eof_count;
            continue;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::cout << ch << std::flush; // Ting kom i chunckes når det ikke var std::flush.
    }
    std::cout << std::endl;
}

void parent::run()
{
    keyboard_thread = std::thread(keyboard_worker, this);
    generator_thread = std::thread(generator_worker, this);
    consumer_thread = std::thread(consumer_worker, this);

    if (keyboard_thread.joinable())
        keyboard_thread.join();
    if (generator_thread.joinable())
        generator_thread.join();
    if (consumer_thread.joinable())
        consumer_thread.join();
}
