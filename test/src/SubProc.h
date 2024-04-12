#pragma once

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
#include <boost/interprocess/anonymous_shared_memory.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/managed_external_buffer.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>

typedef std::optional<std::chrono::steady_clock::time_point> AbsTimeout;

template <typename T>
struct SharedVarImpl {
    typedef boost::interprocess::interprocess_mutex Mutex;
    typedef boost::interprocess::interprocess_condition CondVar;
    typedef boost::interprocess::scoped_lock<Mutex> Lock;
    Mutex mtx;
    CondVar cv;
    size_t in_cntr;
    size_t out_cntr;
    bool wake_flag;
    T data;

    SharedVarImpl() : in_cntr(0), out_cntr(0), data(0), wake_flag(false)
    {
    }

    size_t post(const T& data)
    {
        Lock lock(mtx);
        this->data = data;
        in_cntr++;
        cv.notify_all();
        return in_cntr;
    }

    void wake()
    {
        using namespace std;
        Lock lock(mtx);
        wake_flag = true;
        cv.notify_all();
    }

    std::tuple<size_t, T> wait(AbsTimeout abs_time = AbsTimeout())
    {
        Lock lock(mtx);
        while (!wake_flag) {
            if (out_cntr < in_cntr) {
                out_cntr++;
                return std::make_tuple(out_cntr, data);
            }
            if (abs_time) {
                cv.wait_until(lock, *abs_time);
                auto now = std::chrono::steady_clock::now();
                if (now > *abs_time) {
                    std::stringstream ss;
                    ss << "Timeout while pid=" << getpid()  << " waiting.";
                    throw std::runtime_error(ss.str());                    
                }
            }
            else cv.wait(lock);
            // cv.wait(lock);
        }
        std::stringstream ss;
        ss << "Wake called while pid=" << getpid()  << " waiting.";
        throw std::runtime_error(ss.str());
    }
};

template <typename T>
class SharedVar {
    boost::interprocess::mapped_region* region;
    boost::interprocess::managed_external_buffer* manager;
    SharedVarImpl<T>* ptr;
public:

    SharedVar() {
        using namespace boost::interprocess;
        region = new mapped_region(anonymous_shared_memory(65536));
        manager = new managed_external_buffer(create_only, region->get_address(), region->get_size());
        ptr = manager->construct<SharedVarImpl<T>>("ptr")();
    }

    ~SharedVar()
    {
        if (manager) manager->destroy_ptr(ptr);
        else delete ptr;
        if (manager) delete manager;
        if (region) delete region;
    }

    size_t post(const T& data)
    {
        return ptr->post(data);
    }

    void wake()
    {
        ptr->wake();
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
        using namespace std;
        child_pid = -1;
        tid = nullptr;
        up_data = std::make_unique<SharedVar<UP>>();
        down_data = std::make_unique<SharedVar<DOWN>>();
    }

    ~SubProc()
    {
        down_data->wake();
        usleep(1000);
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
        else {
            tid = new std::thread([&] () { fn(*up_data, *down_data, std::forward<Args>(args)...); });
        }
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
