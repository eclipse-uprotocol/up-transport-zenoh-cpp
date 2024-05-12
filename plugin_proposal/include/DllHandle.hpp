#pragma once

#include <string>
#include <memory>
#include <set>

class DllHandle {
public:
    using WhiteList = std::set<std::string>;
    DllHandle(const std::string& path, const WhiteList& white_list = WhiteList());
    void* getSymbol(const std::string&);
private:
    struct Impl;
    std::shared_ptr<Impl>    pImpl;
};
