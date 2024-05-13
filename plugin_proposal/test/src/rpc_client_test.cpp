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

template <class... Args>
string genString(const char* fmt, Args... args)
{
    char buf[128];
    snprintf(buf, sizeof(buf), fmt, args...);
    return string(buf);
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        cerr << "Must provide path to impl library." << endl;
        exit(-1);
    }

    PluginApi::WhiteList white_list{"6f4e764446ae6c636363448bcfe3e32d"};
    auto plugin =  PluginApi(argv[1]); //, white_list);
    auto session = Session(plugin, "start_doc");

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

