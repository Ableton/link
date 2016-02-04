// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <ableton/discovery/asio/AsioWrapper.hpp>
#include <ableton/discovery/asio/AsioTimer.hpp>
#include <thread>

namespace ableton
{
namespace util
{

struct AsioService
{
  using Timer = util::AsioTimer;

  AsioService()
    : AsioService(DefaultHandler{})
  {
  }

  template <typename ExceptionHandler>
  explicit AsioService(ExceptionHandler exceptHandler)
    : mpWork(new asio::io_service::work(mService))
  {
    mThread =
      std::thread{[](asio::io_service& service, ExceptionHandler handler) {
        for (;;)
        {
          try
          {
            service.run();
            break;
          }
          catch(const typename ExceptionHandler::Exception& exception)
          {
            handler(exception);
          }
        }
      }, std::ref(mService), std::move(exceptHandler)};
  }

  ~AsioService()
  {
    mpWork.reset();
    mThread.join();
  }

  ableton::util::AsioTimer makeTimer()
  {
    return {mService};
  }

  template <typename Handler>
  void post(Handler handler)
  {
    mService.post(std::move(handler));
  }

  asio::io_service mService;

private:

  // Default handler is hidden and defines a hidden exception type
  // that will never be thrown by other code, so it effectively does
  // not catch.
  struct DefaultHandler
  {
    struct Exception { };

    void operator()(const Exception&)
    {
    }
  };

  std::unique_ptr<asio::io_service::work> mpWork;
  std::thread mThread;
};

} // namespace util
} // namespace ableton
