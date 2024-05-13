#include <iostream>
#include <sstream>
#include <future>
#include <string_view>
#include <chrono>
#include <map>
#include <unistd.h>

#include "PluginApi.hpp"

using namespace std;
using namespace PluggableTransport;

int main(int argc, char* argv[])
{
    if (argc < 2) {
        cerr << "Must provide path to impl library." << endl;
        exit(-1);
    }

    PluginApi::WhiteList white_list{"6f4e764446ae6c636363448bcfe3e32d"};
    auto plugin =  PluginApi(argv[1]); //, white_list);
    auto session = Session(plugin, "start_doc");

    auto rpc_server_callback = [&](const string& sending_topic, const string& listening_topic, const Message& message) {
        cout << "rpc callback with"
            << " from=" << sending_topic
            << " to=" << listening_topic
            << " payload=" << message.payload
            << " attributes=" << message.attributes << endl;
        return Message{"hello", "world"};
    };
    auto rpc_server = RpcServer(session, "demo/rpc/*", rpc_server_callback, 4, "rpcserv");
    sleep(10000);
}

