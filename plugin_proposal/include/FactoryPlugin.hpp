#pragma once

#include "DllHandle.hpp"


// This is the name of the only symbol to look up in the DLL
#define FACTORY_GETTER_NAME get_factory__
#define STRINGIFY(s) #s

//
// Class definition is only needed in client side.
//
template <typename FACT>
class FactoryPlugin {
    std::shared_ptr<DllHandle>  dll_handle;
    FACT* factory;
public:
    using WhiteList = DllHandle::WhiteList;
    FactoryPlugin(const std::string& path, const WhiteList& white_list = WhiteList())
    {
        dll_handle = std::make_shared<DllHandle>(path);
        auto get_factory = (void* (*)()) dll_handle->getSymbol(STRINGIFY(FACTORY_GETTER_NAME));
        factory = (FACT*) (*get_factory)();
    }
    FACT* operator->() { return factory; }
};

//
// Macros are only needed in library side to expose the 
//
#define _EXPOSE extern "C" __attribute__((visibility("default")))
#define FACTORY_EXPOSE(fact) _EXPOSE void * FACTORY_GETTER_NAME() { return (void*) &fact; }