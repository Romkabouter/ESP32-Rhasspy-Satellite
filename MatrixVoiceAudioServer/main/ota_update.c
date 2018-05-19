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

#include <stdio.h>
#include <string.h>
#include <esp_spi_flash.h>
#include <esp_partition.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_ota_ops.h"
#include "esp_log.h"
#include "ota_update.h"


// Partition for the OTA update.
static const esp_partition_t *sOtaPartition;

// SPI flash address for next write operation.
static uint32_t sFlashCurrentAddress;

static esp_ota_handle_t sOtaHandle;


static const esp_partition_t *otaUpdateFindNextBootPartition();
static void otaDump128bytes(uint32_t addr, uint8_t *p);
static void otaDump16bytes(uint32_t addr, uint8_t *p);


void otaUpdateInit()
{
    spi_flash_init();
}

int otaUpdateInProgress()
{
    return sOtaPartition ? 1 : 0;
}

TOtaResult otaUpdateBegin()
{
    sOtaPartition = otaUpdateFindNextBootPartition();
    if (!sOtaPartition) {
        return OTA_ERR_PARTITION_NOT_FOUND;
    }

    sFlashCurrentAddress = sOtaPartition->address;
    ESP_LOGI("ota_update", "Set start address for flash writes to 0x%08x", sFlashCurrentAddress);
    
    // TODO
    // This operation would trigger the watchdog of the currently running task if we fed it with the full partition size.
    // To avoid the issue, we erase only a small part here and afterwards erase every page before writing to it.
    esp_err_t result = esp_ota_begin(sOtaPartition, 4096, &sOtaHandle);
    ESP_LOGI("ota_update", "Result from esp_ota_begin: %d %d", result, sOtaHandle);
    if (result == ESP_OK) {
        return OTA_OK;
    }
    
    return OTA_ERR_BEGIN_FAILED;
}

TOtaResult otaUpdateEnd()
{
    if (!sOtaPartition) {
        return OTA_ERR_PARTITION_NOT_FOUND;
    }

    esp_err_t result = esp_ota_end(sOtaHandle);
    if (result != ESP_OK) {
        return OTA_ERR_END_FAILED;
    }

    result = esp_ota_set_boot_partition(sOtaPartition);
    if (result == ESP_OK) {
        ESP_LOGI("ota_update", "Boot partition activated: %s", sOtaPartition->label);
        return OTA_OK;
    }
    
    ESP_LOGE("ota_update", "Failed to activate boot partition %s, error %d", sOtaPartition->label, result);
    
    sOtaPartition = NULL;
    return OTA_ERR_PARTITION_NOT_ACTIVATED;
}

TOtaResult otaUpdateWriteHexData(const char *hexData, int len)
{
    uint8_t buf[4096];
    
    for (int i = 0; i < 4096; i++) {
        buf[i] = (i < len) ? hexData[i] : 0xff;
    }

    // Erase flash pages at 4k boundaries.
    if (sFlashCurrentAddress % 0x1000 == 0) {
        int flashSectorToErase = sFlashCurrentAddress / 0x1000;
        ESP_LOGI("ota_update", "Erasing flash sector %d", flashSectorToErase);
        spi_flash_erase_sector(flashSectorToErase);
    }

    // Write data into flash memory.
    ESP_LOGI("ota_update", "Writing flash at 0x%08x...", sFlashCurrentAddress);
    // esp_err_t result = spi_flash_write(sFlashCurrentAddress, buf, 4096);
    esp_err_t result = esp_ota_write(sOtaHandle, buf, len);
    if (result != ESP_OK) {
        ESP_LOGE("ota_update", "Failed to write flash at address 0x%08x, error %d", sFlashCurrentAddress, result);
        return OTA_ERR_WRITE_FAILED;
    }

    sFlashCurrentAddress += len;
    return OTA_OK;
}

void otaDumpInformation()
{
    uint8_t buf[4096], buf2[4096];
    esp_err_t result;
    
    ESP_LOGI("ota_update", "otaDumpInformation");
    
    size_t chipSize = spi_flash_get_chip_size();
    ESP_LOGI("ota_update", "flash chip size = %d", chipSize);
    
    ESP_LOGI("ota_update", "Reading flash at 0x00000000....");
    result = spi_flash_read(0, buf, 4096);
    ESP_LOGI("ota_update", "Result = %d", result);
    otaDump128bytes(0, buf);
    
    ESP_LOGI("ota_update", "Reading flash at 0x00001000....");
    result = spi_flash_read(0x1000, buf, 4096);
    ESP_LOGI("ota_update", "Result = %d", result);
    otaDump128bytes(0x1000, buf);
    
    ESP_LOGI("ota_update", "Reading flash at 0x00004000....");
    result = spi_flash_read(0x4000, buf, 4096);
    ESP_LOGI("ota_update", "Result = %d", result);
    otaDump128bytes(0x4000, buf);
    
    ESP_LOGI("ota_update", "Reading flash at 0x0000D000....");
    result = spi_flash_read(0xD000, buf, 4096);
    otaDump128bytes(0xD000, buf);
    result = spi_flash_read(0xE000, buf, 4096);
    ESP_LOGI("ota_update", "Result = %d", result);
    otaDump128bytes(0xE000, buf);
    
    ESP_LOGI("ota_update", "Reading flash at 0x00010000....");
    result = spi_flash_read(0x10000, buf, 4096);
    ESP_LOGI("ota_update", "Result = %d", result);
    otaDump128bytes(0x10000, buf);
    
    ESP_LOGI("ota_update", "Reading flash at 0x00020000....");
    result = spi_flash_read(0x20000, buf2, 4096);
    ESP_LOGI("ota_update", "Result = %d", result);
    otaDump128bytes(0x20000, buf2);
    
    ESP_LOGI("ota_update", "Reading flash at 0x00110000....");
    result = spi_flash_read(0x110000, buf2, 4096);
    ESP_LOGI("ota_update", "Result = %d", result);
    otaDump128bytes(0x110000, buf2);
    
    ESP_LOGI("ota_update", "Reading flash at 0x00210000....");
    result = spi_flash_read(0x210000, buf2, 4096);
    ESP_LOGI("ota_update", "Result = %d", result);
    otaDump128bytes(0x210000, buf2);
}


static const esp_partition_t *otaUpdateFindNextBootPartition()
{
    // Factory -> OTA_0
    // OTA_0   -> OTA_1
    // OTA_1   -> OTA_0
    
    const esp_partition_t *currentBootPartition = esp_ota_get_boot_partition();
    const esp_partition_t *nextBootPartition = NULL;
    
    if (!strcmp("factory", currentBootPartition->label)) {
        nextBootPartition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, "ota_0");
    }

    if (!strcmp("ota_0", currentBootPartition->label)) {
        nextBootPartition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, "ota_1");
    }
    
    if (!strcmp("ota_1", currentBootPartition->label)) {
        nextBootPartition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, "ota_0");
    }
    
    if (nextBootPartition) {
        ESP_LOGI("ota_update", "Found next boot partition: %02x %02x 0x%08x %s",
                 nextBootPartition->type, nextBootPartition->subtype, nextBootPartition->address, nextBootPartition->label);
    } else {
        ESP_LOGE("ota_update", "Failed to determine next boot partition from current boot partition: %s",
                 currentBootPartition ? currentBootPartition->label : "NULL");
    }
    
    return nextBootPartition;
}

static void otaDump128bytes(uint32_t addr, uint8_t *p)
{
    for (int i = 0; i < 8; i++) {
        uint32_t addr2 = addr + 16 * i;
        otaDump16bytes(addr2, &p[16*i]);
    }
}

static void otaDump16bytes(uint32_t addr, uint8_t *p)
{
    ESP_LOGI("ota_update", "%08X : %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
             addr, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
}
