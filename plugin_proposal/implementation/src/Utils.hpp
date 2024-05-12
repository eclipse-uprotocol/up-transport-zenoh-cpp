#pragma once

#include <mutex>
#include <condition_variable>
#include <deque>
#include <thread>
#include <vector>

template <typename NODE>
class Fifo {
    std::mutex  mtx;
    std::condition_variable cv;
    std::deque<std::shared_ptr<NODE>> queue;
    bool die;
public:
    Fifo() { die = false; }

    void push(std::shared_ptr<NODE> ptr)
    {
        std::unique_lock<std::mutex> lock(mtx);
        queue.push_front(ptr);
        cv.notify_one();        
    }

    std::shared_ptr<NODE> pull()
    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [&](){ return !queue.empty() || die; });
        if (die) return nullptr;
        auto ptr = queue.back();
        queue.pop_back();
        return ptr;        
    }

    void exit()
    {
        std::unique_lock<std::mutex> lock(mtx);
        die = true;
        cv.notify_all();
    }
};

class ThreadPool {
    std::vector<std::thread>    pool;
public:
    ThreadPool(std::function<void ()> fn, size_t count)
    {
        pool.reserve(count);
        for (size_t i = 0; i < count; i++) pool.emplace_back(fn);
    }

    ~ThreadPool()
    {
        for (auto& thr : pool) thr.join();
        pool.clear();
    }
};