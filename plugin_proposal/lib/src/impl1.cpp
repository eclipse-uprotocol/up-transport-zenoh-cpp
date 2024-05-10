#include <iostream>
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
        unique_lock<mutex> lock(mtx);
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

static std::string keyexpr2string(const z_keyexpr_t& keyexpr)
{
    z_owned_str_t keystr = z_keyexpr_to_string(keyexpr);
    std::string ret(z_loan(keystr));
    z_drop(z_move(keystr));
    return ret;    
}

static string extract(const z_bytes_t& b)
{
    auto ptr = (const char*)b.start;
    return string(ptr, ptr+b.len);
}

static z_bytes_t pack(const string_view& od)
{
    return z_bytes_t{.len = od.size(), .start = (uint8_t*)od.data()};
}

struct RpcClientImpl : public RpcClientApi {
    shared_ptr<SessionImpl> session;
    z_owned_reply_channel_t channel;
    
    RpcClientImpl(shared_ptr<SessionApi> session_base, const string& expr, const string& payload, const string& attributes, const std::chrono::seconds& timeout)
    {
        session = dynamic_pointer_cast<SessionImpl>(session_base);
        cout << __FILE__ << " creating RPC request for keyexpr=" << expr << " payload=" << payload << " attributes=" << attributes << endl;
        z_keyexpr_t keyexpr = z_keyexpr(expr.c_str());
        if (!z_check(keyexpr)) throw std::runtime_error("Not a valid key expression");
        channel = zc_reply_fifo_new(16);
        auto opts = z_get_options_default();
        auto attrs = z_bytes_map_new();
        opts.value.payload = pack(payload);
        z_bytes_map_insert_by_alias(&attrs, z_bytes_new("attributes"), pack(attributes));
        opts.attachment = z_bytes_map_as_attachment(&attrs);
        opts.timeout_ms = chrono::milliseconds(timeout).count();
        z_get(session->session.loan(), keyexpr, "", z_move(channel.send), &opts);
        cout << "after z_get" << endl;
    }

    ~RpcClientImpl()
    {
        cout << "before drop channel" << endl;
        z_drop(z_move(channel));
    }

    tuple<string, string, string> operator()()
    {
        std::string src;
        string payload, attributes;
        z_owned_reply_t reply = z_reply_null();

        cout << "before z_call" << endl;
        for (z_call(channel.recv, &reply); z_check(reply); z_call(channel.recv, &reply)) {
            cout << "after z_call" << endl;
            if (z_reply_is_ok(&reply)) {
                cout << "reply is okay" << endl;
                z_sample_t sample = z_reply_ok(&reply);
                cout << "got sample" << endl;
                src = keyexpr2string(sample.keyexpr);
                cout << "src=" << src << endl;
                payload = extract(sample.payload);
                cout << "payload=" << payload << endl;
                z_bytes_t attr = z_attachment_get(sample.attachment, z_bytes_new("attributes"));
                attributes = extract(attr);
                cout << "attributes=" << attributes << endl;
                break;
            } else {
                cerr << "z_reply_is_okay returned false" << endl;
                // throw std::runtime_error("Received an error");
            }
        }

        z_drop(z_move(reply));
        return std::make_tuple(src, payload, attributes);
    }
};

struct RpcInfo {
    string  keyexpr;
    string  payload;
    string  attributes;
    z_owned_query_t owned_query;

    RpcInfo(const z_query_t *query)
    {
        keyexpr = keyexpr2string(z_query_keyexpr(query));
        // z_bytes_t pred = z_query_parameters(query);
        z_value_t value = z_query_value(query);
        payload = extract(value.payload);

        z_attachment_t attachment = z_query_attachment(query);
        if (!z_check(attachment)) throw std::runtime_error("attachment is missing");
        z_bytes_t avalue = z_attachment_get(attachment, z_bytes_new("attributes"));
        attributes = extract(avalue);
        owned_query = z_query_clone(query);
    }

    ~RpcInfo()
    {
        z_query_drop(&owned_query);
    }
};

struct RpcServerImpl : public RpcServerApi {
    shared_ptr<SessionImpl> session;
    z_owned_queryable_t qable;
    mutex  mtx;
    condition_variable cv;
    deque<shared_ptr<RpcInfo>> queue;
    bool die;
    RpcServerCallback callback;
    vector<thread> thread_pool;

    static void _handler(const z_query_t *query, void *context)
    {
        reinterpret_cast<RpcServerImpl*>(context)->handler(query);
    }

    void handler(const z_query_t *query)
    {
        cout << "rpc handler woke" << endl;
        auto ptr = make_shared<RpcInfo>(query);
        unique_lock<mutex> lock(mtx);
        queue.push_front(ptr);
        cv.notify_one();
    }

    void worker()
    {
        while (true) {
            shared_ptr<RpcInfo> ptr;
            {
                unique_lock<mutex> lock(mtx);
                cv.wait(lock, [&](){ return !queue.empty() && !die; });
                if (die) break;
                ptr = queue.back();
                queue.pop_back();
            }
            auto results = callback(ptr->keyexpr, ptr->payload, ptr->attributes);
            if (results) {
                cout << "sending results" << endl;
                auto payload = get<0>(*results);
                auto attributes = get<1>(*results);
                auto query = z_query_loan(&ptr->owned_query);
                z_query_reply_options_t options = z_query_reply_options_default();
                z_owned_bytes_map_t map = z_bytes_map_new();
                z_bytes_map_insert_by_alias(&map, z_bytes_new("attributes"), pack(attributes));
                options.attachment = z_bytes_map_as_attachment(&map);
                z_query_reply(&query, z_keyexpr(ptr->keyexpr.c_str()), (const uint8_t*)payload.data(), payload.size(), &options);                
            }
            else {
                cout << "no results to send" << endl;
            }
        }
    }

    RpcServerImpl(shared_ptr<SessionApi> session_base, const std::string& keyexpr, RpcServerCallback callback, size_t thread_count) : callback(callback)
    {
        session = dynamic_pointer_cast<SessionImpl>(session_base);
        cout << "registering RPC callback for " << keyexpr << " thread_count=" << thread_count << endl;

        z_owned_closure_query_t closure = z_closure(_handler, NULL, this);
        qable = z_declare_queryable(session->session.loan(), z_keyexpr(keyexpr.c_str()), z_move(closure), NULL);
        if (!z_check(qable)) throw std::runtime_error("Unable to create queryable.");

        die = false;
        thread_pool.reserve(thread_count);
        for (size_t i = 0; i < 10; i++) {
            thread_pool.emplace_back([&]() { worker(); });
        }
    }

    ~RpcServerImpl()
    {
        cout << "rpc server shutting down" << endl;
        {
            unique_lock<mutex> lock(mtx);
            die = true;
        }
        for (auto& thr : thread_pool) thr.join();
        thread_pool.clear();

        z_undeclare_queryable(z_move(qable));
        cout << "bottom of shutdown" << endl;       
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
