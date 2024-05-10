#include <iostream>
#include <sstream>

#include "PluginApi.hpp"

using namespace std;
using namespace PluggableTransport;

int main(int argc, char* argv[])
{
    auto plugin =  make_shared<PluginApi>(argv[1]);
    auto session = Session(plugin, "start_doc");

    auto callback = [](const string& keyexpr, const string& payload, const string& attributes) {
        cout << "in subscriber callback with keyexpr=" << keyexpr << " payload=" << payload << " attributes=" << attributes << endl;
    };

    auto rpc_server_callback = [](const string& keyexpr, const string& payload, const string& attributes) {
        cout << "in rpc_server callback with keyexpr=" << keyexpr << " payload=" << payload << " attributes=" << attributes << endl;
        return make_tuple<string, string>("hello", "world");
    };

    {
        auto subscriber = Subscriber(session, "topic", callback, 4);
        auto publisher = Publisher(session, "topic");

        for (auto i = 0; i < 5; i++) {
            cout << endl << "client code pubishing " << i << endl;
            stringstream ss;
            ss << "payload" << i;
            publisher(ss.str(), "attributes");
        }
    }

    {
        auto rpc_server = RpcServer(session, "rpc_topic", rpc_server_callback, 4);

        for (auto i = 0; i < 5; i++) {
            using namespace std::chrono_literals;

            cout << endl << "rpc client code pubishing " << i << endl;
            stringstream ss;
            ss << "payload" << i;
            auto results = RpcClient(session, "rpc_topic", ss.str(), "attributes", 1s) ();
            cout << "got mock rpc results " << get<0>(results) << ' ' << get<1>(results) << ' ' << get<2>(results) << endl;
        }
    }
}

