
/*
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *     http://aws.amazon.com/apache2.0/
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#ifndef AACE_ENGINE_ADDRESS_BOOK_ADDRESSBOOK_CLOUD_UPLOADER_REST_AGENT_H
#define AACE_ENGINE_ADDRESS_BOOK_ADDRESSBOOK_CLOUD_UPLOADER_REST_AGENT_H

#include <condition_variable>
#include <queue>

#include <AVSCommon/SDKInterfaces/AuthDelegateInterface.h>
#include <AVSCommon/Utils/DeviceInfo.h>
#include <AVSCommon/Utils/HTTP/HttpResponseCode.h>
#include <AVSCommon/Utils/LibcurlUtils/HttpDelete.h>
#include <AVSCommon/Utils/LibcurlUtils/HttpGet.h>
#include <AVSCommon/Utils/LibcurlUtils/HttpPost.h>
#include <AVSCommon/Utils/LibcurlUtils/HTTPResponse.h>
#include <AVSCommon/Utils/RequiresShutdown.h>
#include <AVSCommon/Utils/UUIDGeneration/UUIDGeneration.h>

#include <AACE/Engine/Alexa/AlexaEndpointInterface.h>
#include "AACE/Engine/Utils/JSON/JSON.h"

namespace aace {
namespace engine {
namespace addressBook {

class AddressBookCloudUploaderRESTAgent : public alexaClientSDK::avsCommon::utils::RequiresShutdown {
private:
    AddressBookCloudUploaderRESTAgent(
        std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::AuthDelegateInterface> authDelegate,
        std::shared_ptr<alexaClientSDK::avsCommon::utils::DeviceInfo> deviceInfo);

    bool initialize(std::shared_ptr<aace::engine::alexa::AlexaEndpointInterface> alexaEndpoints);

public:
    using HTTPResponse = alexaClientSDK::avsCommon::utils::libcurlUtils::HTTPResponse;

    static std::shared_ptr<AddressBookCloudUploaderRESTAgent> create(
        std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::AuthDelegateInterface> authDelegate,
        std::shared_ptr<alexaClientSDK::avsCommon::utils::DeviceInfo> deviceInfo,
        std::shared_ptr<aace::engine::alexa::AlexaEndpointInterface> alexaEndpoints);

    virtual ~AddressBookCloudUploaderRESTAgent() = default;

    bool isAccountProvisioned();

    std::string createAndGetCloudAddressBook(
        const std::string& addressBookSourceId,
        const std::string& addressBookType);
    bool getCloudAddressBookId(
        const std::string& addressBookSourceId,
        const std::string& addressBookType,
        std::string& cloudAddressBookId);
    bool deleteCloudAddressBook(const std::string& cloudAddressBookId);

    HTTPResponse uploadDocumentToCloud(
        std::shared_ptr<rapidjson::Document> document,
        const std::string& cloudAddressBookId);
    bool parseCreateAddressBookEntryResponse(const HTTPResponse& response, std::queue<std::string>& failedEntries);
    std::string buildFailedEntriesJson(std::queue<std::string>& failedContact);

    std::string getHTTPErrorString(const HTTPResponse& response);

    /// Resets the internal ACMS REST attributes
    void reset();

protected:
    // RequiresShutdown
    void doShutdown() override;

private:
    enum class CommsProvisionStatus {
        /*
         * Invalid state
         */
        INVALID,

        /*
         * Status is new or unknown.
         */
        UNKNOWN,

        /*
         * Account is provisioned.
         */
        PROVISIONED,

        /*
         * Account is deprovisioned.
         */
        DEPROVISIONED,

        /*
         * Account is auto provisioned.
         */
        AUTO_PROVISIONED,
    };

    struct AlexaAccountInfo {
        std::string commsId;
        CommsProvisionStatus provisionStatus;
    };

    void setPceId(const std::string& pceId);
    std::string getPceId();
    bool isPceIdValid();

    AlexaAccountInfo getAlexaAccountInfo();
    std::string getPceIdFromIdentity(const std::string& commsId);

    std::vector<std::string> buildCommonHTTPHeader();

    std::pair<bool, HTTPResponse> doPost(
        const std::string& url,
        const std::vector<std::string> headerLines,
        const std::string& data,
        std::chrono::seconds timeout);
    std::pair<bool, HTTPResponse> doGet(const std::string& url, const std::vector<std::string>& headers);
    std::pair<bool, HTTPResponse> doDelete(const std::string& url, const std::vector<std::string>& headers);

    std::string buildCreateAddressBookDataJson(
        const std::string& addressBookSourceId,
        const std::string& addressBookType);

private:
    std::string m_pceId;

    /// Indicates shutting down of the module.
    std::atomic<bool> m_isShuttingDown;

    std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::AuthDelegateInterface> m_authDelegate;
    std::shared_ptr<alexaClientSDK::avsCommon::utils::DeviceInfo> m_deviceInfo;

    /// ACMS REST endpoint used for uploading.
    std::string m_acmsEndpoint;

    /// Mutex to allow serialized access to m_pceId
    std::mutex m_pceIdMutex;

    /// Mutex used for wait on m_wake
    std::mutex m_wakeMutex;

    /// Condition variable used to wake waits due to network retries.
    std::condition_variable m_wake;
};

}  // namespace addressBook
}  // namespace engine
}  // namespace aace

#endif  //AACE_ENGINE_ADDRESS_BOOK_ADDRESSBOOK_CLOUD_UPLOADER_REST_AGENT_H
