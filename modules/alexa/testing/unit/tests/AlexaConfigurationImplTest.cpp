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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string>
#include <iostream>

#include <AACE/Alexa/AlexaConfiguration.h>
#include <AACE/Engine/Core/EngineMacros.h>
#include <AACE/Engine/Utils/JSON/JSON.h>

namespace json = aace::engine::utils::json;

class AlexaConfigurationImplTest : public ::testing::Test {};

TEST_F(AlexaConfigurationImplTest, createAlexaClientInfoConfigBestCase) {
    // clang-format off
    json::Value expected = {
        {"aace.alexa",{
            {"alexaClientInfo",{
                {"clientId","CLIENT_ID"},
                {"productId","PRODUCT_ID"},
                {"amazonId","AMAZON_ID"}
            }}
        }}
    };
    // clang-format on

    auto config =
        aace::alexa::config::AlexaConfiguration::createAlexaClientInfoConfig("CLIENT_ID", "PRODUCT_ID", "AMAZON_ID");

    // compare config stream with expected string value
    std::ostringstream configStr;
    configStr << config->getStream()->rdbuf();
    EXPECT_EQ(true, expected == json::toJson(configStr.str())) << "Error in the Configuration String";
}

TEST_F(AlexaConfigurationImplTest, createNotificationsConfigBestCase) {
    std::string expectedConfigStr =
        "{\n"
        "    \"aace.alexa\": {\n"
        "        \"avsDeviceSDK\": {\n"
        "            \"notifications\": {\n"
        "                \"databaseFilePath\": \"DATABASE_PATH\"\n"
        "            }\n"
        "        }\n"
        "    }\n"
        "}";

    auto config = aace::alexa::config::AlexaConfiguration::createNotificationsConfig("DATABASE_PATH");

    //Convert to ostringstream for comparing the istream
    std::ostringstream configStr;
    configStr << config->getStream()->rdbuf();
    EXPECT_EQ(configStr.str(), expectedConfigStr) << "Error in the Configuration String";
}

TEST_F(AlexaConfigurationImplTest, createCertifiedSenderConfigBestCase) {
    std::string expectedConfigStr =
        "{\n"
        "    \"aace.alexa\": {\n"
        "        \"avsDeviceSDK\": {\n"
        "            \"certifiedSender\": {\n"
        "                \"databaseFilePath\": \"DATABASE_PATH\"\n"
        "            }\n"
        "        }\n"
        "    }\n"
        "}";

    auto config = aace::alexa::config::AlexaConfiguration::createCertifiedSenderConfig("DATABASE_PATH");

    //Convert to ostringstream for comparing the istream
    std::ostringstream configStr;
    configStr << config->getStream()->rdbuf();
    EXPECT_EQ(configStr.str(), expectedConfigStr) << "Error in the Configuration String";
}

TEST_F(AlexaConfigurationImplTest, createCapabilitiesDelegateConfigBestCase) {
    std::string expectedConfigStr =
        "{\n"
        "    \"aace.alexa\": {\n"
        "        \"avsDeviceSDK\": {\n"
        "            \"capabilitiesDelegate\": {\n"
        "                \"databaseFilePath\": \"DATABASE_PATH\"\n"
        "            }\n"
        "        }\n"
        "    }\n"
        "}";

    auto config = aace::alexa::config::AlexaConfiguration::createCapabilitiesDelegateConfig("DATABASE_PATH");

    //Convert to ostringstream for comparing the istream
    std::ostringstream configStr;
    configStr << config->getStream()->rdbuf();
    EXPECT_EQ(configStr.str(), expectedConfigStr) << "Error in the Configuration String";
}

TEST_F(AlexaConfigurationImplTest, createCurlConfigBestCase) {
    std::string expectedConfigStr =
        "{\n"
        "    \"aace.alexa\": {\n"
        "        \"avsDeviceSDK\": {\n"
        "            \"libcurlUtils\": {\n"
        "                \"CURLOPT_CAPATH\": \"CAPATH\"\n"
        "            }\n"
        "        }\n"
        "    }\n"
        "}";

    auto config = aace::alexa::config::AlexaConfiguration::createCurlConfig("CAPATH");

    //Convert to ostringstream for comparing the istream
    std::ostringstream configStr;
    configStr << config->getStream()->rdbuf();
    EXPECT_EQ(configStr.str(), expectedConfigStr) << "Error in the Configuration String";
}

TEST_F(AlexaConfigurationImplTest, createCurlConfigWithNetworkInterface) {
    std::string expectedConfigStr =
        "{\n"
        "    \"aace.alexa\": {\n"
        "        \"avsDeviceSDK\": {\n"
        "            \"libcurlUtils\": {\n"
        "                \"CURLOPT_CAPATH\": \"CAPATH\",\n"
        "                \"CURLOPT_INTERFACE\": \"INTERFACE\"\n"
        "            }\n"
        "        }\n"
        "    }\n"
        "}";

    auto config = aace::alexa::config::AlexaConfiguration::createCurlConfig("CAPATH", "INTERFACE");

    //Convert to ostringstream for comparing the istream
    std::ostringstream configStr;
    configStr << config->getStream()->rdbuf();
    EXPECT_EQ(configStr.str(), expectedConfigStr) << "Error in the Configuration String";
}

TEST_F(AlexaConfigurationImplTest, createCurlConfigWithEmptyStringNetworkInterface) {
    std::string expectedConfigStr =
        "{\n"
        "    \"aace.alexa\": {\n"
        "        \"avsDeviceSDK\": {\n"
        "            \"libcurlUtils\": {\n"
        "                \"CURLOPT_CAPATH\": \"CAPATH\"\n"
        "            }\n"
        "        }\n"
        "    }\n"
        "}";

    auto config = aace::alexa::config::AlexaConfiguration::createCurlConfig("CAPATH", "");

    //Convert to ostringstream for comparing the istream
    std::ostringstream configStr;
    configStr << config->getStream()->rdbuf();
    EXPECT_EQ(configStr.str(), expectedConfigStr) << "Error in the Configuration String";
}

TEST_F(AlexaConfigurationImplTest, createCurlConfigWithProxy) {
    std::string expectedConfigStr =
        "{\n"
        "    \"aace.alexa\": {\n"
        "        \"avsDeviceSDK\": {\n"
        "            \"libcurlUtils\": {\n"
        "                \"CURLOPT_CAPATH\": \"CAPATH\",\n"
        "                \"CURLOPT_PROXY\": \"PROXY\"\n"
        "            }\n"
        "        }\n"
        "    }\n"
        "}";

    auto config = aace::alexa::config::AlexaConfiguration::createCurlConfig("CAPATH", "", "PROXY");

    //Convert to ostringstream for comparing the istream
    std::ostringstream configStr;
    configStr << config->getStream()->rdbuf();
    EXPECT_EQ(configStr.str(), expectedConfigStr) << "Error in the Configuration String";
}

TEST_F(AlexaConfigurationImplTest, createSettingsConfigBestCase) {
    std::string expectedConfigStr =
        "{\n"
        "    \"aace.alexa\": {\n"
        "        \"avsDeviceSDK\": {\n"
        "            \"deviceSettings\": {\n"
        "                \"locales\": [\n"
        "                    \"LOCALE\"\n"
        "                ],\n"
        "                \"defaultLocale\": \"LOCALE\",\n"
        "                \"defaultTimezone\": \"America/Vancouver\",\n"
        "                \"databaseFilePath\": \"DATABASE_PATH\"\n"
        "            }\n"
        "        }\n"
        "    }\n"
        "}";

    auto config = aace::alexa::config::AlexaConfiguration::createSettingsConfig("DATABASE_PATH", "LOCALE");

    //Convert to ostringstream for comparing the istream
    std::ostringstream configStr;
    configStr << config->getStream()->rdbuf();
    EXPECT_EQ(configStr.str(), expectedConfigStr) << "Error in the Configuration String";
}

TEST_F(AlexaConfigurationImplTest, createSpeakerManagerConfigBestCase) {
    std::string expectedConfigStr =
        "{\n"
        "    \"aace.alexa\": {\n"
        "        \"speakerManager\": {\n"
        "            \"enabled\": true\n"
        "        }\n"
        "    }\n"
        "}";

    auto config = aace::alexa::config::AlexaConfiguration::createSpeakerManagerConfig(true);

    //Convert to ostringstream for comparing the istream
    std::ostringstream configStr;
    configStr << config->getStream()->rdbuf();
    EXPECT_EQ(configStr.str(), expectedConfigStr) << "Error in the Configuration String";
}

TEST_F(AlexaConfigurationImplTest, createSystemConfigBestCase) {
    std::string expectedConfigStr =
        "{\n"
        "    \"aace.alexa\": {\n"
        "        \"system\": {\n"
        "            \"firmwareVersion\": 1\n"
        "        }\n"
        "    }\n"
        "}";

    auto config = aace::alexa::config::AlexaConfiguration::createSystemConfig(1);

    //Convert to ostringstream for comparing the istream
    std::ostringstream configStr;
    configStr << config->getStream()->rdbuf();
    EXPECT_EQ(configStr.str(), expectedConfigStr) << "Error in the Configuration String";
}

TEST_F(AlexaConfigurationImplTest, createMiscStorageConfigBestCase) {
    std::string expectedConfigStr =
        "{\n"
        "    \"aace.alexa\": {\n"
        "        \"avsDeviceSDK\": {\n"
        "            \"miscDatabase\": {\n"
        "                \"databaseFilePath\": \"DATABASE_PATH\"\n"
        "            }\n"
        "        }\n"
        "    }\n"
        "}";

    auto config = aace::alexa::config::AlexaConfiguration::createMiscStorageConfig("DATABASE_PATH");

    //Convert to ostringstream for comparing the istream
    std::ostringstream configStr;
    configStr << config->getStream()->rdbuf();
    EXPECT_EQ(configStr.str(), expectedConfigStr) << "Error in the Configuration String";
}

TEST_F(AlexaConfigurationImplTest, createTemplateRuntimeConfigBestCase) {
    using TimeoutType = aace::alexa::config::AlexaConfiguration::TemplateRuntimeTimeoutType;

    std::string expectedConfigStr =
        "{\n"
        "    \"aace.alexa\": {\n"
        "        \"avsDeviceSDK\": {\n"
        "            \"templateRuntimeCapabilityAgent\": {\n"
        "                \"displayCardTTSFinishedTimeout\": 1000,\n"
        "                \"displayCardAudioPlaybackFinishedTimeout\": 1000,\n"
        "                \"displayCardAudioPlaybackStoppedPausedTimeout\": 1000\n"
        "            }\n"
        "        }\n"
        "    }\n"
        "}";

    auto config = aace::alexa::config::AlexaConfiguration::createTemplateRuntimeTimeoutConfig(
        {{TimeoutType::DISPLAY_CARD_TTS_FINISHED_TIMEOUT, std::chrono::milliseconds(1000)},
         {TimeoutType::DISPLAY_CARD_AUDIO_PLAYBACK_FINISHED_TIMEOUT, std::chrono::milliseconds(1000)},
         {TimeoutType::DISPLAY_CARD_AUDIO_PLAYBACK_STOPPED_PAUSED_TIMEOUT, std::chrono::milliseconds(1000)}});

    //Convert to ostringstream for comparing the istream
    std::ostringstream configStr;
    configStr << config->getStream()->rdbuf();
    EXPECT_EQ(configStr.str(), expectedConfigStr) << "Error in the Configuration String";
}

TEST_F(AlexaConfigurationImplTest, createTemplateRuntimeConfigWithOneKeyBestCase) {
    using TimeoutType = aace::alexa::config::AlexaConfiguration::TemplateRuntimeTimeoutType;

    std::string expectedConfigStr =
        "{\n"
        "    \"aace.alexa\": {\n"
        "        \"avsDeviceSDK\": {\n"
        "            \"templateRuntimeCapabilityAgent\": {\n"
        "                \"displayCardAudioPlaybackFinishedTimeout\": 1000\n"
        "            }\n"
        "        }\n"
        "    }\n"
        "}";

    auto config = aace::alexa::config::AlexaConfiguration::createTemplateRuntimeTimeoutConfig(
        {{TimeoutType::DISPLAY_CARD_AUDIO_PLAYBACK_FINISHED_TIMEOUT, std::chrono::milliseconds(1000)}});

    //Convert to ostringstream for comparing the istream
    std::ostringstream configStr;
    configStr << config->getStream()->rdbuf();
    EXPECT_EQ(configStr.str(), expectedConfigStr) << "Error in the Configuration String";
}

TEST_F(AlexaConfigurationImplTest, createTemplateRuntimeConfigWithNoKeys) {
    std::string expectedConfigStr =
        "{\n"
        "    \"aace.alexa\": {\n"
        "        \"avsDeviceSDK\": {\n"
        "            \"templateRuntimeCapabilityAgent\": {}\n"
        "        }\n"
        "    }\n"
        "}";

    auto config = aace::alexa::config::AlexaConfiguration::createTemplateRuntimeTimeoutConfig({});

    //Convert to ostringstream for comparing the istream
    std::ostringstream configStr;
    configStr << config->getStream()->rdbuf();
    EXPECT_EQ(configStr.str(), expectedConfigStr) << "Error in the Configuration String";
}

TEST_F(AlexaConfigurationImplTest, createAuthProviderConfigBestCase) {
    std::string expectedConfigStr =
        "{\n"
        "    \"aace.alexa\": {\n"
        "        \"authProvider\": {\n"
        "            \"providers\": [\n"
        "                \"ABC\",\n"
        "                \"ABCD\",\n"
        "                \"TEST\"\n"
        "            ]\n"
        "        }\n"
        "    }\n"
        "}";

    auto config = aace::alexa::config::AlexaConfiguration::createAuthProviderConfig({"ABC", "ABCD", "TEST"});

    //Convert to ostringstream for comparing the istream
    std::ostringstream configStr;
    configStr << config->getStream()->rdbuf();
    EXPECT_EQ(configStr.str(), expectedConfigStr) << "Error in the Configuration String";
}
