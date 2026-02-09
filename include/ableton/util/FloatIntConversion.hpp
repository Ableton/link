/* Copyright 2025, Ableton AG, Berlin. All rights reserved.
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
 *
 *  If you would like to incorporate Link into a proprietary software application,
 *  please contact <link-devs@ableton.com>.
 */


#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace ableton
{
namespace util
{

template <typename T>
int16_t floatToInt16(const T src)
{
  static_assert(std::is_same<T, float>::value || std::is_same<T, double>::value);

  constexpr auto kScale = std::numeric_limits<int16_t>::max() + 1;
  constexpr auto kMin = T{-1};
  constexpr auto kMax = T{1} - T{1} / kScale;
  return int16_t(std::round(kScale * std::clamp(src, kMin, kMax)));
}

template <typename T>
T int16ToFloat(const int16_t src)
{
  static_assert(std::is_same<T, float>::value || std::is_same<T, double>::value);

  constexpr auto kScale = std::numeric_limits<int16_t>::max() + 1;
  return static_cast<T>(src) / kScale;
}

} // namespace util
} // namespace ableton
