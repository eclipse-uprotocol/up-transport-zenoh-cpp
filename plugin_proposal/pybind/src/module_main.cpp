#include <iostream>
#include <sstream>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>
#include <pybind11/chrono.h>

#include "PluginApi.hpp"

namespace py = pybind11;
using namespace std;
using namespace PluggableTransport;

// struct RpcClientFuture {
//     std::future<std::tuple<std::string, Message>> f;

//     RpcClientFuture(Session ses, const string& topic, const Message& msg, const chrono::seconds& timeout) : f(std::move((ses, topic, msg, timeout))) {}
//     auto get() { return f.get(); }
// };

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

     py::class_<Message>(m, "Message")
        .def(py::init<string, string>())
        .def_readwrite("payload", &Message::payload)
        .def_readwrite("attributes", &Message::attributes)
        .def("__repr__", [](Message me) { stringstream ss; ss << "Message{" << me.payload << ", " << me.attributes << '}'; return ss.str(); })
        ;

     py::class_<Subscriber>(m, "Subscriber")
        .def(py::init<Session, string, SubscriberServerCallback, size_t, string>())
        ;

    //  py::class_<RpcClientFuture>(m, "RpcClientFuture")
    //     .def(py::init<Session, string, Message, chrono::seconds>())
    //     .def("get", [](RpcClientFuture me) { return me.get(); })
    //     ;

     py::class_<RpcServer>(m, "RpcServer")
        .def(py::init<Session, string, RpcServerCallback, size_t, string>())
        ;
}