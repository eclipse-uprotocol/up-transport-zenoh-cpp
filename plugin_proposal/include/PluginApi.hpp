#pragma once

#include "FactoryPlugin.hpp"
#include <chrono>
#include <optional>

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
using SubscriberServerCallback = std::function<void (const std::string&, const std::string&, const std::string&)>;

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
   virtual void operator()(const std::string& payload, const std::string attributes) = 0;
};

struct SubscriberApi {
   using FactoryParams = FactoryGetter<SubscriberApi, const std::string&, SubscriberServerCallback, size_t>;
};

struct RpcClientApi {
   using FactoryParams = FactoryGetter<RpcClientApi, const std::string&, const std::string&, const std::string&, const std::chrono::seconds&>;
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
   Session(std::shared_ptr<PluginApi> plugin, const std::string& start_doc)
      : plugin(plugin), pImpl((*plugin)->get_session(start_doc)) {}

   friend class Publisher;
   friend class Subscriber;
   friend class RpcClient;
   friend class RpcServer;
};


class Publisher {
   std::shared_ptr<PublisherApi> pImpl;
public:
   template <class... Args>
   Publisher(Session session, Args... args)
      : pImpl((*session.plugin)->get_publisher(session.pImpl, args...)) {}

   template <class... Args>
   void operator()(Args... args) { (*pImpl)(args...); }
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
   template <class... Args>
   RpcClient(Session session, Args... args)
      : pImpl((*session.plugin)->get_rpc_client(session.pImpl, args...)) {}

   std::tuple<std::string, std::string, std::string> operator()() { return (*pImpl)(); }
};

class RpcServer {
   std::shared_ptr<RpcServerApi> pImpl;
public:
   template <class... Args>
   RpcServer(Session session, Args... args)
      : pImpl((*session.plugin)->get_rpc_server(session.pImpl, args...)) {}

};

}; // PluggableTransport