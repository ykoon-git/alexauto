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

#ifndef AACE_JNI_BLUETOOTH_BLUETOOTH_SERVER_SOCKET_H
#define AACE_JNI_BLUETOOTH_BLUETOOTH_SERVER_SOCKET_H

#include <AACE/Bluetooth/BluetoothServerSocket.h>
#include <AACE/JNI/Core/PlatformInterfaceBinder.h>

namespace aace {
namespace jni {
namespace bluetooth {

class BluetoothServerSocketBinder : public aace::bluetooth::BluetoothServerSocket {
public:
    BluetoothServerSocketBinder(jobject obj);

    // aace::bluetooth::BluetoothServerSocket
    std::shared_ptr<aace::bluetooth::BluetoothSocket> accept() override;
    void close() override;

private:
    JObject m_obj;
};

}  // namespace bluetooth
}  // namespace jni
}  // namespace aace

#endif
