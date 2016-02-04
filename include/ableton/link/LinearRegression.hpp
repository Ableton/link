// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <numeric>
#include <cfloat>
#include <cmath>
#include <utility>

namespace ableton
{
namespace link
{

template <typename It>
std::pair<double, double> linearRegression(It begin, It end)
{
  using namespace std;
  using Point = pair<double, double>;

  const double numPoints = static_cast<double>(distance(begin, end));

  const double meanX = accumulate(begin, end, 0.0,
    [](double a, Point b) { return a + b.first; }) / numPoints;

  const double productXX = accumulate(begin, end, 0.0,
    [&meanX](double a, Point b) { return a + pow(b.first - meanX, 2.0); });

  const double meanY =  accumulate(begin, end, 0.0,
    [](double a, Point b) { return a + b.second; }) / numPoints;

  const double productXY = inner_product(begin, end, begin, 0.0,
    [](double a, double b) { return a + b; },
    [&meanX, &meanY](Point a, Point b)
      { return ((a.first - meanX) * (b.second - meanY)); });

  const double slope = productXX == 0.0
    ? 0.0
    : productXY / productXX;

  const double intercept = meanY - (slope * meanX);

  return make_pair(slope, intercept);
}


} // namespace link
} // namespace ableton
