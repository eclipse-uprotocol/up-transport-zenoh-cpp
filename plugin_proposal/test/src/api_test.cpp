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

// future<tuple<string, string, string>> queryCall(Session s, std::string expr, const string& payload, const string& attributes, const chrono::seconds& timeout)
// {
//     auto p = make_shared<string>(payload);
//     auto a = make_shared<string>(attributes);
//     return std::async([=]() { return RpcClient(s, expr, *p, *a, timeout)(); } );
// }

int main(int argc, char* argv[])
{
    auto plugin =  make_shared<PluginApi>(argv[1]);
    auto session = Session(plugin, "start_doc");

    auto callback = [](const string& keyexpr, const string& payload, const string& attributes) {
        cout << "subscriber callback with keyexpr=" << keyexpr << " payload=" << payload << " attributes=" << attributes << endl;
    };

    // {
    //     auto subscriber = Subscriber(session, "upl/string_test", callback, 4);
    //     auto publisher = Publisher(session, "upl/string_test");

    //     for (auto i = 0; i < 5; i++) {
    //         cout << endl << "client code pubishing " << i << endl;
    //         stringstream ss;
    //         ss << "payload" << i;
    //         publisher(ss.str(), "attributes");
    //         sleep(1);
    //     }
    // }

    // {
    //     Histogram<string> histo;
    //     auto rpc_server_callback = [&](const string& keyexpr, const string& payload, const string& attributes) {
    //         histo(keyexpr);
    //         cout << "rpc callback with keyexpr=" << keyexpr << " payload=" << payload << " attributes=" << attributes << endl;
    //         return make_tuple<string, string>("hello", "world");
    //     };

    //     auto rpc_server1 = RpcServer(session, "demo/rpc/action1", rpc_server_callback, 4);
    //     auto rpc_server2 = RpcServer(session, "demo/rpc/action2", rpc_server_callback, 4);
    //     sleep(10);
    //     cout << "counts = " << histo.density() << endl;
    // }


    {
        // auto rpc_server = RpcServer(session, "demo/rpc/action1", rpc_server_callback, 4);

        for (auto i = 0; i < 5; i++) {
            using namespace std::chrono_literals;

            cout << endl << "rpc client code pubishing " << i << endl;
            stringstream ss;
            ss << "payload" << i;
            auto f = queryCall(session, "demo/rpc/action1", ss.str(), "attributes", 1s);
            auto results = f.get();
            cout << "rpc results " << get<0>(results) << ' ' << get<1>(results) << ' ' << get<2>(results) << endl;
        }
    }
}

