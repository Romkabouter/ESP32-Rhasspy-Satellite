/*
 * Copyright 2017 <Admobilize>
 * MATRIX Labs  [http://creator.matrix.one]
 * This file is part of MATRIX Voice HAL for ESP32
 *
 * Author:
 *       Andrés Calderón <andres.calderon@admobilize.com>
 *
 * MATRIX Voice ESP32 HAL is free software: you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef CPP_DRIVER_WISHBONE_BUS_H_
#define CPP_DRIVER_WISHBONE_BUS_H_

#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <esp_err.h>
#include <freertos/task.h>
#include <cmath>

#include <stdint.h>
#include <string>

namespace matrix_hal {

struct hardware_address {
  uint8_t readnwrite : 1;
  uint16_t reg : 15;
};

class WishboneBus {
 public:
  WishboneBus() {}

  esp_err_t Init();

  esp_err_t RegWrite16(uint16_t add, uint16_t data);
  esp_err_t RegRead16(uint16_t add, uint16_t* data);

  esp_err_t SpiRead(uint16_t add, uint8_t* data, int length);
  esp_err_t SpiWrite(uint16_t add, const uint8_t* data, int length);

  uint32_t FPGAFrequency() { return 50000000; }

 private:
  esp_err_t SpiTransfer(uint8_t* send_buffer, uint8_t* receive_buffer,
                        uint32_t size);

 private:
  spi_device_handle_t spi_;
};
};      // namespace matrix_hal
#endif  // CPP_DRIVER_WISHBONE_BUS_H_
