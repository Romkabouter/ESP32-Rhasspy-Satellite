//
//  network.h
//  esp32-ota
//
//  Networking code for OTA demo application
//
//  Created by Andreas Schweizer on 02.12.2016.
//  Copyright © 2016 Classy Code GmbH
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef __NETWORK_H__
#define __NETWORK_H__ 1

// Call this function once at the beginning to configure this module.
void networkInit();

// Try to connect asynchronously to the defined access point.
void networkConnect(const char *ssid, const char *password);

// Check if the module is currently connected to an access point.
int networkIsConnected();
int HotwordDetected();
int mqttIsConnected();

#endif // __NETWORK_H__
