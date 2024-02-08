#ifndef _H_USUBSCRIPTION_COMMON_H_
#define _H_USUBSCRIPTION_COMMON_H_

#include <unordered_map>
#include <up-cpp/uri/serializer/LongUriSerializer.h>
#include <uri.pb.h>

using namespace uprotocol::v1;
using namespace uprotocol::uri;

namespace uprotocol::uSubscription {

/* request enumberations */
enum class Request {
    SUBSCRIPTION_REQUEST = 1,
    UNSUBSCRIBE_REQUEST,
    FETCH_SUBSCRIPTION_REQUEST,
    CREATE_TOPIC_REQUEST,
    DEPRECATE_TOPIC_REQUEST,
    NOTIFICATION_REQUEST,
    FETCH_SUBSCRIBERS_REQUEST,
    RESET_REQUEST
};

static std::unordered_map<std::string, Request> requestStrToNum = {
    {"uprotocol.core.usubscription.v3.SubscriptionRequest",         Request::SUBSCRIPTION_REQUEST},
    {"uprotocol.core.usubscription.v3.UnsubscribeRequest",          Request::UNSUBSCRIBE_REQUEST},
    {"uprotocol.core.usubscription.v3.FetchSubscriptionsRequest",   Request::FETCH_SUBSCRIPTION_REQUEST},
    {"uprotocol.core.usubscription.v3.CreateTopicRequest",          Request::CREATE_TOPIC_REQUEST},
    {"uprotocol.core.usubscription.v3.DeprecateTopicRequest",       Request::DEPRECATE_TOPIC_REQUEST},
    {"uprotocol.core.usubscription.v3.NotificationsRequest",        Request::NOTIFICATION_REQUEST},
    {"uprotocol.core.usubscription.v3.FetchSubscribersRequest",     Request::FETCH_SUBSCRIBERS_REQUEST},
    {"uprotocol.core.usubscription.v3.ResetRequest",                Request::RESET_REQUEST}};
}

//core.usubscription.subscribe
static UUri uSubRequestsUri = LongUriSerializer::deserialize("/core.usubscription/1/rpc.subscribe"); 
//uSub->streamer - rpc regex 
static UUri uSubRemoteRequestsUri; 
/* URI for sending usubscription updates */ 
static UUri uSubUpdatesUri; 
#endif /* _USUBSCRIPTION_COMMON_H_ */