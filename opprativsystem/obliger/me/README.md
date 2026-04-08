# Multithreaded Ring Buffer Demo

This project shows a simple producer/consumer system in C++ using:
- `std::thread`
- `std::mutex`
- `std::condition_variable`
- `std::atomic<bool>`

## What happens when you run it

`main()` creates a `parent` object and calls `run()`.

Inside `run()`, three threads are started:

1. Keyboard producer (`keyboard_worker`)
2. Generator producer (`generator_worker`)
3. Consumer (`consumer_worker`)

The two producers push characters into a shared `ringbuffer`, and the consumer pops and prints them.

## File overview

- `001_main.cpp`
	- Program entry point.
	- Creates `parent system;` and calls `system.run();`.

- `100_parent.hpp` / `101_parent.cpp`
	- Owns threads and shared `ringbuffer buf`.
	- Coordinates startup and shutdown.
	- Implements producer and consumer worker functions.

- `200_ringbuffer.hpp` / `201_ringbuffer.cpp`
	- Thread-safe bounded buffer for `char` values.
	- `push()` blocks when full.
	- `pop()` blocks when empty.

## Thread roles

### 1) Keyboard producer

Reads characters from `std::cin` one by one and pushes them into the buffer.

When input ends (Ctrl-D on Linux/macOS), it pushes a special end marker:
- `\x1d`

### 2) Generator producer

Generates 10 messages like:
- `Generator 1: ringbuffer activity`

Each message is pushed character-by-character into the buffer.
There is a short delay (`100 ms`) between messages.

When done, it pushes a second end marker:
- `\x1e`

### 3) Consumer

Continuously pops characters from the buffer.

- Normal characters are printed to `std::cout`.
- Marker characters (`\x1d`, `\x1e`) are not printed.
- The consumer exits only after it has received both markers (one from each producer).

This ensures it does not stop until both producers are done.

## How the ring buffer synchronization works

`ringbuffer` uses one mutex + one condition variable:

- `push(char value)`
	- Waits while buffer is full (`size == capacity`).
	- Adds item.
	- Notifies waiting threads.

- `pop()`
	- Waits while buffer is empty.
	- Removes and returns front item.
	- Notifies waiting threads.

This prevents race conditions and avoids busy-wait loops.

## Program lifecycle

1. `parent::run()` starts all 3 threads.
2. Producers feed the buffer.
3. Consumer drains and prints.
4. Consumer stops after seeing both end markers.
5. `run()` joins all threads before returning.

The destructor also attempts to join any still-joinable threads as a safety net.

## Build and run (main)

```bash
g++ 001_main.cpp 101_parent.cpp 201_ringbuffer.cpp -o main
./main
```

Then type input and finish with Ctrl-D to close keyboard input.
