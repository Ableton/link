/* Copyright 2016, Ableton AG, Berlin. All rights reserved.
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

#define CATCH_CONFIG_RUNNER

#include <ableton/test/CatchWrapper.hpp>
#include <string>

#if defined(LINK_PLATFORM_WINDOWS) && LINK_BUILD_VLD
#include <vld.h>
#endif

namespace
{

const char* pathForXmlOutput(const int argc, const char* const argv[])
{
  const std::string outputArgPrefix = "--gtest_output=xml:";

  for (int i = 1; i < argc; i++)
  {
    if (outputArgPrefix.compare({argv[i], outputArgPrefix.length()}) == 0)
    {
      return argv[i] + outputArgPrefix.length();
    }
  }

  return nullptr;
}

} // anonymous namespace

int main(const int argc, const char* const argv[])
{
  // If google test-style output is requested, re-interpreted the argument call
  const auto pPath = pathForXmlOutput(argc, argv);
  const auto args = pPath == nullptr
                      ? std::vector<const char*>{}
                      : std::vector<const char*>{"-r", "junit", "-o", pPath};

  // Filter out GTest Arguments and remove them from the input arguments
  std::vector<const char*> inArgs(argv, argv + argc);

  inArgs.erase(std::remove_if(inArgs.begin(), inArgs.end(),
                 [](const char* arg) {
                   return std::string{arg}.find("--gtest") != std::string::npos;
                 }),
    inArgs.end());

  inArgs.insert(inArgs.end(), args.begin(), args.end());

  Catch::Session session;
  session.applyCommandLine(int(inArgs.size()), inArgs.data());
  return session.run();
}
