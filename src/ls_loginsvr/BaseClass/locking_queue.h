#pragma once

#include <stdexcept>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>

template<
    typename T,
    typename Container = std::queue<T>
>
class locking_queue {
private:
    typedef std::lock_guard<std::mutex> lock_guard;
    typedef std::unique_lock<std::mutex> unique_lock;

public:
    class queue_empty {};

    typedef Container container_type;
    typedef typename Container::value_type value_type;
    typedef typename Container::size_type size_type;

    locking_queue(const locking_queue&) = delete;
    locking_queue& operator=(const locking_queue&) = delete;

    locking_queue()
        : unfinished_tasks(0)
    {}

    explicit locking_queue(const container_type& other)
        : container(other), unfinished_tasks(container.size())
    {}

    bool empty() const {
        lock_guard guard(mutex);
        return container.empty();
    }

    size_type size() const {
        lock_guard guard(mutex);
        return container.size();
    }

    value_type pop(bool block = false, int timeout = 0) {
        unique_lock lock(mutex);
        pop_common(lock, block, timeout);
        value_type element(container.front());
        container.pop();
        return element;
    }

    void pop_safe(value_type& element, bool block = false, int timeout = 0) {
        unique_lock lock(mutex);
        pop_common(lock, block, timeout);
        element = container.front();
        container.pop();
    }

    void push(const value_type& element) {
        {
            lock_guard guard(mutex);
            container.push(element);
            unfinished_tasks++;
        }
        non_empty.notify_one();
    }

    void task_done() {
        lock_guard guard(mutex);
        unsigned long unfinished = unfinished_tasks - 1;
        if (unfinished < 0) {
            throw std::logic_error("Task done reported more times than the number of elements in the queue");
        }
        if (unfinished == 0) {
            all_tasks_done.notify_all();
        }
        unfinished_tasks = unfinished;
    }

    void join() const {
        unique_lock lock(mutex);
        while (unfinished_tasks) {
            all_tasks_done.wait(lock);
        }
    }

private:
    void pop_common(unique_lock& lock, bool block, int timeout) {
        if (block) {
            while (container.empty()) {
                if (timeout > 0) {
                    if (non_empty.wait_for(lock, std::chrono::seconds(timeout)) == std::cv_status::timeout) {
                        throw queue_empty();
                    }
                } else {
                    non_empty.wait(lock);
                }
            }
        } else {
            if (container.empty()) {
                throw queue_empty();
            }
        }
    }

private:
    container_type container;
    mutable std::mutex mutex;
    mutable std::condition_variable non_empty;
    unsigned long unfinished_tasks;
    mutable std::condition_variable all_tasks_done;
};
