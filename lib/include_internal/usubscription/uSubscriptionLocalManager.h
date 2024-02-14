
#ifndef _H_USUBSCRIPTION_LOCAL_MANAGER_
#define _H_USUBSCRIPTION_LOCAL_MANAGER_

#include <unordered_map>
#include <ustatus.pb.h>
#include <core/usubscription/v3/usubscription.pb.h>

using namespace std;
using namespace uprotocol::core::usubscription::v3;
using namespace uprotocol::v1;

class SubscriptionLocalManager {

    public:

        SubscriptionLocalManager(const SubscriptionLocalManager&) = delete;
         
        SubscriptionLocalManager& operator=(const SubscriptionLocalManager&) = delete;

        static SubscriptionLocalManager& instance() noexcept;
               
        UStatus init();

        UStatus term();
       
        template<typename T>
        UStatus setStatus(UUri uri, 
                          T status);

        SubscriptionStatus_State getSubscriptionStatus(const UUri &uri);

        UCode getPublisherStatus(const UUri &uri);
        
    private:
    
        SubscriptionLocalManager() {};

        unordered_map<std::string, UCode> pubStatusMap_;
        unordered_map<std::string, SubscriptionStatus_State> subStatusMap_;
};

#endif //_H_USUBSCRIPTION_LOCAL_MANAGER_