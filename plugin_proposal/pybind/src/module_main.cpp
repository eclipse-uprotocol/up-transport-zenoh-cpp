#include <iostream>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "PluginApi.hpp"

namespace py = pybind11;
using namespace std;
using namespace PluggableTransport;

PYBIND11_MODULE(pyPluginApi, m)
{
    cout << "hello world" << endl;

    py::class_<PluginApi>(m, "PluginApi")
        .def(py::init<string>())
        ;

    py::class_<Session>(m, "Session")
        .def(py::init<PluginApi, string, string>())
        ;

    py::class_<Publisher>(m, "Publisher")
        .def(py::init<Session, string, string>())
        .def("__call__", [](Publisher me, const string& p, const string& a) { me(Message{p, a}); })
        ;

    //
    // Due to concurrency and the GIL
    //
}