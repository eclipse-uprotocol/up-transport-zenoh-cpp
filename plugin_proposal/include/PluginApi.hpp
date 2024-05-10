#pragma once

#include "FactoryPlugin.hpp"
#include <chrono>
#include <optional>
#include <future>

namespace PluggableTransport {
//
// This is the base type for the common session object.
//
struct SessionApi {
   virtual ~SessionApi() {}
};

//
// This is a type alias for subscriber server processing.
//
struct Message {
   std::string payload;
   std::string attributes;
};

using SubscriberServerCallback = std::function<void (const std::string&, const Message&)>;

//
// This is a type alias for RPC server processing.
//
using RpcServerCallback = std::function<std::optional<std::tuple<std::string, std::string>> (const std::string&, const std::string&, const std::string&)>;

//
// This makes a type alias for the getter lambdas implemented in the implementation module.
//
template <typename CLS, class... Args>
using FactoryGetter = std::function< std::shared_ptr<CLS> (std::shared_ptr<SessionApi>, Args...) >;


//
// These are the base types for the concepts provided by the implementation plugin.
// The type alias at the top of each is the signature of the factory function for the implementation.
//
struct PublisherApi {
   using FactoryParams = FactoryGetter<PublisherApi, const std::string&>;
   virtual void operator()(const Message&) = 0;
};

struct SubscriberApi {
   using FactoryParams = FactoryGetter<SubscriberApi, const std::string&, SubscriberServerCallback, size_t>;
};

struct RpcClientApi {
   using FactoryParams = FactoryGetter<RpcClientApi, const std::string&, const Message&, const std::chrono::seconds&>;
   virtual std::tuple<std::string, std::string, std::string> operator()() = 0;
};

struct RpcServerApi {
   using FactoryParams = FactoryGetter<RpcServerApi, const std::string&, RpcServerCallback, size_t>;
};

//
// The next declaration is the factory struct that the implementation will return at load time.
//

struct Factories {
   std::function< std::shared_ptr<SessionApi> (const std::string&)> get_session;
   PublisherApi::FactoryParams get_publisher;
   SubscriberApi::FactoryParams get_subscriber;
   RpcClientApi::FactoryParams get_rpc_client;
   RpcServerApi::FactoryParams get_rpc_server;
};

using PluginApi = FactoryPlugin<Factories>;

//
// Below this point are the thin wrapper classes that hold the pImpls pointing back to the dll implementation
//
class Session {
   std::shared_ptr<PluginApi> plugin;
   std::shared_ptr<SessionApi> pImpl;
public:
   friend class Publisher;
   friend class Subscriber;
   friend class RpcClient;
   friend class RpcServer;

   Session(std::shared_ptr<PluginApi> plugin, const std::string& start_doc)
      : plugin(plugin), pImpl((*plugin)->get_session(start_doc)) {}

   // std::future<std::tuple<std::string, std::string, std::string>> queryCall(std::string expr, const std::string& payload, const std::string& attributes, const std::chrono::seconds& timeout);
};


class Publisher {
   std::shared_ptr<PublisherApi> pImpl;
public:
   template <class... Args>
   Publisher(Session session, Args... args)
      : pImpl((*session.plugin)->get_publisher(session.pImpl, args...)) {}

   void operator()(const Message& message) { (*pImpl)(message); }
};

class Subscriber {
   std::shared_ptr<SubscriberApi> pImpl;
public:
   template <class... Args>
   Subscriber(Session session, Args... args)
      : pImpl((*session.plugin)->get_subscriber(session.pImpl, args...)) {}
};

class RpcClient {
   std::shared_ptr<RpcClientApi> pImpl;
public:
   RpcClient(Session session, const std::string& topic, const Message& message, const std::chrono::seconds& timeout)
      : pImpl((*session.plugin)->get_rpc_client(session.pImpl, topic, message, timeout)) {}

   std::tuple<std::string, std::string, std::string> operator()() { return (*pImpl)(); }
};

std::future<std::tuple<std::string, std::string, std::string>> queryCall(Session s, std::string expr, const Message& message, const std::chrono::seconds& timeout)
{
    auto msg = std::make_shared<Message>(message);
    return std::async([=]() { return RpcClient(s, expr, *msg, timeout)(); } );
}

class RpcServer {
   std::shared_ptr<RpcServerApi> pImpl;
public:
   template <class... Args>
   RpcServer(Session session, Args... args)
      : pImpl((*session.plugin)->get_rpc_server(session.pImpl, args...)) {}

};

}; // PluggableTransport