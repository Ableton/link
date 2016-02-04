#define CATCH_CONFIG_RUNNER

#include <ableton/test/CatchWrapper.hpp>
#include <string>

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

} // namespace

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
    [] (const char* arg) {
      return std::string{arg}.find("--gtest") != std::string::npos;
    }), inArgs.end());

  inArgs.insert(inArgs.end(), args.begin(), args.end());

  Catch::Session session;
  session.applyCommandLine(int(inArgs.size()), inArgs.data());
  return session.run();
}
