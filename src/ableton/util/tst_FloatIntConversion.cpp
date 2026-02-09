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


#include <ableton/test/CatchWrapper.hpp>
#include <ableton/util/FloatIntConversion.hpp>

namespace ableton
{
namespace util
{

TEST_CASE("FloatToInt")
{
  REQUIRE(0 == floatToInt16(0.f));
  REQUIRE(floatToInt16(-0.5f) == -1 * floatToInt16(0.5f));
  REQUIRE(std::numeric_limits<int16_t>::max() == floatToInt16(1.0f));
  REQUIRE(std::numeric_limits<int16_t>::min() == floatToInt16(-1.0f));
  REQUIRE(std::numeric_limits<int16_t>::max() == floatToInt16(2.0f));
  REQUIRE(std::numeric_limits<int16_t>::min() == floatToInt16(-2.0f));
}

TEST_CASE("DoubleToInt")
{
  REQUIRE(0 == floatToInt16(0.0));
  REQUIRE(floatToInt16(-0.5) == -1 * floatToInt16(0.5));
  REQUIRE(std::numeric_limits<int16_t>::max() == floatToInt16(1.0));
  REQUIRE(std::numeric_limits<int16_t>::min() == floatToInt16(-1.0));
  REQUIRE(std::numeric_limits<int16_t>::max() == floatToInt16(2.0));
  REQUIRE(std::numeric_limits<int16_t>::min() == floatToInt16(-2.0));
}

TEST_CASE("IntToFloat")
{
  REQUIRE(Approx(0.0f) == int16ToFloat<float>(0));
  REQUIRE(Approx(-0.5f) == int16ToFloat<float>(-16384));
  REQUIRE(Approx(0.5f) == int16ToFloat<float>(16384));
  REQUIRE(Approx(1.0f - 1.0f / 32768.0f)
          == int16ToFloat<float>(std::numeric_limits<int16_t>::max()));
  REQUIRE(Approx(-1.0f) == int16ToFloat<float>(std::numeric_limits<int16_t>::min()));
}

TEST_CASE("IntToDouble")
{
  REQUIRE(Approx(0.0) == int16ToFloat<double>(0));
  REQUIRE(Approx(-0.5) == int16ToFloat<double>(-16384));
  REQUIRE(Approx(0.5) == int16ToFloat<double>(16384));
  REQUIRE(Approx(1.0 - 1.0 / 32768.0)
          == int16ToFloat<double>(std::numeric_limits<int16_t>::max()));
  REQUIRE(Approx(-1.0) == int16ToFloat<double>(std::numeric_limits<int16_t>::min()));
}

} // namespace util
} // namespace ableton
