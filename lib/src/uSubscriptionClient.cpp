#include <up-client-zenoh-cpp/rpc/zenohRpcClient.h>
#include <up-client-zenoh-cpp/usubscription/uSubscriptionClient.h>
#include <up-client-zenoh-cpp/usubscription/uSubscriptionCommon.h>
#include <up-cpp/uuid/serializer/UuidSerializer.h>
#include <up-cpp/uuid/factory/Uuidv8Factory.h>
#include <core/usubscription/v3/usubscription.pb.h>
#include <../include_internal/usubscription/uSubscriptionLocalManager.h>
#include <google/protobuf/message.h>
#include <ustatus.pb.h>

using namespace std;
using namespace uprotocol::utransport;
using namespace uprotocol::uri;
using namespace uprotocol::uuid;
using namespace uprotocol::uSubscription;

uSubscriptionClient& uSubscriptionClient::instance() noexcept {
    
    static uSubscriptionClient client;

    return client;
}

UStatus uSubscriptionClient::init() {
   
    UStatus status;

    do {

        if (UCode::OK != SubscriptionLocalManager::instance().init().code()) {
            spdlog::error("SubscriptionLocalManager::instance().init() failed");
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
        
        if (UCode::OK != SubscriptionLocalManager::instance().term().code()) {
            spdlog::error("SubscriptionLocalManager::instance().term() failed");
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
        
        if (UCode::OK != SubscriptionLocalManager::instance().setStatus(request.topic(), UCode::UNKNOWN).code()) {
            spdlog::error("setStatus failed");
            status.set_code(UCode::INTERNAL);
            break;
        }

        auto payload = sendRequest(request);
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

        if (UCode::ALREADY_EXISTS == status.code()) {
            status.set_code(UCode::OK);
        }

        if (UCode::OK != SubscriptionLocalManager::instance().setStatus(request.topic(), status.code()).code()) {
            spdlog::error("setStatus failed");
            status.set_code(UCode::INTERNAL);
            break;
        }

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

    status.set_code(UCode::INTERNAL);

    auto payload = sendRequest(request);

    if (0 == payload.size()) {
        spdlog::error("payload size is 0");
        return status;
    }
           
    if (false == status.ParseFromArray(payload.data(), payload.size())) {
        spdlog::error("ParseFromArray failed");
        return status;
    }

    if (UCode::OK != SubscriptionLocalManager::instance().setStatus(request.topic(), status.code()).code()) {
        spdlog::error("setStatus failed");
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
    
    if (UCode::OK != SubscriptionLocalManager::instance().setStatus(request.topic(), resp.status().state()).code()) {
        spdlog::error("setStatus failed");
        return std::nullopt;
    }
    
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
            if (UCode::OK != SubscriptionLocalManager::instance().setStatus(request.topic(), SubscriptionStatus_State_UNSUBSCRIBED).code()) {
                spdlog::error("setStatus failed");
                status.set_code(UCode::INTERNAL);
                break;
            }
            status.set_code(UCode::OK);
        } else {
            /* TODO - what should be the behavior in case of an error */
            if (UCode::OK != SubscriptionLocalManager::instance().setStatus(request.topic(), SubscriptionStatus_State_UNSUBSCRIBED).code()) {
                spdlog::error("setStatus failed");
                status.set_code(UCode::INTERNAL);
                break;
            }
            status.set_code(UCode::INTERNAL);
        }
        
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

SubscriptionLocalManager& SubscriptionLocalManager::instance() noexcept {

    static SubscriptionLocalManager instance;

    return instance;
}

UStatus SubscriptionLocalManager::init() { 
    
    UStatus status;

    status.set_code(UCode::OK);
    
    return status;
};

UStatus SubscriptionLocalManager::term() {
    
    UStatus status;

    status.set_code(UCode::OK);
    
    return status;
};

template<typename T>
UStatus SubscriptionLocalManager::setStatus(UUri uri, 
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

SubscriptionStatus_State SubscriptionLocalManager::getSubscriptionStatus(const UUri &uri) {

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

UCode SubscriptionLocalManager::getPublisherStatus(const UUri &uri) {
    
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
   return SubscriptionLocalManager::instance().getPublisherStatus(uri);
}

SubscriptionStatus_State getSubscriberStatus(const UUri &uri) {
    return SubscriptionLocalManager::instance().getSubscriptionStatus(uri);
}
