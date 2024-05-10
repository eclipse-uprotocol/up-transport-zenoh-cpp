#include <iostream>
#include <vector>
#include <map>
#include "PluginApi.hpp"

using namespace std;

namespace PluggableTransport {

class SubscriberImpl;

struct SessionImpl : public SessionApi {
    string start_doc;
    map<string, SubscriberServerCallback> sub_handlers;
    map<string, RpcServerCallback> rpc_handlers;

    SessionImpl(const string& start_doc) : start_doc(start_doc)
    {
        cout << __FILE__ << " SessionImpl with " << start_doc << " in " << __FILE__ << endl;
    }
};

struct PublisherImpl : public PublisherApi {
    shared_ptr<SessionImpl> session;
    string keyexpr;
    
    PublisherImpl(shared_ptr<SessionApi> session_base, const string& keyexpr) : keyexpr(keyexpr)
    {
        session = dynamic_pointer_cast<SessionImpl>(session_base);
        cout << __FILE__ << " creating publisher for " << keyexpr << endl;
    }

    void operator()(const std::string& payload, const std::string attributes)
    {
        cout << __FILE__ << " publishing payload=" << payload << " attributes=" << attributes << " to " << keyexpr << endl;
        auto it = session->sub_handlers.find(keyexpr);
        if (it != session->sub_handlers.end())
            it->second(keyexpr, payload, attributes);
    }
};

struct SubscriberImpl : public SubscriberApi {
    shared_ptr<SessionImpl> session;
    
    SubscriberImpl(shared_ptr<SessionApi> session_base, const std::string& keyexpr, SubscriberServerCallback callback, size_t thread_count)
    {
        session = dynamic_pointer_cast<SessionImpl>(session_base);
        cout << __FILE__ << " subscriber registering callback for " << keyexpr << " thread_count=" << thread_count << endl;
        session->sub_handlers[keyexpr] = callback;
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
        auto it = session->rpc_handlers.find(keyexpr);
        if (it != session->rpc_handlers.end()) {
            auto result = it->second(keyexpr, payload, attributes);
            if (result) {
                result_payload = get<0>(*result);
                result_attributes = get<1>(*result);
            }
        }

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
        session->rpc_handlers[keyexpr] = callback;
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
