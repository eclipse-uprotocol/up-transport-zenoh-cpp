#pragma once

#include <stdexcept>
#include <string>
#include <memory>
#include <sstream>
#include <functional>
#include <set>
#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <openssl/evp.h>

template <typename FACT>
class FactoryPlugin {
    void* dl_handle;
    std::string path;

    void throw_error(const std::string& desc)
    {
        using namespace std;
        stringstream ss;
        ss << "Cannot " << desc << " dll file \"" << path << '"';
        throw runtime_error(ss.str());
    }

    std::string compute_md5()
    {
        using namespace std;

        int fd = ::open(path.c_str(), O_RDONLY);
        if (fd < 0) throw_error("open");
        struct stat sbuf;
        ::fstat(fd, &sbuf);
        void* ptr = mmap(0, sbuf.st_size, PROT_READ, MAP_SHARED, fd, 0);
        if (ptr == nullptr) {
            close(fd);
            throw_error("mmap");
        }
        close(fd);

        const size_t digest_len = 16;
        unsigned char digest[digest_len];
        EVP_Q_digest(NULL, "MD5", NULL, ptr, sbuf.st_size, digest, NULL);
        munmap(ptr, sbuf.st_size);

        std::string        output;
        output.resize(digest_len * 2);
        for (unsigned int i = 0 ; i < digest_len ; ++i)
            std::sprintf(&output[i * 2], "%02x", digest[i]);
        return output;
    }
public:
    FACT* factory;

    using WhiteList = std::set<std::string>;
    FactoryPlugin(const std::string& path, const WhiteList& white_list = WhiteList()) : path(path), dl_handle(nullptr)
    {
        auto hash = compute_md5();
        if (white_list.size() > 0) {
            if (white_list.count(hash) == 0) {
                using namespace std;
                stringstream ss;
                ss << "MD5 hash=" << hash << " for  file \"" << path << "\" is not in white list";
                throw runtime_error(ss.str());
            }
        }
        else {
            using namespace std;
            cerr << "MD5 hash=" << hash << " for \"" << path << "\" was not used because there is no whitelist";
        }
        dl_handle = dlopen(path.c_str(), RTLD_NOW|RTLD_LOCAL);
        if (dl_handle == nullptr) throw_error("dlopen");
        void* ptr = dlsym(dl_handle, "get_factory__");
        if (ptr == nullptr) {
            dlclose(dl_handle);
            throw_error("locate get_factory__ symbol in ");
        }
        auto get_factory = (void* (*)())ptr;
        factory = (FACT*) (*get_factory)();
    }

    ~FactoryPlugin()
    {
        if (dl_handle != nullptr) {
            if (dlclose(dl_handle) != 0) throw_error("dlclose");
        }
    }

    FACT* operator->() { return factory; }
};

#define _EXPOSE extern "C" __attribute__((visibility("default")))
#define FACTORY_EXPOSE(fact) _EXPOSE void * get_factory__() { return (void*) &fact; }