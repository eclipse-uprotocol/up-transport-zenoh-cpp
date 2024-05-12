#include "DllHandle.hpp"
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <link.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <openssl/evp.h>

using namespace std;

struct DllHandle::Impl {
    void* dl_handle;
    std::string path;

    void throw_error(const std::string& desc, const std::string& fpath)
    {
        using namespace std;
        stringstream ss;
        ss << "Cannot " << desc << " dll file \"" << fpath << '"';
        throw runtime_error(ss.str());
    }

    std::string compute_md5(const std::string& fpath)
    {
        using namespace std;

        int fd = ::open(fpath.c_str(), O_RDONLY);
        if (fd < 0) throw_error("open", fpath);
        struct stat sbuf;
        ::fstat(fd, &sbuf);
        void* ptr = mmap(0, sbuf.st_size, PROT_READ, MAP_SHARED, fd, 0);
        if (ptr == nullptr) {
            close(fd);
            throw_error("mmap", fpath);
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

    Impl(const std::string& path, const WhiteList& white_list = WhiteList()) : path(path), dl_handle(nullptr)
    {
        using namespace std;
        auto hash = compute_md5(path);
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
            cerr << "MD5 hash=" << hash << " for \"" << path << "\" was found but whitelist is missing." << endl;
        }
        dl_handle = dlopen(path.c_str(), RTLD_NOW|RTLD_LOCAL);
        if (dl_handle == nullptr) throw_error("dlopen", path);


        struct ::link_map *lmp;
        if (dlinfo(dl_handle, RTLD_DI_LINKMAP, &lmp) != 0) throw_error("dlinfo(RTLD_DI_LINKMAP)", path);
        for ( ; lmp; lmp = lmp->l_next) {
            using namespace std;
            cout << "MD5: " << compute_md5(lmp->l_name)  << ' ' << lmp->l_name  << endl;
        }
    }

    ~Impl()
    {
        // apparently, dlclose with actually not unload or even run dtors if there are 'UNIQUE' tagged objects
        // So, don't depend on dtors in plugin code for back-to-back clean testing.
        if (dl_handle != nullptr) {
            if (dlclose(dl_handle) != 0) throw_error("dlclose", path);
        }
    }

    void *getSymbol(const std::string& symbol)
    {
        void* ptr = dlsym(dl_handle, "get_factory__");
        if (ptr == nullptr) {
            dlclose(dl_handle);
            throw_error("locate get_factory__ symbol in ", path);
        }
        return ptr; 
    }
};

DllHandle::DllHandle(const std::string& path, const WhiteList& white_list)
    : pImpl(new Impl(path, white_list)) {}

void* DllHandle::getSymbol(const std::string& symbol)
{
    return pImpl->getSymbol(symbol);
}