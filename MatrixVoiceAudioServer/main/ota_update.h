//
//  ota_update.h
//  esp32-ota
//
//  Updating the firmware over the air.
//
//  Created by Andreas Schweizer on 25.11.2016.
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

#ifndef __OTA_UPDATE_H__
#define __OTA_UPDATE_H__ 1

typedef enum {
    OTA_OK = 0,
    OTA_ERR_PARTITION_NOT_FOUND = 1,
    OTA_ERR_PARTITION_NOT_ACTIVATED = 2,
    OTA_ERR_BEGIN_FAILED = 3,
    OTA_ERR_WRITE_FAILED = 4,
    OTA_ERR_END_FAILED = 5,
} TOtaResult;

// Call this function once at the beginning to configure this module.
void otaUpdateInit();

// Call to check if an OTA update is ongoing.
int otaUpdateInProgress();

// Start an OTA update.
TOtaResult otaUpdateBegin();

// Call this function for every line with up to 4 kBytes of hex data.
TOtaResult otaUpdateWriteHexData(const char *hexData, int len);

// Finish an OTA update.
TOtaResult otaUpdateEnd();

void otaDumpInformation();

#endif // __OTA_UPDATE_H__
