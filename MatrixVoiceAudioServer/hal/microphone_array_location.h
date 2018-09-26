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

#ifndef CPP_DRIVER_MICARRAY_LOCATION_H_
#define CPP_DRIVER_MICARRAY_LOCATION_H_

#include <string>
#include "./matrix_driver.h"

namespace matrix_hal {

/*
  x,y  position in milimeters
 */

static float micarray_location[8][2] = {
    {20.0908795, -48.5036755},  /* M1 */
    {-20.0908795, -48.5036755}, /* M2 */
    {-48.5036755, -20.0908795}, /* M3 */
    {-48.5036755, 20.0908795},  /* M4 */
    {-20.0908795, 48.5036755},  /* M5 */
    {20.0908795, 48.5036755},   /* M6 */
    {48.5036755, 20.0908795},   /* M7 */
    {48.5036755, -20.0908795}   /* M8 */
};

};      // namespace matrix_hal
#endif  // CPP_DRIVER_MICARRAY_LOCATION_H_
