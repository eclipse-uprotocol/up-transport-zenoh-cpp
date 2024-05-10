#include <iostream>
// #include <vector>
// #include <map>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <thread>
#include "PluginApi.hpp"
#include "zenohc.hxx"

using namespace std;

namespace PluggableTransport {

class SubscriberImpl;

static zenohc::Session inst()
{
    zenohc::Config config;
    return zenohc::expect<zenohc::Session>(zenohc::open(std::move(config)));
}

struct SessionImpl : public SessionApi {
    string start_doc;
    zenohc::Session session;

    SessionImpl(const string& start_doc) : start_doc(start_doc), session(inst())
    {
        cout << __FILE__ << " SessionImpl with " << start_doc << " in " << __FILE__ << endl;
    }
};

struct PublisherImpl : public PublisherApi {
    shared_ptr<SessionImpl> session;
    string keyexpr;
    z_owned_publisher_t handle;
    
    PublisherImpl(shared_ptr<SessionApi> session_base, const string& keyexpr) : keyexpr(keyexpr)
    {
        session = dynamic_pointer_cast<SessionImpl>(session_base);
        cout << __FILE__ << " creating publisher for " << keyexpr << endl;
        handle = z_declare_publisher(session->session.loan(), z_keyexpr(keyexpr.c_str()), nullptr);
        if (!z_check(handle)) throw std::runtime_error("Cannot declare publisher");
    }

    ~PublisherImpl()
    {
        z_undeclare_publisher(&handle);
    }

    void operator()(const std::string& payload, const std::string& attributes)
    {
        cout << __FILE__ << " publishing payload=" << payload << " attributes=" << attributes << " to " << keyexpr << endl;
        z_publisher_put_options_t options = z_publisher_put_options_default();
        z_owned_bytes_map_t map = z_bytes_map_new();
        options.attachment = z_bytes_map_as_attachment(&map);
        z_bytes_map_insert_by_alias(&map, z_bytes_new("attributes"), z_bytes_t{.len=attributes.size(), .start=(const uint8_t*)attributes.data()});
        if (z_publisher_put(z_loan(handle), (const uint8_t*)payload.data(), payload.size(), &options)) {
            z_drop(z_move(map));
            throw std::runtime_error("Cannot publish");
        }
        z_drop(z_move(map));
    }
};

struct SubInfo {
    string  keyexpr;
    string  payload;
    string  attributes;

    SubInfo(const zenohc::Sample& sample)
    {
        keyexpr = sample.get_keyexpr().as_string_view();
        payload = sample.get_payload().as_string_view();
        attributes = sample.get_attachment().get("attributes").as_string_view();
    } 
};

struct SubscriberImpl : public SubscriberApi {
    shared_ptr<SessionImpl> session;
    unique_ptr<zenohc::Subscriber> handle;
    zenohc::KeyExprView expr;
    mutex  mtx;
    condition_variable cv;
    bool die;
    deque<shared_ptr<SubInfo>> queue;
    vector<thread> thread_pool;
    SubscriberServerCallback callback;

    void handler(const zenohc::Sample& sample)
    {
        auto ptr = make_shared<SubInfo>(sample);
        unique_lock<std::mutex> lock(mtx);
        queue.push_front(ptr);
        cv.notify_one();
    }

    void worker()
    {
        while (true) {
            shared_ptr<SubInfo> ptr = nullptr;
            {
                unique_lock<mutex> lock(mtx);
                cv.wait(lock, [&](){ return !queue.empty() && !die; });
                if (die) break;
                ptr = queue.back();
                queue.pop_back();
            }
            callback(ptr->keyexpr, ptr->payload, ptr->attributes);
        }
    }

    SubscriberImpl(shared_ptr<SessionApi> session_base, const std::string& expr, SubscriberServerCallback callback, size_t thread_count)
        : expr(expr), callback(callback)
    {
        session = dynamic_pointer_cast<SessionImpl>(session_base);
        handle = std::make_unique<zenohc::Subscriber>(
            zenohc::expect<zenohc::Subscriber>(
                session->session.declare_subscriber(
                    expr,
                    [&](const zenohc::Sample& arg) { this->handler(arg); } )));
        die = false;
        thread_pool.reserve(thread_count);
        for (size_t i = 0; i < 10; i++) {
            thread_pool.emplace_back([&]() { worker(); });
        }
    }

    ~SubscriberImpl()
    {
        {
            std::unique_lock<std::mutex> lock(mtx);
            die = true;
        }
        for (auto& thr : thread_pool) thr.join();
        thread_pool.clear();
    }
};


struct RpcClientImpl : public RpcClientApi {
    shared_ptr<SessionImpl> session;
    string keyexpr;
    string payload;
    string attributes;
    string result_payload;
    string result_attributes;
    
    RpcClientImpl(shared_ptr<SessionApi> session_base, const string& keyexpr, const string& payload, const string& attributes, const std::chrono::seconds&)
        : keyexpr(keyexpr), payload(payload), attributes(attributes)
    {
        session = dynamic_pointer_cast<SessionImpl>(session_base);
        cout << __FILE__ << " creating RPC request for keyexpr=" << keyexpr << " payload=" << payload << " attributes=" << attributes << endl;
    }

    tuple<string, string, string> operator()()
    {
        cout << __FILE__ << " waiting for RPC" << endl;
        return make_tuple(keyexpr, result_payload, result_attributes);
    }
};

struct RpcServerImpl : public RpcServerApi {
    shared_ptr<SessionImpl> session;
    
    RpcServerImpl(shared_ptr<SessionApi> session_base, const std::string& keyexpr, RpcServerCallback callback, size_t thread_count)
    {
        session = dynamic_pointer_cast<SessionImpl>(session_base);
        cout << __FILE__ << " registering RPC callback for " << keyexpr << " thread_count=" << thread_count << endl;
    }
};

Factories factories = {
    [](const auto start_doc) { return make_shared<SessionImpl>(start_doc); },
    [](auto session_base, auto ...args) { return make_shared<PublisherImpl>(session_base, args...); },
    [](auto session_base, auto ...args) { return make_shared<SubscriberImpl>(session_base, args...); },
    [](auto session_base, auto ...args) { return make_shared<RpcClientImpl>(session_base, args...); },
    [](auto session_base, auto ...args) { return make_shared<RpcServerImpl>(session_base, args...); },
};

}; // PluggableTransport

FACTORY_EXPOSE(PluggableTransport::factories)
