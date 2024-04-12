#include "SubProc.h"
#include <iostream>
#include <vector>
#include <sstream>

using namespace std;

void job(SharedVar<size_t>& up, SharedVar<size_t>& down, int arg)
{
    try {
        while (true) {
            auto [ down_cntr, off ] = down.wait();
            auto up_cntr = up.post(arg + off + 10);
        }
    }
    catch(const std::exception& ex) {
        // cerr << "child threw exception=" << ex.what() << endl;
    }
    exit(0);
}


int main(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    {
        vector<shared_ptr<SubProc<size_t, size_t>>> procs;
        const size_t cnt = 4;
        for (size_t i = 0; i < cnt; i++) procs.push_back(make_shared<SubProc<size_t, size_t>>(true));

        for (size_t i = 0; i < cnt; i++) procs[i]->run(job, i);
        cout << "launched" << endl;
        sleep(1);

        try {
            for (size_t j = 0; j < 3; j++) {
                usleep(10000);
                cout << endl << endl;
                for(auto child : procs) child->post(j);
                auto abs_time = std::chrono::steady_clock::now() + std::chrono::milliseconds(1000);
                for (size_t i = 0; i < cnt; i++) {
                    auto [ up_cntr, up_data ] = procs[i]->wait(abs_time);
                    cout << "parent woke i=" << i << " up_cntr=" << up_cntr << " up_data=" << up_data << endl;
                }
            }
            cout << "finished outer loop" << endl;
        }
        catch (const std::exception& ex) {
            cerr << "parent threw exception = <<" << ex.what() << ">>" << endl;
        }

        procs.clear();
        cout << "after procs.clear()" << endl;
    }
    cout << "############################" << endl;
    {
        vector<shared_ptr<SubProc<size_t, size_t>>> procs;
        const size_t cnt = 4;
        for (size_t i = 0; i < cnt; i++) procs.push_back(make_shared<SubProc<size_t, size_t>>(false));

        for (size_t i = 0; i < cnt; i++) procs[i]->run(job, i);
        cout << "launched" << endl;
        sleep(1);

        try {
            for (size_t j = 0; j < 3; j++) {
                usleep(10000);
                cout << endl << endl;
                for(auto child : procs) child->post(j);
                auto abs_time = std::chrono::steady_clock::now() + std::chrono::milliseconds(1000);
                for (size_t i = 0; i < cnt; i++) {
                    auto [ up_cntr, up_data ] = procs[i]->wait(abs_time);
                    cout << "parent woke i=" << i << " up_cntr=" << up_cntr << " up_data=" << up_data << endl;
                }
            }
            cout << "finished outer loop" << endl;
        }
        catch (const std::exception& ex) {
            cerr << "parent threw exception = <<" << ex.what() << ">>" << endl;
        }

        procs.clear();
        cout << "after procs.clear()" << endl;
    }

    return 0;
}