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

// JSON for Modern C++
#include <nlohmann/json.hpp>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <AVSCommon/AVS/Initialization/AlexaClientSDKInit.h>
#include <AVSCommon/Utils/WaitEvent.h>

#include <AACE/AddressBook/AddressBook.h>
#include <AACE/Engine/AddressBook/AddressBookCloudUploader.h>
#include <AACE/Test/Unit/Metrics/MockMetricRecorderServiceInterface.h>

namespace aace {
namespace test {
namespace unit {
namespace addressBook {

/// Plenty of timeout to wait for HTTP timeouts and retries
static std::chrono::seconds TIMEOUT(20);

/// Plenty of timeout to wait for HTTP timeouts with CleanAddressBooks
static std::chrono::seconds LARGE_TIMEOUT(60);

/// Mock token to be returned by @c getAuthToken
static const std::string AUTH_TOKEN = "MockAuthToken";

using json = nlohmann::json;

class MockAuthDelegateInterface : public alexaClientSDK::avsCommon::sdkInterfaces::AuthDelegateInterface {
public:
    MOCK_METHOD1(
        addAuthObserver,
        void(std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface> observer));
    MOCK_METHOD1(
        removeAuthObserver,
        void(std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface> observer));
    MOCK_METHOD0(getAuthToken, std::string());
    MOCK_METHOD1(onAuthFailure, void(const std::string& token));
};

class MockNetworkInfoProvider : public aace::network::NetworkInfoProvider {
public:
    MOCK_METHOD0(getNetworkStatus, aace::network::NetworkInfoProvider::NetworkStatus());
    MOCK_METHOD0(getWifiSignalStrength, int());
};

class MockNetworkObservableInterface : public aace::engine::network::NetworkObservableInterface {
public:
    MOCK_METHOD1(addObserver, void(std::shared_ptr<aace::engine::network::NetworkInfoObserver> observer));
    MOCK_METHOD1(removeObserver, void(std::shared_ptr<aace::engine::network::NetworkInfoObserver> observer));
};

class MockAddressBookServiceInterface : public aace::engine::addressBook::AddressBookServiceInterface {
public:
    MOCK_METHOD2(
        addObserver,
        void(std::shared_ptr<aace::engine::addressBook::AddressBookObserver> observer, const std::string& serviceType));
    MOCK_METHOD1(removeObserver, void(std::shared_ptr<aace::engine::addressBook::AddressBookObserver> observer));
    MOCK_METHOD2(
        getEntries,
        bool(const std::string& id, std::weak_ptr<aace::addressBook::AddressBook::IAddressBookEntriesFactory> factory));
    MOCK_METHOD1(setDelegate, void(std::shared_ptr<aace::engine::addressBook::AddressBookDelegateInterface> delegate));
    MOCK_METHOD0(servicesEnablementChanged, void());
};

class DummyAlexaEndpointInterface : public aace::engine::alexa::AlexaEndpointInterface {
public:
    std::string getAVSGateway() override {
        // Not called.
        return "";
    }
    std::string getLWAEndpoint() override {
        // Not called.
        return "";
    }
    std::string getACMSEndpoint() override {
        return "https://alexa-comms-mobile-service-na.amazon.com";
    }

    std::string getFeatureDiscoveryEndpoint() override {
        return "";
    }
};

// clang-format off
static const std::string CAPABILITIES_CONFIG_JSON =
    "{"
    "    \"deviceInfo\":{"
    "        \"deviceSerialNumber\":\"MockAddressBookTest\", "
    "        \"clientId\":\"MockClientId\","
    "        \"productId\":\"MockProductID\","
    "        \"manufacturerName\":\"MockManufacturerName\","
    "        \"description\":\"MockDescription\""
    "    }"
    " }";
// clang-format on

struct PhoneNumber {
    std::string label;
    std::string number;
};

struct PostalAddress {
    std::string label;
    std::string addressLine1;
    std::string addressLine2;
    std::string addressLine3;
    std::string city;
    std::string stateOrRegion;
    std::string districtOrCounty;
    std::string postalCode;
    std::string country;
    float latitudeInDegrees;
    float longitudeInDegrees;
    float accuracyInMeters;
};

static std::string buildEntryPayloadJustName(
    const std::string& entryId,
    const std::string& firstName,
    const std::string& lastName,
    const std::string& nickName) {
    // clang-format off
        json payload = {
               {"entryId",entryId},
               {"name",{
                    {"firstName",firstName},
                    {"lastName",lastName},
                    {"nickName",nickName}
               }}
        };
    // clang-format on
    return payload.dump();
}

static std::string buildEntryPayloadWithNameAndPhoneNumbers(
    const std::string& entryId,
    const std::string& firstName,
    const std::string& lastName,
    const std::string& nickName,
    const std::vector<PhoneNumber> phoneNumbers) {
    // clang-format off
        json payload = {
               {"entryId",entryId},
               {"name",{
                    {"firstName",firstName},
                    {"lastName",lastName},
                    {"nickName",nickName}
               }},
               {"phoneNumbers", json::array()}
        };
    // clang-format on
    for (auto& phoneNumber : phoneNumbers) {
        payload["phoneNumbers"].push_back({{"label", phoneNumber.label}, {"number", phoneNumber.number}});
    }
    return payload.dump();
}

static std::string buildEntryPayloadWithNameAndPostalAddresses(
    const std::string& entryId,
    const std::string& firstName,
    const std::string& lastName,
    const std::string& nickName,
    const std::vector<PostalAddress> postalAddresses) {
    // clang-format off
        json payload = {
               {"entryId",entryId},
               {"name",{
                    {"firstName",firstName},
                    {"lastName",lastName},
                    {"nickName",nickName}
               }},
               {"postalAddresses", json::array()}
        };
    // clang-format on
    for (auto& postalAddres : postalAddresses) {
        payload["postalAddresses"].push_back({{"label", postalAddres.label},
                                              {"addressLine1", postalAddres.addressLine1},
                                              {"addressLine2", postalAddres.addressLine2},
                                              {"addressLine3", postalAddres.addressLine3},
                                              {"city", postalAddres.city},
                                              {"stateOrRegion", postalAddres.stateOrRegion},
                                              {"districtOrCounty", postalAddres.districtOrCounty},
                                              {"postalCode", postalAddres.postalCode},
                                              {"country", postalAddres.country},
                                              {"latitudeInDegrees", postalAddres.latitudeInDegrees},
                                              {"longitudeInDegrees", postalAddres.longitudeInDegrees},
                                              {"accuracyInMeters", postalAddres.accuracyInMeters}});
    }
    return payload.dump();
}

class AddressBookCloudUploaderTest : public ::testing::Test {
public:
    void SetUp() override {
        auto inString = std::shared_ptr<std::istringstream>(new std::istringstream(CAPABILITIES_CONFIG_JSON));
        alexaClientSDK::avsCommon::avs::initialization::AlexaClientSDKInit::initialize({inString});

        m_mockAuthDelegate = std::make_shared<testing::StrictMock<MockAuthDelegateInterface>>();
        m_mockNetworkObservableInterface = std::make_shared<testing::StrictMock<MockNetworkObservableInterface>>();
        m_mockAddressBookServiceInterface = std::make_shared<testing::StrictMock<MockAddressBookServiceInterface>>();
        m_alexaEndpointInterface = std::make_shared<DummyAlexaEndpointInterface>();
        m_mockMetricRecorder = std::make_shared<aace::test::unit::core::MockMetricRecorderServiceInterface>();

        // create device info
        m_deviceInfo = alexaClientSDK::avsCommon::utils::DeviceInfo::create(
            alexaClientSDK::avsCommon::utils::configuration::ConfigurationNode::getRoot());

        m_mockContactAddressBook = std::make_shared<aace::engine::addressBook::AddressBookEntity>(
            "1000", "TestAddressBook", aace::engine::addressBook::AddressBookType::CONTACT);
        m_mockNavigationAddressBook = std::make_shared<aace::engine::addressBook::AddressBookEntity>(
            "1001", "TestAddressBook", aace::engine::addressBook::AddressBookType::NAVIGATION);

        EXPECT_CALL(*m_mockAuthDelegate, addAuthObserver(testing::_)).WillOnce(testing::Return());
        EXPECT_CALL(*m_mockAuthDelegate, removeAuthObserver(testing::_)).WillOnce(testing::Return());
        EXPECT_CALL(*m_mockNetworkObservableInterface, addObserver(testing::_)).WillOnce(testing::Return());
        EXPECT_CALL(*m_mockNetworkObservableInterface, removeObserver(testing::_)).WillOnce(testing::Return());
        EXPECT_CALL(*m_mockAddressBookServiceInterface, addObserver(testing::_, "AVS")).WillOnce(testing::Return());
        EXPECT_CALL(*m_mockAddressBookServiceInterface, removeObserver(testing::_)).WillOnce(testing::Return());

        m_addressBookCloudUploader = aace::engine::addressBook::AddressBookCloudUploader::create(
            m_mockAddressBookServiceInterface,
            m_mockAuthDelegate,
            m_deviceInfo,
            aace::network::NetworkInfoProvider::NetworkStatus::CONNECTED,
            m_mockNetworkObservableInterface,
            m_alexaEndpointInterface,
            m_mockMetricRecorder,
            false);  // To false, makes test cases simple by not waiting on EXPECT_CALL due to cleaning of Address Book events.
    }

    void TearDown() override {
        m_addressBookCloudUploader->shutdown();
        if (alexaClientSDK::avsCommon::avs::initialization::AlexaClientSDKInit::isInitialized()) {
            alexaClientSDK::avsCommon::avs::initialization::AlexaClientSDKInit::uninitialize();
        }
    }

    std::shared_ptr<alexaClientSDK::avsCommon::utils::DeviceInfo> m_deviceInfo;
    std::shared_ptr<aace::engine::addressBook::AddressBookEntity> m_mockContactAddressBook;
    std::shared_ptr<aace::engine::addressBook::AddressBookEntity> m_mockNavigationAddressBook;

    std::shared_ptr<aace::engine::addressBook::AddressBookCloudUploader> m_addressBookCloudUploader;
    std::shared_ptr<testing::StrictMock<MockAuthDelegateInterface>> m_mockAuthDelegate;
    std::shared_ptr<testing::StrictMock<MockNetworkObservableInterface>> m_mockNetworkObservableInterface;
    std::shared_ptr<testing::StrictMock<MockAddressBookServiceInterface>> m_mockAddressBookServiceInterface;
    std::shared_ptr<aace::engine::alexa::AlexaEndpointInterface> m_alexaEndpointInterface;
    std::shared_ptr<aace::engine::metrics::MetricRecorderServiceInterface> m_mockMetricRecorder;
};

TEST_F(AddressBookCloudUploaderTest, create) {
    ASSERT_NE(nullptr, m_addressBookCloudUploader);
}

TEST_F(AddressBookCloudUploaderTest, WithNoNetworkAndAuthRefreshedAddAndRemoveAddressBookExpectNoGetEntiresCall) {
    // Expect No Call for GetEntries
    EXPECT_CALL(*m_mockAddressBookServiceInterface, getEntries("1000", testing::_)).Times(0);

    // First disconnect the network as to make the event loop wait for all the events.
    m_addressBookCloudUploader->onNetworkInfoChanged(
        aace::network::NetworkInfoProvider::NetworkStatus::DISCONNECTED, 123);
    m_addressBookCloudUploader->onAuthStateChange(
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::State::REFRESHED,
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::Error::SUCCESS);

    m_addressBookCloudUploader->addressBookAdded(m_mockContactAddressBook);
    m_addressBookCloudUploader->addressBookRemoved(m_mockContactAddressBook);

    // Now connect the network
    m_addressBookCloudUploader->onNetworkInfoChanged(aace::network::NetworkInfoProvider::NetworkStatus::CONNECTED, 123);

    // Sleep to allow AddressBookCloudUploader threads to run.
    std::this_thread::sleep_for(std::chrono::seconds(3));
}

TEST_F(AddressBookCloudUploaderTest, WithNoNetworkAndAuthRefreshedAddSingleContactAddressBookExpectNoGetEntriesCall) {
    // Expect no call for GetEntries
    EXPECT_CALL(*m_mockAddressBookServiceInterface, getEntries("1000", testing::_)).Times(0);

    m_addressBookCloudUploader->onNetworkInfoChanged(
        aace::network::NetworkInfoProvider::NetworkStatus::DISCONNECTED, 123);
    m_addressBookCloudUploader->onAuthStateChange(
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::State::REFRESHED,
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::Error::SUCCESS);

    m_addressBookCloudUploader->addressBookAdded(m_mockContactAddressBook);

    // Sleep to allow AddressBookCloudUploader threads to run.
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

TEST_F(
    AddressBookCloudUploaderTest,
    WithNoNetworkAndAuthRefreshedAddSingleNavigationAddressBookExpectNoGetEntriesCall) {
    // Expect No Call for GetEntries
    EXPECT_CALL(*m_mockAddressBookServiceInterface, getEntries("1001", testing::_)).Times(0);

    m_addressBookCloudUploader->onNetworkInfoChanged(
        aace::network::NetworkInfoProvider::NetworkStatus::DISCONNECTED, 123);
    m_addressBookCloudUploader->onAuthStateChange(
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::State::REFRESHED,
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::Error::SUCCESS);

    m_addressBookCloudUploader->addressBookAdded(m_mockNavigationAddressBook);

    // Sleep to allow AddressBookCloudUploader threads to run.
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

TEST_F(AddressBookCloudUploaderTest, WithNetworkAndNoAuthRefreshedAddSingleContactAddressBookExpectNoGetEntriesCall) {
    // Expect No Call for GetEntries
    EXPECT_CALL(*m_mockAddressBookServiceInterface, getEntries("1000", testing::_)).Times(0);

    m_addressBookCloudUploader->onNetworkInfoChanged(aace::network::NetworkInfoProvider::NetworkStatus::CONNECTED, 123);
    m_addressBookCloudUploader->onAuthStateChange(
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::State::UNINITIALIZED,
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::Error::SUCCESS);

    m_addressBookCloudUploader->addressBookAdded(m_mockContactAddressBook);

    // Sleep to allow AddressBookCloudUploader threads to run.
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

TEST_F(AddressBookCloudUploaderTest, WithNetworkAndAuthRefreshedAddSingleContactAddressBookWithCleanAllAddressBook) {
    alexaClientSDK::avsCommon::utils::WaitEvent waitEvent;
    auto mockAuthDelegate = std::make_shared<testing::StrictMock<MockAuthDelegateInterface>>();
    auto mockNetworkObservableInterface = std::make_shared<testing::StrictMock<MockNetworkObservableInterface>>();
    auto mockAddressBookServiceInterface = std::make_shared<testing::StrictMock<MockAddressBookServiceInterface>>();
    auto alexaEndpointInterface = std::make_shared<DummyAlexaEndpointInterface>();

    EXPECT_CALL(*mockAuthDelegate, addAuthObserver(testing::_)).WillOnce(testing::Return());
    EXPECT_CALL(*mockAuthDelegate, removeAuthObserver(testing::_)).WillOnce(testing::Return());
    EXPECT_CALL(*mockNetworkObservableInterface, addObserver(testing::_)).WillOnce(testing::Return());
    EXPECT_CALL(*mockNetworkObservableInterface, removeObserver(testing::_)).WillOnce(testing::Return());
    EXPECT_CALL(*mockAddressBookServiceInterface, addObserver(testing::_, "AVS")).WillOnce(testing::Return());
    EXPECT_CALL(*mockAddressBookServiceInterface, removeObserver(testing::_)).WillOnce(testing::Return());

    auto addressBookCloudUploader = aace::engine::addressBook::AddressBookCloudUploader::create(
        mockAddressBookServiceInterface,
        mockAuthDelegate,
        m_deviceInfo,
        aace::network::NetworkInfoProvider::NetworkStatus::CONNECTED,
        mockNetworkObservableInterface,
        alexaEndpointInterface,
        m_mockMetricRecorder,
        true);

    EXPECT_CALL(*mockAuthDelegate, getAuthToken())
        .WillRepeatedly(
            testing::Return(std::string(AUTH_TOKEN)));  // Called multiple times due to provision status retry.

    EXPECT_CALL(*mockAddressBookServiceInterface, getEntries("1000", testing::_))
        .WillOnce(testing::InvokeWithoutArgs([&waitEvent]() -> bool {
            waitEvent.wakeUp();
            return true;
        }));

    addressBookCloudUploader->onNetworkInfoChanged(aace::network::NetworkInfoProvider::NetworkStatus::CONNECTED, 123);
    addressBookCloudUploader->onAuthStateChange(
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::State::REFRESHED,
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::Error::SUCCESS);

    addressBookCloudUploader->addressBookAdded(m_mockContactAddressBook);

    EXPECT_TRUE(waitEvent.wait(LARGE_TIMEOUT));  // Sufficient time for Contact and Navigation address book clean up.

    addressBookCloudUploader->shutdown();
}

TEST_F(AddressBookCloudUploaderTest, WithNetworkAndAuthRefreshedAddSingleContactAddressBook) {
    alexaClientSDK::avsCommon::utils::WaitEvent waitEvent;
    EXPECT_CALL(*m_mockAuthDelegate, getAuthToken())
        .WillRepeatedly(
            testing::Return(std::string(AUTH_TOKEN)));  // Called multiple times due to provision status retry.
    EXPECT_CALL(*m_mockAddressBookServiceInterface, getEntries("1000", testing::_))
        .WillOnce(testing::InvokeWithoutArgs([&waitEvent]() -> bool {
            waitEvent.wakeUp();
            return true;
        }));

    m_addressBookCloudUploader->onNetworkInfoChanged(aace::network::NetworkInfoProvider::NetworkStatus::CONNECTED, 123);
    m_addressBookCloudUploader->onAuthStateChange(
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::State::REFRESHED,
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::Error::SUCCESS);

    m_addressBookCloudUploader->addressBookAdded(m_mockContactAddressBook);

    EXPECT_TRUE(waitEvent.wait(TIMEOUT));
}

TEST_F(AddressBookCloudUploaderTest, WithNetworkAndAuthRefreshedAddSingleNavigationAddressBook) {
    alexaClientSDK::avsCommon::utils::WaitEvent waitEvent;
    EXPECT_CALL(*m_mockAuthDelegate, getAuthToken())
        .WillRepeatedly(
            testing::Return(std::string(AUTH_TOKEN)));  // Called multiple times due to provision status retry.
    EXPECT_CALL(*m_mockAddressBookServiceInterface, getEntries("1001", testing::_))
        .WillOnce(testing::InvokeWithoutArgs([&waitEvent]() -> bool {
            waitEvent.wakeUp();
            return true;
        }));

    m_addressBookCloudUploader->onNetworkInfoChanged(aace::network::NetworkInfoProvider::NetworkStatus::CONNECTED, 123);
    m_addressBookCloudUploader->onAuthStateChange(
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::State::REFRESHED,
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::Error::SUCCESS);

    m_addressBookCloudUploader->addressBookAdded(m_mockNavigationAddressBook);

    EXPECT_TRUE(waitEvent.wait(TIMEOUT));
}

TEST_F(AddressBookCloudUploaderTest, WithNetworkAndAuthRefreshedAddTwiceSameAddressBook) {
    alexaClientSDK::avsCommon::utils::WaitEvent waitEvent1;
    alexaClientSDK::avsCommon::utils::WaitEvent waitEvent2;
    int callCounter = 0;
    EXPECT_CALL(*m_mockAuthDelegate, getAuthToken())
        .WillRepeatedly(
            testing::Return(std::string(AUTH_TOKEN)));  // Called multiple times due to provision status retry.

    m_addressBookCloudUploader->onNetworkInfoChanged(aace::network::NetworkInfoProvider::NetworkStatus::CONNECTED, 123);
    m_addressBookCloudUploader->onAuthStateChange(
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::State::REFRESHED,
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::Error::SUCCESS);

    EXPECT_CALL(*m_mockAddressBookServiceInterface, getEntries("1001", testing::_))
        .Times(2)
        .WillRepeatedly(testing::InvokeWithoutArgs([&waitEvent1, &waitEvent2, &callCounter]() -> bool {
            callCounter++;
            if (callCounter == 1) {
                waitEvent1.wakeUp();
            }
            if (callCounter == 2) {
                waitEvent2.wakeUp();
            }
            return true;
        }));

    EXPECT_TRUE(m_addressBookCloudUploader->addressBookAdded(m_mockNavigationAddressBook));
    EXPECT_TRUE(m_addressBookCloudUploader->addressBookAdded(m_mockNavigationAddressBook));

    EXPECT_TRUE(waitEvent1.wait(TIMEOUT));
    EXPECT_TRUE(waitEvent2.wait(TIMEOUT));
}

TEST_F(
    AddressBookCloudUploaderTest,
    WithNetworkAndAuthRefreshedAddContactAndNavigationAddressBookAndExpectGetEntreisCalledInSameSequence) {
    alexaClientSDK::avsCommon::utils::WaitEvent waitEvent1;
    alexaClientSDK::avsCommon::utils::WaitEvent waitEvent2;
    EXPECT_CALL(
        *m_mockAuthDelegate,
        getAuthToken())
        .WillRepeatedly(
            testing::Return(std::string(AUTH_TOKEN)));  // Called multiple times due to provision status retry.
    {
        ::testing::InSequence dummy;

        EXPECT_CALL(*m_mockAddressBookServiceInterface, getEntries("1000", testing::_))
            .WillOnce(testing::InvokeWithoutArgs([&waitEvent1]() -> bool {
                waitEvent1.wakeUp();
                return true;
            }));
        EXPECT_CALL(*m_mockAddressBookServiceInterface, getEntries("1001", testing::_))
            .WillOnce(testing::InvokeWithoutArgs([&waitEvent2]() -> bool {
                waitEvent2.wakeUp();
                return true;
            }));
    }

    m_addressBookCloudUploader->onNetworkInfoChanged(aace::network::NetworkInfoProvider::NetworkStatus::CONNECTED, 123);
    m_addressBookCloudUploader->onAuthStateChange(
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::State::REFRESHED,
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::Error::SUCCESS);

    EXPECT_TRUE(m_addressBookCloudUploader->addressBookAdded(m_mockContactAddressBook));
    EXPECT_TRUE(m_addressBookCloudUploader->addressBookAdded(m_mockNavigationAddressBook));

    EXPECT_TRUE(waitEvent1.wait(TIMEOUT));
    EXPECT_TRUE(waitEvent2.wait(TIMEOUT));
}

TEST_F(AddressBookCloudUploaderTest, CheckForSequneceCallWhenAddingAndRemovingOneAddressBook) {
    alexaClientSDK::avsCommon::utils::WaitEvent waitEvent;
    EXPECT_CALL(*m_mockAuthDelegate, getAuthToken())
        .Times(testing::AtLeast(1))  // 1 for provisioning check, rest is the uploading of entries with retry.
        .WillRepeatedly(testing::Return(std::string(AUTH_TOKEN)));

    m_addressBookCloudUploader->onNetworkInfoChanged(aace::network::NetworkInfoProvider::NetworkStatus::CONNECTED, 123);
    m_addressBookCloudUploader->onAuthStateChange(
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::State::REFRESHED,
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::Error::SUCCESS);

    EXPECT_CALL(*m_mockAddressBookServiceInterface, getEntries("1000", testing::_))
        .WillOnce(testing::InvokeWithoutArgs([&waitEvent]() -> bool {
            waitEvent.wakeUp();
            return true;
        }));

    m_addressBookCloudUploader->addressBookAdded(m_mockContactAddressBook);
    EXPECT_TRUE(waitEvent.wait(TIMEOUT));

    m_addressBookCloudUploader->addressBookRemoved(m_mockContactAddressBook);

    // Sleep to allow AddressBookCloudUploader threads to run.
    std::this_thread::sleep_for(std::chrono::seconds(3));
}

TEST_F(AddressBookCloudUploaderTest, CheckForSequneceCallWhenAddingAndRemovingTwoAddressBook) {
    alexaClientSDK::avsCommon::utils::WaitEvent waitEvent1;
    alexaClientSDK::avsCommon::utils::WaitEvent waitEvent2;
    EXPECT_CALL(
        *m_mockAuthDelegate,
        getAuthToken())
        .WillRepeatedly(
            testing::Return(std::string(AUTH_TOKEN)));  // Called multiple times due to provision status retry.

    {
        ::testing::InSequence dummy;

        EXPECT_CALL(*m_mockAddressBookServiceInterface, getEntries("1000", testing::_))
            .WillOnce(testing::InvokeWithoutArgs([&waitEvent1]() -> bool {
                waitEvent1.wakeUp();
                return true;
            }));

        EXPECT_CALL(*m_mockAddressBookServiceInterface, getEntries("1001", testing::_))
            .WillOnce(testing::InvokeWithoutArgs([&waitEvent2]() -> bool {
                waitEvent2.wakeUp();
                return true;
            }));
    }

    m_addressBookCloudUploader->onNetworkInfoChanged(aace::network::NetworkInfoProvider::NetworkStatus::CONNECTED, 123);
    m_addressBookCloudUploader->onAuthStateChange(
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::State::REFRESHED,
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::Error::SUCCESS);

    EXPECT_TRUE(m_addressBookCloudUploader->addressBookAdded(m_mockContactAddressBook))
        << "Contact AddressBook cannot be added ";
    EXPECT_TRUE(waitEvent1.wait(TIMEOUT));

    EXPECT_TRUE(m_addressBookCloudUploader->addressBookRemoved(m_mockContactAddressBook))
        << "Contact AddressBook cannot be removed ";
    // Sleep to allow AddressBookCloudUploader threads to run.
    std::this_thread::sleep_for(std::chrono::seconds(3));

    EXPECT_TRUE(m_addressBookCloudUploader->addressBookAdded(m_mockNavigationAddressBook))
        << "Navigation AddressBook cannot be added ";
    EXPECT_TRUE(waitEvent2.wait(TIMEOUT));

    EXPECT_TRUE(m_addressBookCloudUploader->addressBookRemoved(m_mockNavigationAddressBook))
        << "Navigation AddressBook cannot be removed ";
    // Sleep to allow AddressBookCloudUploader threads to run.
    std::this_thread::sleep_for(std::chrono::seconds(3));
}

TEST_F(AddressBookCloudUploaderTest, AddingEntryWithSameEntryIdShouldFail_deprecated) {
    alexaClientSDK::avsCommon::utils::WaitEvent waitEvent;
    EXPECT_CALL(*m_mockAuthDelegate, getAuthToken())
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Return(std::string(AUTH_TOKEN)));
    EXPECT_CALL(*m_mockAddressBookServiceInterface, getEntries(testing::_, testing::_))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Invoke(
            [&waitEvent](
                const std::string& id,
                std::weak_ptr<aace::addressBook::AddressBook::IAddressBookEntriesFactory> factory) -> bool {
                if (auto sharedRef = factory.lock()) {
                    EXPECT_TRUE(sharedRef->addName("001", "Alice", "Smith", "U"));
                    EXPECT_FALSE(sharedRef->addName("001", "Dummy", "Dummy", "U"));
                }
                waitEvent.wakeUp();
                return true;
            }));

    m_addressBookCloudUploader->onNetworkInfoChanged(aace::network::NetworkInfoProvider::NetworkStatus::CONNECTED, 123);
    m_addressBookCloudUploader->onAuthStateChange(
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::State::REFRESHED,
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::Error::SUCCESS);

    m_addressBookCloudUploader->addressBookAdded(m_mockContactAddressBook);

    EXPECT_TRUE(waitEvent.wait(TIMEOUT));
}

TEST_F(AddressBookCloudUploaderTest, AddingEntryWithSameEntryIdShouldFail) {
    alexaClientSDK::avsCommon::utils::WaitEvent waitEvent;
    EXPECT_CALL(*m_mockAuthDelegate, getAuthToken())
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Return(std::string(AUTH_TOKEN)));
    EXPECT_CALL(*m_mockAddressBookServiceInterface, getEntries(testing::_, testing::_))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Invoke(
            [&waitEvent](
                const std::string& id,
                std::weak_ptr<aace::addressBook::AddressBook::IAddressBookEntriesFactory> factory) -> bool {
                if (auto sharedRef = factory.lock()) {
                    EXPECT_TRUE(sharedRef->addEntry(buildEntryPayloadJustName("001", "Alice", "Smith", "U")));
                    EXPECT_FALSE(sharedRef->addEntry(buildEntryPayloadJustName("001", "Dummy", "Dummy", "U")));
                }
                waitEvent.wakeUp();
                return true;
            }));

    m_addressBookCloudUploader->onNetworkInfoChanged(aace::network::NetworkInfoProvider::NetworkStatus::CONNECTED, 123);
    m_addressBookCloudUploader->onAuthStateChange(
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::State::REFRESHED,
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::Error::SUCCESS);

    m_addressBookCloudUploader->addressBookAdded(m_mockContactAddressBook);

    EXPECT_TRUE(waitEvent.wait(TIMEOUT));
}

TEST_F(AddressBookCloudUploaderTest, AddingTwoPhoneNumbersShouldPass_deprecated) {
    alexaClientSDK::avsCommon::utils::WaitEvent waitEvent;
    EXPECT_CALL(*m_mockAuthDelegate, getAuthToken())
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Return(std::string(AUTH_TOKEN)));
    EXPECT_CALL(*m_mockAddressBookServiceInterface, getEntries(testing::_, testing::_))
        .WillRepeatedly(testing::Invoke(
            [&waitEvent](
                const std::string& id,
                std::weak_ptr<aace::addressBook::AddressBook::IAddressBookEntriesFactory> factory) -> bool {
                if (auto sharedRef = factory.lock()) {
                    EXPECT_TRUE(sharedRef->addPhone("001", "HOME", "123456789"));
                    EXPECT_TRUE(sharedRef->addPhone("001", "WORK", "123456789"));
                }
                waitEvent.wakeUp();
                return true;
            }));

    m_addressBookCloudUploader->onNetworkInfoChanged(aace::network::NetworkInfoProvider::NetworkStatus::CONNECTED, 123);
    m_addressBookCloudUploader->onAuthStateChange(
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::State::REFRESHED,
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::Error::SUCCESS);

    m_addressBookCloudUploader->addressBookAdded(m_mockContactAddressBook);

    EXPECT_TRUE(waitEvent.wait(TIMEOUT));
}

TEST_F(AddressBookCloudUploaderTest, AddingTwoPhoneNumberShouldPass) {
    alexaClientSDK::avsCommon::utils::WaitEvent waitEvent;
    EXPECT_CALL(*m_mockAuthDelegate, getAuthToken())
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Return(std::string(AUTH_TOKEN)));
    EXPECT_CALL(*m_mockAddressBookServiceInterface, getEntries(testing::_, testing::_))
        .WillRepeatedly(testing::Invoke(
            [&waitEvent](
                const std::string& id,
                std::weak_ptr<aace::addressBook::AddressBook::IAddressBookEntriesFactory> factory) -> bool {
                if (auto sharedRef = factory.lock()) {
                    // clang-format off
                    EXPECT_TRUE(sharedRef->addEntry(buildEntryPayloadWithNameAndPhoneNumbers("001", "firstName", "lastName", "",
                        {
                            {"HOME", "123456789"}, 
                            {"WORK", "123456789"}
                        }
                    )));
                    // clang-format on
                }
                waitEvent.wakeUp();
                return true;
            }));

    m_addressBookCloudUploader->onNetworkInfoChanged(aace::network::NetworkInfoProvider::NetworkStatus::CONNECTED, 123);
    m_addressBookCloudUploader->onAuthStateChange(
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::State::REFRESHED,
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::Error::SUCCESS);

    m_addressBookCloudUploader->addressBookAdded(m_mockContactAddressBook);

    EXPECT_TRUE(waitEvent.wait(TIMEOUT));
}

TEST_F(AddressBookCloudUploaderTest, VerifyPhoneNumberInputSanity) {
    alexaClientSDK::avsCommon::utils::WaitEvent waitEvent;
    EXPECT_CALL(*m_mockAuthDelegate, getAuthToken())
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Return(std::string(AUTH_TOKEN)));
    EXPECT_CALL(*m_mockAddressBookServiceInterface, getEntries(testing::_, testing::_))
        .WillRepeatedly(testing::Invoke(
            [&waitEvent](
                const std::string& id,
                std::weak_ptr<aace::addressBook::AddressBook::IAddressBookEntriesFactory> factory) -> bool {
                if (auto sharedRef = factory.lock()) {
                    // clang-format off
                    EXPECT_TRUE(sharedRef->addEntry(buildEntryPayloadWithNameAndPhoneNumbers("001", "firstName", "lastName", "",
                        {
                            {"HOME", "123456789"}, 
                            {"HOME", "123456789"}, // Adding HOME label again
                            {"RANDOM_LABEL1", "123456789"}, // Some random label name
                            {"12345678", "123456789"}, // Label as numbers
                            {"WORK", "STRING"}, // Phone number as string.
                            {"ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUV","1234"}, // Label has 100 characters
                            {"MAIL","1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"} // Number has 100 characters
                        }
                    )));
                    EXPECT_FALSE(sharedRef->addEntry(buildEntryPayloadWithNameAndPhoneNumbers("002", "firstName", "lastName", "",
                        {
                            {"ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUV\
                              1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890\
                              ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUV\
                              1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890\
                              ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUV",
                              "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890\
                              ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUV\
                              1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890\
                              ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUV\
                              12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901"}  // Phone Exceeds (Max) 1000 + 1
                        }
                    )));
                    // clang-format on
                }
                waitEvent.wakeUp();
                return true;
            }));

    m_addressBookCloudUploader->onNetworkInfoChanged(aace::network::NetworkInfoProvider::NetworkStatus::CONNECTED, 123);
    m_addressBookCloudUploader->onAuthStateChange(
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::State::REFRESHED,
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::Error::SUCCESS);

    m_addressBookCloudUploader->addressBookAdded(m_mockContactAddressBook);

    EXPECT_TRUE(waitEvent.wait(TIMEOUT));
}

TEST_F(AddressBookCloudUploaderTest, VerifyPostalAddressInputSanity) {
    alexaClientSDK::avsCommon::utils::WaitEvent waitEvent;
    EXPECT_CALL(*m_mockAuthDelegate, getAuthToken())
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Return(std::string(AUTH_TOKEN)));
    EXPECT_CALL(*m_mockAddressBookServiceInterface, getEntries(testing::_, testing::_))
        .WillRepeatedly(testing::Invoke(
            [&waitEvent](
                const std::string& id,
                std::weak_ptr<aace::addressBook::AddressBook::IAddressBookEntriesFactory> factory) -> bool {
                if (auto sharedRef = factory.lock()) {
                    // clang-format off
                    EXPECT_TRUE(sharedRef->addEntry(buildEntryPayloadWithNameAndPostalAddresses("001", "firstName", "lastName", "",
                        {
                            {"Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5}, 
                            {"Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5}, // Adding HOME label again
                            {"RANDOM_LABEL1", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5}, // Adding HOME label again
                            {"12345678", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5}, // // Label as numbers
                            {"LABEL", "ADDRESS_LINE_1_ADDRESS_LINE_1_ADDRESS_LINE_1_ADDRESS_LINE_1_",  // 60
                                      "ADDRESS_LINE_2_ADDRESS_LINE_2_ADDRESS_LINE_1_ADDRESS_LINE_2_",  // 60
                                      "ADDRESS_LINE_3_ADDRESS_LINE_3_ADDRESS_LINE_3_ADDRESS_LINE_3_",  // 60
                                      "CITY_CITY_CITY_CITY_CITY_CITY_CITY_CITY_CITY_CITY_CITY_CITY_CITY_CITY_CITY_CITY_CITY_CITY_CITY_CITY_",  // 100
                                      "STATEORREGION_STATEORREGION_STATEORREGION_STATEORREGION_STATEORREGION_STATEORREGION_STATEORREGION_ST",  // 100
                                      "DISTRICTORCOUNTY_DISTRICTORCOUNTY_DISTRICTORCOUNTY_DISTRICTORCOUNTY_DISTRICTORCOUNTY_DISTRICTORCOUNT",  // 100
                                      "POSTALCODE_POSTALCODE_POSTALCODE_POSTALCODE_POSTALCODE_POSTALCODE_POSTALCODE_POSTALCODE_POSTALCODE_P",  // 100
                                      "COUNTRY_COUNTRY_COUNTRY_COUNTRY_COUNTRY_COUNTRY_COUNTRY_COUNTRY_COUNTRY_COUNTRY_COUNTRY_COUNTRY_COUN",  // 100
                                      90, 100, 5 } // Address fields each having 100 chars
                        }
                    )));
                    EXPECT_FALSE(sharedRef->addEntry(buildEntryPayloadWithNameAndPostalAddresses("002", "firstName", "lastName", "",
                        {
                            {"Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5}, 
                            {"Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5}, // Adding HOME label again
                            {"RANDOM_LABEL1", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5}, // Adding HOME label again
                            {"12345678", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5}, // // Label as numbers
                            {"LABEL", "ADDRESS_LINE_1_ADDRESS_LINE_1_ADDRESS_LINE_1_ADDRESS_LINE_1_A",     // 60 + 1
                                      "ADDRESS_LINE_2_ADDRESS_LINE_2_ADDRESS_LINE_1_ADDRESS_LINE_2_A",     // 60 + 1
                                      "ADDRESS_LINE_3_ADDRESS_LINE_3_ADDRESS_LINE_3_ADDRESS_LINE_3_A",     // 60 + 1
                                      "CITY_CITY_CITY_CITY_CITY_CITY_CITY_CITY_CITY_CITY_CITY_CITY_CITY_CITY_CITY_CITY_CITY_CITY_CITY_CITY_",  // 100
                                      "STATEORREGION_STATEORREGION_STATEORREGION_STATEORREGION_STATEORREGION_STATEORREGION_STATEORREGION_ST",  // 100
                                      "DISTRICTORCOUNTY_DISTRICTORCOUNTY_DISTRICTORCOUNTY_DISTRICTORCOUNTY_DISTRICTORCOUNTY_DISTRICTORCOUNT",  // 100
                                      "POSTALCODE_POSTALCODE_POSTALCODE_POSTALCODE_POSTALCODE_POSTALCODE_POSTALCODE_POSTALCODE_POSTALCODE_P",  // 100
                                      "COUNTRY_COUNTRY_COUNTRY_COUNTRY_COUNTRY_COUNTRY_COUNTRY_COUNTRY_COUNTRY_COUNTRY_COUNTRY_COUNTRY_COUN",  // 100
                                      90, 100, 5 } // Address fields each having 100 chars
                        }
                    )));

                    // clang-format on
                }
                waitEvent.wakeUp();
                return true;
            }));

    m_addressBookCloudUploader->onNetworkInfoChanged(aace::network::NetworkInfoProvider::NetworkStatus::CONNECTED, 123);
    m_addressBookCloudUploader->onAuthStateChange(
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::State::REFRESHED,
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::Error::SUCCESS);

    m_addressBookCloudUploader->addressBookAdded(m_mockNavigationAddressBook);

    EXPECT_TRUE(waitEvent.wait(TIMEOUT));
}

TEST_F(AddressBookCloudUploaderTest, AddingPostalAddressForContactAddressBookShouldPass_deprecated) {
    alexaClientSDK::avsCommon::utils::WaitEvent waitEvent;

    EXPECT_CALL(*m_mockAuthDelegate, getAuthToken()).WillRepeatedly(testing::Return(std::string(AUTH_TOKEN)));

    EXPECT_CALL(*m_mockAddressBookServiceInterface, getEntries(testing::_, testing::_))
        .WillRepeatedly(testing::Invoke(
            [&waitEvent](
                const std::string& id,
                std::weak_ptr<aace::addressBook::AddressBook::IAddressBookEntriesFactory> factory) -> bool {
                if (auto sharedRef = factory.lock()) {
                    // clang-format off
                    EXPECT_TRUE(sharedRef->addPostalAddress("001", "Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5));
                    // clang-format on
                }
                waitEvent.wakeUp();
                return true;
            }));

    m_addressBookCloudUploader->onNetworkInfoChanged(aace::network::NetworkInfoProvider::NetworkStatus::CONNECTED, 123);
    m_addressBookCloudUploader->onAuthStateChange(
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::State::REFRESHED,
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::Error::SUCCESS);

    m_addressBookCloudUploader->addressBookAdded(m_mockContactAddressBook);
    EXPECT_TRUE(waitEvent.wait(TIMEOUT));
}

TEST_F(AddressBookCloudUploaderTest, AddingPostalAddressForContactAddressBookShouldPass) {
    alexaClientSDK::avsCommon::utils::WaitEvent waitEvent;

    EXPECT_CALL(*m_mockAuthDelegate, getAuthToken()).WillRepeatedly(testing::Return(std::string(AUTH_TOKEN)));

    EXPECT_CALL(*m_mockAddressBookServiceInterface, getEntries(testing::_, testing::_))
        .WillRepeatedly(testing::Invoke(
            [&waitEvent](
                const std::string& id,
                std::weak_ptr<aace::addressBook::AddressBook::IAddressBookEntriesFactory> factory) -> bool {
                if (auto sharedRef = factory.lock()) {
                    // clang-format off
                    EXPECT_TRUE(sharedRef->addEntry(buildEntryPayloadWithNameAndPostalAddresses("001", "firstName", "lastName", "",
                        { 
                            {"Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5}
                        }
                    )));
                    // clang-format on
                }
                waitEvent.wakeUp();
                return true;
            }));

    m_addressBookCloudUploader->onNetworkInfoChanged(aace::network::NetworkInfoProvider::NetworkStatus::CONNECTED, 123);
    m_addressBookCloudUploader->onAuthStateChange(
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::State::REFRESHED,
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::Error::SUCCESS);

    m_addressBookCloudUploader->addressBookAdded(m_mockContactAddressBook);
    EXPECT_TRUE(waitEvent.wait(TIMEOUT));
}

TEST_F(AddressBookCloudUploaderTest, AddingPhoneForNavigationAddressBookShouldFail_deprecated) {
    alexaClientSDK::avsCommon::utils::WaitEvent waitEvent;

    EXPECT_CALL(*m_mockAuthDelegate, getAuthToken()).WillRepeatedly(testing::Return(std::string(AUTH_TOKEN)));

    EXPECT_CALL(*m_mockAddressBookServiceInterface, getEntries(testing::_, testing::_))
        .WillRepeatedly(testing::Invoke(
            [&waitEvent](
                const std::string& id,
                std::weak_ptr<aace::addressBook::AddressBook::IAddressBookEntriesFactory> factory) -> bool {
                if (auto sharedRef = factory.lock()) {
                    EXPECT_FALSE(sharedRef->addPhone("001", "HOME", "123456789"));
                }
                waitEvent.wakeUp();
                return true;
            }));

    m_addressBookCloudUploader->onNetworkInfoChanged(aace::network::NetworkInfoProvider::NetworkStatus::CONNECTED, 123);
    m_addressBookCloudUploader->onAuthStateChange(
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::State::REFRESHED,
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::Error::SUCCESS);

    m_addressBookCloudUploader->addressBookAdded(m_mockNavigationAddressBook);

    EXPECT_TRUE(waitEvent.wait(TIMEOUT));
}

TEST_F(AddressBookCloudUploaderTest, AddingPhoneForNavigationAddressBookShouldFail) {
    alexaClientSDK::avsCommon::utils::WaitEvent waitEvent;

    EXPECT_CALL(*m_mockAuthDelegate, getAuthToken()).WillRepeatedly(testing::Return(std::string(AUTH_TOKEN)));

    EXPECT_CALL(*m_mockAddressBookServiceInterface, getEntries(testing::_, testing::_))
        .WillRepeatedly(testing::Invoke(
            [&waitEvent](
                const std::string& id,
                std::weak_ptr<aace::addressBook::AddressBook::IAddressBookEntriesFactory> factory) -> bool {
                if (auto sharedRef = factory.lock()) {
                    // clang-format off
                    EXPECT_FALSE(sharedRef->addEntry(buildEntryPayloadWithNameAndPhoneNumbers("001", "firstName", "lastName", "",
                        {
                            {"HOME", "123456789"}
                        }
                    )));
                    // clang-format on
                }
                waitEvent.wakeUp();
                return true;
            }));

    m_addressBookCloudUploader->onNetworkInfoChanged(aace::network::NetworkInfoProvider::NetworkStatus::CONNECTED, 123);
    m_addressBookCloudUploader->onAuthStateChange(
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::State::REFRESHED,
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::Error::SUCCESS);

    m_addressBookCloudUploader->addressBookAdded(m_mockNavigationAddressBook);

    EXPECT_TRUE(waitEvent.wait(TIMEOUT));
}

TEST_F(AddressBookCloudUploaderTest, AddingTwoPostalAddressShouldPass_deprecated) {
    alexaClientSDK::avsCommon::utils::WaitEvent waitEvent;
    EXPECT_CALL(*m_mockAuthDelegate, getAuthToken())
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Return(std::string(AUTH_TOKEN)));
    EXPECT_CALL(*m_mockAddressBookServiceInterface, getEntries(testing::_, testing::_))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Invoke(
            [&waitEvent](
                const std::string& id,
                std::weak_ptr<aace::addressBook::AddressBook::IAddressBookEntriesFactory> factory) -> bool {
                if (auto sharedRef = factory.lock()) {
                    // clang-format off
                    EXPECT_TRUE( sharedRef->addPostalAddress("001", "Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5));
                    EXPECT_TRUE( sharedRef->addPostalAddress("002", "Work", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5));
                    // clang-format on
                }
                waitEvent.wakeUp();
                return true;
            }));

    m_addressBookCloudUploader->onNetworkInfoChanged(aace::network::NetworkInfoProvider::NetworkStatus::CONNECTED, 123);

    m_addressBookCloudUploader->onAuthStateChange(
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::State::REFRESHED,
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::Error::SUCCESS);

    m_addressBookCloudUploader->addressBookAdded(m_mockNavigationAddressBook);

    EXPECT_TRUE(waitEvent.wait(TIMEOUT));
}

TEST_F(AddressBookCloudUploaderTest, AddingTwoPostalAddressShouldPass) {
    alexaClientSDK::avsCommon::utils::WaitEvent waitEvent;
    EXPECT_CALL(*m_mockAuthDelegate, getAuthToken())
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Return(std::string(AUTH_TOKEN)));
    EXPECT_CALL(*m_mockAddressBookServiceInterface, getEntries(testing::_, testing::_))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Invoke(
            [&waitEvent](
                const std::string& id,
                std::weak_ptr<aace::addressBook::AddressBook::IAddressBookEntriesFactory> factory) -> bool {
                if (auto sharedRef = factory.lock()) {
                    // clang-format off
                    EXPECT_TRUE(sharedRef->addEntry(buildEntryPayloadWithNameAndPostalAddresses("001", "firstName", "lastName", "",
                        {
                            {"Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5}, 
                            {"Work", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5}
                        }  
                    )));
                    // clang-format on
                }
                waitEvent.wakeUp();
                return true;
            }));

    m_addressBookCloudUploader->onNetworkInfoChanged(aace::network::NetworkInfoProvider::NetworkStatus::CONNECTED, 123);

    m_addressBookCloudUploader->onAuthStateChange(
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::State::REFRESHED,
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::Error::SUCCESS);

    m_addressBookCloudUploader->addressBookAdded(m_mockNavigationAddressBook);

    EXPECT_TRUE(waitEvent.wait(TIMEOUT));
}

TEST_F(AddressBookCloudUploaderTest, AddingMaxAllowedPhonesForSameEntryIdShouldFail_deprecated) {
    alexaClientSDK::avsCommon::utils::WaitEvent waitEvent;
    EXPECT_CALL(*m_mockAuthDelegate, getAuthToken())
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Return(std::string(AUTH_TOKEN)));
    EXPECT_CALL(*m_mockAddressBookServiceInterface, getEntries(testing::_, testing::_))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Invoke(
            [&waitEvent](
                const std::string& id,
                std::weak_ptr<aace::addressBook::AddressBook::IAddressBookEntriesFactory> factory) -> bool {
                if (auto sharedRef = factory.lock()) {
                    // 1
                    EXPECT_TRUE(sharedRef->addPhone("001", "HOME", "123456789"));
                    EXPECT_TRUE(sharedRef->addPhone("001", "WORK", "123456789"));
                    EXPECT_TRUE(sharedRef->addPhone("001", "HOME", "123456789"));
                    EXPECT_TRUE(sharedRef->addPhone("001", "WORK", "123456789"));
                    EXPECT_TRUE(sharedRef->addPhone("001", "HOME", "123456789"));
                    EXPECT_TRUE(sharedRef->addPhone("001", "WORK", "123456789"));
                    EXPECT_TRUE(sharedRef->addPhone("001", "HOME", "123456789"));
                    EXPECT_TRUE(sharedRef->addPhone("001", "WORK", "123456789"));
                    EXPECT_TRUE(sharedRef->addPhone("001", "HOME", "123456789"));
                    EXPECT_TRUE(sharedRef->addPhone("001", "WORK", "123456789"));
                    // 11
                    EXPECT_TRUE(sharedRef->addPhone("001", "HOME", "123456789"));
                    EXPECT_TRUE(sharedRef->addPhone("001", "WORK", "123456789"));
                    EXPECT_TRUE(sharedRef->addPhone("001", "HOME", "123456789"));
                    EXPECT_TRUE(sharedRef->addPhone("001", "WORK", "123456789"));
                    EXPECT_TRUE(sharedRef->addPhone("001", "HOME", "123456789"));
                    EXPECT_TRUE(sharedRef->addPhone("001", "WORK", "123456789"));
                    EXPECT_TRUE(sharedRef->addPhone("001", "HOME", "123456789"));
                    EXPECT_TRUE(sharedRef->addPhone("001", "WORK", "123456789"));
                    EXPECT_TRUE(sharedRef->addPhone("001", "HOME", "123456789"));
                    EXPECT_TRUE(sharedRef->addPhone("001", "WORK", "123456789"));
                    // 21
                    EXPECT_TRUE(sharedRef->addPhone("001", "HOME", "123456789"));
                    EXPECT_TRUE(sharedRef->addPhone("001", "WORK", "123456789"));
                    EXPECT_TRUE(sharedRef->addPhone("001", "HOME", "123456789"));
                    EXPECT_TRUE(sharedRef->addPhone("001", "WORK", "123456789"));
                    EXPECT_TRUE(sharedRef->addPhone("001", "HOME", "123456789"));
                    EXPECT_TRUE(sharedRef->addPhone("001", "WORK", "123456789"));
                    EXPECT_TRUE(sharedRef->addPhone("001", "HOME", "123456789"));
                    EXPECT_TRUE(sharedRef->addPhone("001", "WORK", "123456789"));
                    EXPECT_TRUE(sharedRef->addPhone("001", "HOME", "123456789"));
                    EXPECT_TRUE(sharedRef->addPhone("001", "WORK", "123456789"));
                    // 31
                    EXPECT_FALSE(sharedRef->addPhone("001", "WORK", "123456789"));
                }
                waitEvent.wakeUp();
                return true;
            }));

    m_addressBookCloudUploader->onNetworkInfoChanged(aace::network::NetworkInfoProvider::NetworkStatus::CONNECTED, 123);
    m_addressBookCloudUploader->onAuthStateChange(
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::State::REFRESHED,
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::Error::SUCCESS);

    m_addressBookCloudUploader->addressBookAdded(m_mockContactAddressBook);

    EXPECT_TRUE(waitEvent.wait(TIMEOUT));
}

TEST_F(AddressBookCloudUploaderTest, AddingMaxAllowedPhonesForSameEntryIdShouldFail) {
    alexaClientSDK::avsCommon::utils::WaitEvent waitEvent;
    EXPECT_CALL(*m_mockAuthDelegate, getAuthToken())
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Return(std::string(AUTH_TOKEN)));
    EXPECT_CALL(*m_mockAddressBookServiceInterface, getEntries(testing::_, testing::_))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Invoke(
            [&waitEvent](
                const std::string& id,
                std::weak_ptr<aace::addressBook::AddressBook::IAddressBookEntriesFactory> factory) -> bool {
                if (auto sharedRef = factory.lock()) {
                    // clang-format off
                    EXPECT_FALSE(sharedRef->addEntry(buildEntryPayloadWithNameAndPhoneNumbers("001", "firstName", "lastName", "",
                        {
                            {"HOME", "123456789"}, // 1
                            {"WORK", "123456789"},
                            {"HOME", "123456789"},
                            {"WORK", "123456789"},
                            {"HOME", "123456789"},
                            {"WORK", "123456789"},
                            {"HOME", "123456789"},
                            {"WORK", "123456789"},
                            {"HOME", "123456789"},
                            {"WORK", "123456789"},
                            {"HOME", "123456789"}, // 11
                            {"WORK", "123456789"},
                            {"HOME", "123456789"},
                            {"WORK", "123456789"},
                            {"HOME", "123456789"},
                            {"WORK", "123456789"},
                            {"HOME", "123456789"},
                            {"WORK", "123456789"},
                            {"HOME", "123456789"},
                            {"WORK", "123456789"},
                            {"HOME", "123456789"}, //21
                            {"WORK", "123456789"},
                            {"HOME", "123456789"},
                            {"WORK", "123456789"},
                            {"HOME", "123456789"},
                            {"WORK", "123456789"},
                            {"HOME", "123456789"},
                            {"WORK", "123456789"},
                            {"HOME", "123456789"},
                            {"WORK", "123456789"},
                            {"HOME", "123456789"} //31
                        }
                    )));
                    // clang-format on
                }
                waitEvent.wakeUp();
                return true;
            }));

    m_addressBookCloudUploader->onNetworkInfoChanged(aace::network::NetworkInfoProvider::NetworkStatus::CONNECTED, 123);
    m_addressBookCloudUploader->onAuthStateChange(
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::State::REFRESHED,
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::Error::SUCCESS);

    m_addressBookCloudUploader->addressBookAdded(m_mockContactAddressBook);

    EXPECT_TRUE(waitEvent.wait(TIMEOUT));
}

TEST_F(AddressBookCloudUploaderTest, AddingMaxAllowedPostalAddressForSameEntryIdShouldFail_deprecated) {
    alexaClientSDK::avsCommon::utils::WaitEvent waitEvent;
    EXPECT_CALL(*m_mockAuthDelegate, getAuthToken())
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Return(std::string(AUTH_TOKEN)));
    EXPECT_CALL(*m_mockAddressBookServiceInterface, getEntries(testing::_, testing::_))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Invoke(
            [&waitEvent](
                const std::string& id,
                std::weak_ptr<aace::addressBook::AddressBook::IAddressBookEntriesFactory> factory) -> bool {
                if (auto sharedRef = factory.lock()) {
                    // clang-format off
                    //1
                    EXPECT_TRUE( sharedRef->addPostalAddress("001", "Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5)); 
                    EXPECT_TRUE( sharedRef->addPostalAddress("001", "Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5));
                    EXPECT_TRUE( sharedRef->addPostalAddress("001", "Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5)); 
                    EXPECT_TRUE( sharedRef->addPostalAddress("001", "Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5));
                    EXPECT_TRUE( sharedRef->addPostalAddress("001", "Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5)); 
                    EXPECT_TRUE( sharedRef->addPostalAddress("001", "Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5));
                    EXPECT_TRUE( sharedRef->addPostalAddress("001", "Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5)); 
                    EXPECT_TRUE( sharedRef->addPostalAddress("001", "Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5));
                    EXPECT_TRUE( sharedRef->addPostalAddress("001", "Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5)); 
                    EXPECT_TRUE( sharedRef->addPostalAddress("001", "Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5));
                    //11
                    EXPECT_TRUE( sharedRef->addPostalAddress("001", "Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5)); 
                    EXPECT_TRUE( sharedRef->addPostalAddress("001", "Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5));
                    EXPECT_TRUE( sharedRef->addPostalAddress("001", "Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5)); 
                    EXPECT_TRUE( sharedRef->addPostalAddress("001", "Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5));
                    EXPECT_TRUE( sharedRef->addPostalAddress("001", "Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5)); 
                    EXPECT_TRUE( sharedRef->addPostalAddress("001", "Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5));
                    EXPECT_TRUE( sharedRef->addPostalAddress("001", "Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5)); 
                    EXPECT_TRUE( sharedRef->addPostalAddress("001", "Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5));
                    EXPECT_TRUE( sharedRef->addPostalAddress("001", "Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5)); 
                    EXPECT_TRUE( sharedRef->addPostalAddress("001", "Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5));
                    //21
                    EXPECT_TRUE( sharedRef->addPostalAddress("001", "Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5)); 
                    EXPECT_TRUE( sharedRef->addPostalAddress("001", "Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5));
                    EXPECT_TRUE( sharedRef->addPostalAddress("001", "Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5)); 
                    EXPECT_TRUE( sharedRef->addPostalAddress("001", "Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5));
                    EXPECT_TRUE( sharedRef->addPostalAddress("001", "Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5)); 
                    EXPECT_TRUE( sharedRef->addPostalAddress("001", "Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5));
                    EXPECT_TRUE( sharedRef->addPostalAddress("001", "Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5)); 
                    EXPECT_TRUE( sharedRef->addPostalAddress("001", "Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5));
                    EXPECT_TRUE( sharedRef->addPostalAddress("001", "Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5)); 
                    EXPECT_TRUE( sharedRef->addPostalAddress("001", "Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5));
                    //31
                    EXPECT_FALSE( sharedRef->addPostalAddress("001", "Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5));
                    // clang-format on
                }
                waitEvent.wakeUp();
                return true;
            }));

    m_addressBookCloudUploader->onNetworkInfoChanged(aace::network::NetworkInfoProvider::NetworkStatus::CONNECTED, 123);
    m_addressBookCloudUploader->onAuthStateChange(
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::State::REFRESHED,
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::Error::SUCCESS);

    m_addressBookCloudUploader->addressBookAdded(m_mockNavigationAddressBook);

    EXPECT_TRUE(waitEvent.wait(TIMEOUT));
}

TEST_F(AddressBookCloudUploaderTest, AddingMaxAllowedPostalAddressForSameEntryIdShouldFail) {
    alexaClientSDK::avsCommon::utils::WaitEvent waitEvent;
    EXPECT_CALL(*m_mockAuthDelegate, getAuthToken())
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Return(std::string(AUTH_TOKEN)));
    EXPECT_CALL(*m_mockAddressBookServiceInterface, getEntries(testing::_, testing::_))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Invoke(
            [&waitEvent](
                const std::string& id,
                std::weak_ptr<aace::addressBook::AddressBook::IAddressBookEntriesFactory> factory) -> bool {
                if (auto sharedRef = factory.lock()) {
                    // clang-format off
                    EXPECT_FALSE(sharedRef->addEntry(buildEntryPayloadWithNameAndPostalAddresses("001", "firstName", "lastName", "",
                        {
                            {"Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5}, // 1
                            {"Work", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5},
                            {"Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5}, 
                            {"Work", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5},
                            {"Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5}, 
                            {"Work", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5},
                            {"Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5}, 
                            {"Work", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5},
                            {"Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5}, 
                            {"Work", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5},
                            {"Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5}, // 11
                            {"Work", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5},
                            {"Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5}, 
                            {"Work", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5},
                            {"Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5}, 
                            {"Work", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5},
                            {"Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5}, 
                            {"Work", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5},
                            {"Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5}, 
                            {"Work", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5},
                            {"Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5}, // 21
                            {"Work", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5},
                            {"Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5}, 
                            {"Work", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5},
                            {"Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5}, 
                            {"Work", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5},
                            {"Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5}, 
                            {"Work", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5},
                            {"Home", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5}, 
                            {"Work", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5},
                            {"Work", "123 Main Street", "", "", "Santa Clara", "California", "", "95001", "US", 90, 180, 5}, // 31
                        }  
                    )));
                    // clang-format on
                }
                waitEvent.wakeUp();
                return true;
            }));

    m_addressBookCloudUploader->onNetworkInfoChanged(aace::network::NetworkInfoProvider::NetworkStatus::CONNECTED, 123);
    m_addressBookCloudUploader->onAuthStateChange(
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::State::REFRESHED,
        alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::Error::SUCCESS);

    m_addressBookCloudUploader->addressBookAdded(m_mockNavigationAddressBook);

    EXPECT_TRUE(waitEvent.wait(TIMEOUT));
}

}  // namespace addressBook
}  // namespace unit
}  // namespace test
}  // namespace aace
