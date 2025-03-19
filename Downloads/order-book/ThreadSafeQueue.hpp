#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>

// Templates should be completely instantiated in header file so compiler can generate the appropriate instantiation

template<typename T>
class ThreadSafeQueue {
public:
    // default constructor
    ThreadSafeQueue() = default;
    // no copy constructo, copying a queue w a mutex / conditional variable migjt pose an issue, copies might interefere with each other
    ThreadSafeQueue(const ThreadSafeQueue&) = delete;
    // cannot assign one thread safe queue to another
    ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;

    // Push a new item into the queue.
    // this is for when the caller indicates that the object remains valid state after this push
    void push(const T& value) {
        {   
            // immediately locks the mutex
            // lock guaard is an RAII wrapper that automatically locks a mutex upon creation and unlocks it when goes out of scope
            std::lock_guard<std::mutex> lock(mtx);
            q.push(value);
        }
        // after value is pushed, this wakes up one thread that might be blocked in a wait_pop call
        cv.notify_one(); // Notify one waiting thread.
    }

    // Overloaded push for rvalue references.
    // this for when the caller indicates that the object can be safelty moved

    void push(T&& value) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            // converts the rvalue to an xvalue, enabling the queue to "steal" /move the internal resources of value instead of copying them
            q.push(std::move(value));
        }
        cv.notify_one();
    }

    // Try to pop an item; returns false if the queue is empty.
    bool try_pop(T& result) {
        std::lock_guard<std::mutex> lock(mtx);
        if (q.empty())
            return false;
        result = std::move(q.front());
        q.pop();
        return true;
    }

    // Wait until an item is available, then pop it.
    void wait_pop(T& result) {
        std::unique_lock<std::mutex> lock(mtx);
        // released the mutex, held in lock and puts the thread to sleep until this condition variable is notified
        cv.wait(lock, [this]() { return !q.empty(); });
        result = std::move(q.front());
        q.pop();

    }

    // Can also have a wait_pop_for, if there is a timeout, then u can wake and thread can see if it shld exit for example
    // also can have wait_pop_for if there is a need to perform some periodic tasks like logging, updating metrics or rebalancing work

    // Check if the queue is empty.
    bool empty() const {
        std::lock_guard<std::mutex> lock(mtx);
        return q.empty();
    }

private:
    mutable std::mutex mtx;
    std::queue<T> q;
    std::condition_variable cv;
};
