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

#ifndef AACE_ENGINE_BLUETOOTH_GATT_SERVER_INTERFACE_H
#define AACE_ENGINE_BLUETOOTH_GATT_SERVER_INTERFACE_H

#include <string>
#include <memory>
#include <istream>

#include <AACE/Bluetooth/ByteArray.h>

namespace aace {
namespace engine {
namespace bluetooth {

class GATTServerInterface {
public:
    virtual ~GATTServerInterface() = default;

    virtual bool setCharacteristicValue(
        const std::string& serviceId,
        const std::string& characteristicId,
        aace::bluetooth::ByteArrayPtr data) = 0;
};

}  // namespace bluetooth
}  // namespace engine
}  // namespace aace

#endif
