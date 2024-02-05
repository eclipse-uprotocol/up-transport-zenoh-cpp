#ifndef _H_USUBSCRIPTION_CLIENT_H_
#define _H_USUBSCRIPTION_CLIENT_H_

#include <future>
#include <optional>
#include <up-cpp/transport/datamodel/UPayload.h>
#include <up-core-api/core/usubscription/v3/usubscription.pb.h>
#include <up-core-api/ustatus.pb.h>

using namespace std;
using namespace uprotocol::utransport;
using namespace uprotocol::core::usubscription::v3;
using namespace uprotocol::v1;

namespace uprotocol::uSubscription {

   typedef void (*notifyFunc)(const SubscriptionStatus& );

   class uSubscriptionClient {

      public:

         uSubscriptionClient(const uSubscriptionClient&) = delete;
         
         uSubscriptionClient& operator=(const uSubscriptionClient&) = delete;

         static uSubscriptionClient& instance() noexcept;

         /**
         * initialized the uSubClient 
         * returns UCode_OK on success and ERROR on failure
         */
         UStatus init();

         /**
         * terminates the uSubClient 
         * returns UCode_OK on success and ERROR on failure
         */
         UStatus term(); 

         /**
         * subscribe for topic 
         * @param request - request
         * @param func - pointer for notification function
         * @return returns SubscriptionResponse on success and nullopt on failure
         */
         std::optional<SubscriptionResponse> subscribe(const SubscriptionRequest &request);

         /**
         * unSubscribe from topic 
         * @param request - request
         * @return returns OK on success and ERROR on failure
         */
         UStatus unSubscribe(const UnsubscribeRequest &request);

         /**
         * fetch subscriptions from uSubscription for a specific URI 
         * @param request the request
         * @return returns FetchSubscriptionsResponse on success , nullopt on failure 
         */
         std::optional<FetchSubscriptionsResponse> fetchSubscriptions(const FetchSubscriptionsRequest &request);

         /**
         * create topic
         * @param request - request
         * @return returns OK on success and ERROR on failure
         */
         UStatus createTopic(CreateTopicRequest &request);

         /**
         * deprecate topic
         * @param request - request
         * @return returns OK on success and ERROR on failure
         */
         UStatus deprecateTopic(const DeprecateTopicRequest &request);

         /**
         * fetch subscribers from uSubscription for a specific URI 
         * @param request  - requeset
         * @return returns FetchSubscribersResponse on success , nullopt on failure 
         */
         std::optional<FetchSubscribersResponse> fetchSubscribers(const FetchSubscribersRequest &request);

         /**
         * register from notifications for a specific URI 
         * @param request - request
         * @return returns OK on success and ERROR on failure
         */
         UStatus registerNotifications(NotificationsRequest &request,
                                       const notifyFunc func);

         /**
         * unregister from notifications for a specific URI 
         * @param request - request
         * @return returns OK on success and ERROR on failure
         */
         UStatus unRegisterNotifications(const NotificationsRequest &request);

      private:
          
         uSubscriptionClient() {}

         template <typename T>
            UPayload sendRequest(const T &request) noexcept;
         
         static constexpr auto responseTimeout_ = std::chrono::milliseconds(1000);
   }; 
}

#endif  /* _USUBSCRIPTION_CLIENT_H_ */