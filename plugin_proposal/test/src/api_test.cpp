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

template <class... Args>
string genString(const char* fmt, Args... args)
{
    char buf[128];
    snprintf(buf, sizeof(buf), fmt, args...);
    return string(buf);
}

void test(const string& dll_path)
{
    PluginApi::WhiteList white_list{"6f4e764446ae6c636363448bcfe3e32d"};
    auto plugin =  PluginApi(dll_path); //, white_list);
    auto session = Session(plugin, "start_doc");

    {
        auto callback = [](const string& sending_topic, const string& listening_topic, const Message& message) {
            cout << "subscriber callback with"
                << " from=" << sending_topic
                << " to=" << listening_topic
                << " payload=" << message.payload
                << " attributes=" << message.attributes << endl;
        };
        auto subscriber = Subscriber(session, "upl/*", callback, 4, "sub");
        auto p1 = Publisher(session, "upl/p1", "p1");
        auto p2 = Publisher(session, "upl/p2", "p2");

        for (auto i = 0; i < 5; i++) {
            cout << endl << "client code pubishing " << i << endl;
            p1(Message{genString("pay_A_%d", i), genString("attr_A_%d", i)});
            p2(Message{genString("pay_B_%d", i), genString("attr_B_%d", i)});
            usleep(100000);
        }
    }
    cout << "################### rpc test" << endl;
    {
        auto rpc_server_callback = [&](const string& sending_topic, const string& listening_topic, const Message& message) {
            cout << "rpc callback with"
                << " from=" << sending_topic
                << " to=" << listening_topic
                << " payload=" << message.payload
                << " attributes=" << message.attributes << endl;
            return Message{"hello", "world"};
        };
        auto rpc_server = RpcServer(session, "demo/rpc/*", rpc_server_callback, 4, "rpcserv");

        for (auto i = 0; i < 5; i++) {
            using namespace std::chrono_literals;
            auto f1 = queryCall(session, "demo/rpc/action1", Message{genString("pay_A_%d", i), genString("attr_A_%d", i)}, 1s);
            auto f2 = queryCall(session, "demo/rpc/action2", Message{genString("pay_B_%d", i), genString("attr_B_%d", i)}, 1s);
            auto results1 = f1.get();
            auto results2 = f2.get();
            cout << "rpc results " << get<0>(results1) << ' ' << get<1>(results1).payload << ' ' << get<1>(results1).attributes << endl;
            cout << "rpc results " << get<0>(results2) << ' ' << get<1>(results2).payload << ' ' << get<1>(results2).attributes << endl;
            usleep(100000);
        }     
    }
}

int main(int argc, char* argv[])
{
    test(argv[1]);
    // Oh, by the way, dlclose() will not unload or invoke dtors if there are 'UNIQUE' tagged objects in plugin.
    // Palm to forehead.
    sleep(5);
    test(argv[1]);
}

