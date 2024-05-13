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

    auto p1 = Publisher(session, "upl/p1", "p1");
    auto p2 = Publisher(session, "upl/p2", "p2");

    for (auto i = 0; i < 5; i++) {
        cout << endl << "client code pubishing " << i << endl;
        p1(Message{genString("pay_A_%d", i), genString("attr_A_%d", i)});
        p2(Message{genString("pay_B_%d", i), genString("attr_B_%d", i)});
        usleep(100000);
    }
}

