#include <up-client-zenoh-cpp/rpc/zenohRpcClient.h>
#include <up-client-zenoh-cpp/usubscription/uSubscriptionClient.h>
#include <up-client-zenoh-cpp/usubscription/common/uSubscriptionCommon.h>
#include <up-cpp/uuid/serializer/UuidSerializer.h>
#include <up-cpp/uuid/factory/Uuidv8Factory.h>
#include <core/usubscription/v3/usubscription.pb.h>
#include <google/protobuf/message.h>
#include <ustatus.pb.h>

using namespace std;
using namespace uprotocol::utransport;
using namespace uprotocol::uri;
using namespace uprotocol::uuid;
using namespace uprotocol::uSubscription;

class SubscriptionStatusLocal {

    public:

        SubscriptionStatusLocal(const SubscriptionStatusLocal&) = delete;
         
        SubscriptionStatusLocal& operator=(const SubscriptionStatusLocal&) = delete;

        static SubscriptionStatusLocal& instance() noexcept;
               
        UStatus init();

        UStatus term();
       
        template<typename T>
        UStatus setStatus(UUri uri, 
                          T status);

        SubscriptionStatus_State getSubscriptionStatus(const UUri &uri);

        UCode getPublisherStatus(const UUri &uri);
        
    private:
    
        SubscriptionStatusLocal() {};

        unordered_map<std::string, UCode> pubStatusMap_;
        unordered_map<std::string, SubscriptionStatus_State> subStatusMap_;
};

uSubscriptionClient& uSubscriptionClient::instance() noexcept {
    
    static uSubscriptionClient client;

    return client;
}

UStatus uSubscriptionClient::init() {
   
    UStatus status;

    do {

        if (UCode::OK != SubscriptionStatusLocal::instance().init().code()) {
            spdlog::error("SubscriptionStatusLocal::instance().init() failed");
            status.set_code(UCode::INTERNAL);
            break;
        }
        
        if (UCode::OK != ZenohRpcClient::instance().init().code()) {
            spdlog::error("ZenohRpcClient::instance().init failed");
            status.set_code(UCode::INTERNAL);
            break;
        }

        status.set_code(UCode::OK);

    } while(0);

    return status;
}

UStatus uSubscriptionClient::term() {

    UStatus status;

    do {
        
        if (UCode::OK != SubscriptionStatusLocal::instance().term().code()) {
            spdlog::error("SubscriptionStatusLocal::instance().term() failed");
            status.set_code(UCode::INTERNAL);
            break;
        }

        if (UCode::OK != ZenohRpcClient::instance().term().code()) {
            spdlog::error("ZenohRpcClient::instance().term failed");
            status.set_code(UCode::INTERNAL);
            break;
        }

        status.set_code(UCode::OK);

    } while(0);

    return status;
}

UStatus uSubscriptionClient::createTopic(CreateTopicRequest &request) {

    UStatus status;

    do {
        SubscriptionStatusLocal::instance().setStatus(request.topic(), UCode::UNKNOWN);

        UPayload payload = sendRequest(request);
        if (0 == payload.size()) {
            spdlog::error("payload size is 0");
            status.set_code(UCode::UNKNOWN);
            break;
        }
            
        if (false == status.ParseFromArray(payload.data(), payload.size())) {
            spdlog::error("ParseFromArray failed");
            status.set_code(UCode::UNKNOWN);
            break;
        }

        SubscriptionStatusLocal::instance().setStatus(request.topic(), status.code());

    } while(0);

    return status;
}

UStatus uSubscriptionClient::registerNotifications(NotificationsRequest &request,
                                                 const notifyFunc func) {
    UStatus status;

    return status;
}

UStatus uSubscriptionClient::deprecateTopic(const DeprecateTopicRequest &request) {
    
    UStatus status;

    UPayload payload = sendRequest(request);

    status.set_code(UCode::INTERNAL);

    if (0 == payload.size()) {
        spdlog::error("payload size is 0");
        return status;
    }
           
    if (false == status.ParseFromArray(payload.data(), payload.size())) {
        spdlog::error("ParseFromArray failed");
        return status;
    }

    return status;
}

std::optional<SubscriptionResponse> uSubscriptionClient::subscribe(const SubscriptionRequest &request) {
    
    UPayload payload = sendRequest(request);

    if (0 == payload.size()) {
        spdlog::error("payload size is 0");
        return std::nullopt;
    }

    SubscriptionResponse resp;

    if (false == resp.ParseFromArray(payload.data(), payload.size())) {
        spdlog::error("ParseFromArray failed");
        return std::nullopt;
    }
    
    SubscriptionStatusLocal::instance().setStatus(request.topic(), resp.status().state());
    
    return resp;
}

UStatus uSubscriptionClient::unSubscribe(const UnsubscribeRequest &request) {
   
    UStatus status;

    do {
        UPayload payload = sendRequest(request);

        if (0 == payload.size()) {
            spdlog::error("payload size is 0");
            status.set_code(UCode::INTERNAL);
            break;
        }

        UStatus resp;

        if (false == resp.ParseFromArray(payload.data(), payload.size())) {
            spdlog::error("ParseFromArray failed");
            status.set_code(UCode::INTERNAL);
            break;
        }

        if (UCode::OK == resp.code()) {
            SubscriptionStatusLocal::instance().setStatus(request.topic(), SubscriptionStatus_State_UNSUBSCRIBED);
        } else {
            /* TODO */
        }

        status.set_code(UCode::OK);

    } while(0);
    
    return status;
}

template <typename T>
UPayload uSubscriptionClient::sendRequest(const T &request) noexcept {

    UUID uuid = Uuidv8Factory::create();

    uint8_t buffer[request.ByteSizeLong() + sizeof(uint8_t)];
    size_t size = request.ByteSizeLong() ;
    
    UPayload retPayload(nullptr, 0, UPayloadType::REFERENCE);

    do {

        if (false == request.SerializeToArray(buffer + 1, size)) {
            spdlog::error("SerializeToArray failure");
            break;
        }

        const google::protobuf::Descriptor* descriptor = request.GetDescriptor();

        buffer[0] = static_cast<uint8_t>(requestStrToNum[descriptor->full_name()]);

        UPayload payload(buffer, sizeof(buffer), UPayloadType::REFERENCE);
       
        UAttributesBuilder builder(uuid, UMessageType::REQUEST, UPriority::STANDARD);

        auto future = ZenohRpcClient::instance().invokeMethod(uSubRequestsUri, payload, builder.build());
        if (false == future.valid()) {
            spdlog::error("result is not valid");
            break;
        }

        switch (std::future_status status = future.wait_for(responseTimeout_); status) {
            case std::future_status::timeout: {
                spdlog::error("timeout received while waiting for response");
                return retPayload;
            } 
            break;
            case std::future_status::ready: {
                retPayload = future.get();
            }
            break;
        }

    } while(0);

    return retPayload;
}

SubscriptionStatusLocal& SubscriptionStatusLocal::instance() noexcept {

    static SubscriptionStatusLocal instance;

    return instance;
}

UStatus SubscriptionStatusLocal::init() { 
    
    UStatus status;

    status.set_code(UCode::OK);
    
    return status;
};

UStatus SubscriptionStatusLocal::term() {
    
    UStatus status;

    status.set_code(UCode::OK);
    
    return status;
};

template<typename T>
UStatus SubscriptionStatusLocal::setStatus(UUri uri, 
                                           T status) {

    std::string serUri;
    UStatus retStatus;

    do {
        
        if (!uri.SerializeToString(&serUri)) {
            spdlog::error("SerializeToString failed");
            retStatus.set_code(UCode::INTERNAL);
            break;
        }

        if constexpr (std::is_same_v<T, SubscriptionStatus_State>) {
            subStatusMap_[serUri] = status;
        } else if constexpr (std::is_same_v<T, UCode>){
            pubStatusMap_[serUri] = status;
        } else {
            spdlog::error("status type is not supported");
            retStatus.set_code(UCode::INTERNAL);
            break;
        }

        retStatus.set_code(UCode::OK);
    } while(0);

    return retStatus;
}

SubscriptionStatus_State SubscriptionStatusLocal::getSubscriptionStatus(const UUri &uri) {

    std::string serUri;

    if (!uri.SerializeToString(&serUri)) {
        spdlog::error("SerializeToString failed");
        return SubscriptionStatus_State_UNSUBSCRIBED;
    }

    if (subStatusMap_.find(serUri) != subStatusMap_.end()) {
        return subStatusMap_[serUri];
    } else {
        return SubscriptionStatus_State_UNSUBSCRIBED;
    }            
}

UCode SubscriptionStatusLocal::getPublisherStatus(const UUri &uri) {
    
    std::string serUri;
    if (!uri.SerializeToString(&serUri)) {
        return UCode::INTERNAL;
    }

    if (pubStatusMap_.find(serUri) != pubStatusMap_.end()) {
        return pubStatusMap_[serUri];
    } else {
        return UCode::UNAVAILABLE;
    }
}

UCode getPublisherStatus(const UUri &uri) {
   return SubscriptionStatusLocal::instance().getPublisherStatus(uri);
}

SubscriptionStatus_State getSubscriberStatus(const UUri &uri) {
    return SubscriptionStatusLocal::instance().getSubscriptionStatus(uri);
}
