// #include "SubProc.h"
#include <iostream>
#include <vector>
#include <sstream>
#include <boost/interprocess/managed_mapped_file.hpp>

namespace ipc = boost::interprocess;

using namespace std;

// void job(SharedVar<size_t>& up, SharedVar<size_t>& down, int arg)
// {
//     cout << "child " << getpid() << endl;
//     while (true) {
//         auto [ down_cntr, off ] = down.wait();
//         // cout << "down.wait " << getpid() << endl;
//         auto up_cntr = up.post(arg + off + 10);
//         // cout << "child " << arg << ':' << getpid() << " dwn=" << down_cntr << " up=" << up_cntr << " off=" << off << endl;
//     }
// }

int main(int argc, char *argv[])
{
    ipc::managed_mapped_file* mfile;
    try {
        stringstream ss;
        ss << "/tmp/tst_" << getpid();
        mfile = new ipc::managed_mapped_file(ipc::create_only, ss.str().c_str(), 65536);
        cout << "after mfile" << endl;
        int* ptr = mfile->construct<int>("counter")();
        *ptr = 5;
    }
    catch (const std::exception& ex) {
        cerr << "Threw " << ex.what() << endl;
        exit(-1);
    }

    if (fork() == 0) {
        auto [ ptr, sz ] = mfile->find<int>("counter");
        cout << "child got " << *ptr << endl;
    }
    // cout << "parent is " << getpid() << endl;
    // vector<shared_ptr<SubProc<size_t, size_t>>> procs;
    // const size_t cnt = 1;
    // for (size_t i = 0; i < cnt; i++) procs.push_back(make_shared<SubProc<size_t, size_t>>());

    // for (size_t i = 0; i < cnt; i++) procs[i]->run(job, i);
    // cout << "launched" << endl;
    // sleep(1);

    // for (size_t j = 0; j < 10; j++) {
    //     usleep(10000);
    //     cout << endl << endl;
    //     for(auto child : procs) child->post(j);
    //     // cout << "after post" << endl;
    //     auto abs_time = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
    //     for (size_t i = 0; i < cnt; i++) {
    //         auto [ up_cntr, up_data ] = procs[i]->wait(abs_time);
    //         cout << "top side " << i << ' ' << up_cntr << ' ' << up_data << endl;
    //     }
    // }

    // return 0;
}