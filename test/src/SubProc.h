#pragma once

#include <unistd.h>
#include <sys/mman.h>
#include <signal.h>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <optional>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <sstream>
#include <thread>
#include <iostream>

typedef std::optional<std::chrono::time_point<std::chrono::steady_clock>> AbsTimeout;

template <typename T>
struct SharedVarImpl {
    std::mutex mtx;
    std::condition_variable cv;
    size_t in_cntr;
    size_t out_cntr;
    T data;

    SharedVarImpl() : in_cntr(0), out_cntr(0), data(0)
    {
    }

    size_t post(const T& data)
    {
        std::unique_lock<std::mutex> lock(mtx);
        this->data = data;
        in_cntr++;
        cv.notify_all();
        return in_cntr;
    }

    std::tuple<size_t, T> wait(AbsTimeout abs_time = AbsTimeout())
    {
        if (abs_time) {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait_until(lock, *abs_time, [&]() { return out_cntr != in_cntr; });
            if (in_cntr == out_cntr) {
                std::stringstream ss;
                ss << "timeout pid=" << getpid();
                throw std::runtime_error(ss.str());
            }
            out_cntr++;
            return std::make_tuple(out_cntr, data);
        }
        else {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [&]() { return out_cntr != in_cntr; });
            out_cntr++;
            return std::make_tuple(out_cntr, data);
        }
    }
};

template <typename T>
class SharedVar {
    void* raw_ptr;
    SharedVarImpl<T>* ptr;
public:

    SharedVar(bool use_processes) {
        if (use_processes) {
            raw_ptr = mmap(0, 4096, PROT_READ|PROT_WRITE, MAP_ANON|MAP_SHARED, -1, 0);
            ptr = new (raw_ptr) SharedVarImpl<T>();
        }
        else {
            raw_ptr = nullptr;
            ptr = new SharedVarImpl<T>();
        }
    }

    ~SharedVar()
    {
        if (raw_ptr != nullptr) munmap(raw_ptr, 4096);
        else delete ptr;
    }

    size_t post(const T& data)
    {
        return ptr->post(data);
    }

    std::tuple<size_t, T> wait(AbsTimeout timeout = AbsTimeout())
    {
        return ptr->wait(timeout);
    }
};

template <typename UP, typename DOWN>
class SubProc {
    bool use_processes;
    std::unique_ptr<SharedVar<UP>>   up_data;
    std::unique_ptr<SharedVar<DOWN>>   down_data;
    int child_pid;
    std::thread* tid;
public:

    SubProc(bool use_processes = true) : use_processes(use_processes)
    {
        child_pid = -1;
        tid = nullptr;
        up_data = std::make_unique<SharedVar<UP>>(use_processes);
        down_data = std::make_unique<SharedVar<DOWN>>(use_processes);
    }

    ~SubProc()
    {
        if (child_pid != -1) {
            kill(SIGINT, child_pid);
            child_pid = -1;
        }
        if (tid) {
            delete tid;
        }
    }

    template <typename FN, class... Args>
    void run(FN fn, Args&&... args)
    {
        if (use_processes) {
            auto pid = fork();
            if (pid == 0) {
                fn(*up_data, *down_data, std::forward<Args>(args)...);
                _exit(0);
            }
            else {
                child_pid = pid;
            }
        }
        // else {
        //     tid = new std::thread([&] () { fn(*up_data, *down_data, std::forward<Args>(args)...); });
        // }
    }

    std::tuple<size_t, UP> wait(AbsTimeout timeout = AbsTimeout())
    {
        return up_data->wait(timeout);
    }

    size_t post(const DOWN& data)
    {
        return down_data->post(data);
    }
};
