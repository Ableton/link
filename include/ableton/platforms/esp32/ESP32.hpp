/* Copyright 2019, Mathias Bredholt, Torso Electronics, Copenhagen. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <cstdint>

inline uint64_t __bswap64(uint64_t _x)
{
  return ((uint64_t)((_x >> 56) | ((_x >> 40) & 0xff00) | ((_x >> 24) & 0xff0000)
                     | ((_x >> 8) & 0xff000000) | ((_x << 8) & ((uint64_t)0xff << 32))
                     | ((_x << 24) & ((uint64_t)0xff << 40))
                     | ((_x << 40) & ((uint64_t)0xff << 48)) | ((_x << 56))));
}

#ifndef ntohll
#define ntohll(x) __bswap64(x)
#endif

#ifndef htonll
#define htonll(x) __bswap64(x)
#endif
