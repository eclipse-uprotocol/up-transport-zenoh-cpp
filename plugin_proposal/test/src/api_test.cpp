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

template <typename KEY>
class Histogram {
    std::mutex mtx;
    std::map<KEY, size_t>   counts;
public:
    void operator()(const KEY& key)
    {
        std::lock_guard<std::mutex> lock(mtx);
        auto it = counts.find(key);
        if (it != counts.end()) it->second++;
        else counts[key] = 1;
    }

    std::string density()
    {
        std::lock_guard<std::mutex> lock(mtx);
        stringstream ss;
        ss << "( ";
        for (const auto& [k,v] : counts) {
            ss << '(' << k << ", " << v << ' ';
        }
        ss << ')';
        return ss.str();
    }
};


int main(int argc, char* argv[])
{
    auto plugin =  make_shared<PluginApi>(argv[1]);
    auto session = Session(plugin, "start_doc");

    auto callback = [](const string& sending_topic, const string& listening_topic, const Message& message) {
        cout << "subscriber callback with from " << sending_topic << " to " << listening_topic << " payload=" << message.payload << " attributes=" << message.attributes << endl;
    };

    {
        auto subscriber = Subscriber(session, "upl/*", callback);
        auto p1 = Publisher(session, "upl/p1");
        auto p2 = Publisher(session, "upl/p2");

        for (auto i = 0; i < 5; i++) {
            cout << endl << "client code pubishing " << i << endl;
            stringstream ss;
            ss << "payload" << i;
            p1(Message{ss.str(), "attributes"});
            p2(Message{ss.str(), "attributes"});
            usleep(100000);
        }
    }

    // {
    //     Histogram<string> histo;
    //     auto rpc_server_callback = [&](const string& keyexpr, const Message& message) {
    //         histo(keyexpr);
    //         cout << "rpc callback with keyexpr=" << keyexpr << " payload=" << message.payload << " attributes=" << message.attributes << endl;
    //         return Message{"hello", "world"};
    //     };

    //     auto rpc_server1 = RpcServer(session, "demo/rpc/action1", rpc_server_callback, 4);
    //     auto rpc_server2 = RpcServer(session, "demo/rpc/action2", rpc_server_callback, 4);
    //     for (auto i = 0; i < 5; i++) {
    //         using namespace std::chrono_literals;

    //         cout << endl << "rpc client code pubishing " << i << endl;
    //         stringstream ss;
    //         ss << "payload" << i;
    //         auto f1 = queryCall(session, "demo/rpc/action1", Message{ss.str(), "attributes"}, 1s);
    //         auto results1 = f1.get();
    //         cout << "rpc results " << get<0>(results1) << ' ' << get<1>(results1).payload << ' ' << get<1>(results1).attributes << endl;
    //         auto f2 = queryCall(session, "demo/rpc/action2", Message{ss.str(), "attributes"}, 1s);
    //         auto results2 = f2.get();
    //         cout << "rpc results " << get<0>(results2) << ' ' << get<1>(results2).payload << ' ' << get<1>(results2).attributes << endl;
    //     }
    //     cout << "counts = " << histo.density() << endl;
    // }


    // {
    //     // auto rpc_server = RpcServer(session, "demo/rpc/action1", rpc_server_callback, 4);

    //     for (auto i = 0; i < 5; i++) {
    //         using namespace std::chrono_literals;

    //         cout << endl << "rpc client code pubishing " << i << endl;
    //         stringstream ss;
    //         ss << "payload" << i;
    //         auto f = queryCall(session, "demo/rpc/action1", Message{ss.str(), "attributes"}, 1s);
    //         auto results = f.get();
    //         cout << "rpc results " << get<0>(results) << ' ' << get<1>(results) << ' ' << get<2>(results) << endl;
    //     }
    // }
}

