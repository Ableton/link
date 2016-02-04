// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#include <ableton/link/LinearRegression.hpp>
#include <ableton/test/CatchWrapper.hpp>
#include <array>
#include <vector>

namespace ableton
{
namespace link
{

using Vector = std::vector<std::pair<double, double>>;

TEST_CASE("LinearRegression | EmptyVector", "[LinearRegression]")
{
  Vector data;
  const auto result =
    linearRegression(data.begin(), data.end());
  CHECK(0 == result.first);
}

TEST_CASE("LinearRegression | OnePoint", "[LinearRegression]")
{
  using Array = std::array<std::pair<double, double>, 1>;
  Array data;
  data[0] = {};
  const auto result =
    linearRegression(data.begin(), data.end());
  CHECK(0 == result.first);
  CHECK(0 == result.second);
}

TEST_CASE("LinearRegression | TwoPoints", "[LinearRegression]")
{
  Vector data;
  data.emplace_back(0.0, 0.0);
  data.emplace_back(666666.6, 66666.6);

  const auto result = linearRegression(data.begin(), data.end());
  CHECK(0.001 > fabs(0.1 - result.first));
  CHECK(0.0 == result.second);
}

TEST_CASE("LinearRegression | 10000Points", "[LinearRegression]")
{
  Vector data;
  const double slope = -0.2;
  const double intercept = -357.53456;

  for (int i = 1; i < 10000; ++i)
  {
    data.emplace_back(i, i * slope + intercept);
  }

  const auto result = linearRegression(data.begin(), data.end());
  CHECK(1.0e-6 > fabs(slope - result.first));
  CHECK(1.0e-6 > fabs(intercept - result.second));
}

} // namespace link
} // namespace ableton
