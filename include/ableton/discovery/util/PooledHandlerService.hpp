// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <ableton/discovery/asio/AsioWrapper.hpp>
#include <ableton/discovery/util/Injected.hpp>

namespace ableton
{
namespace util
{

template <
  typename IoService,
  typename Log,
  std::size_t MaxNumHandlers = 32,
  std::size_t MaxHandlerSize = 128>
struct PooledHandlerService
{
  using Timer = typename Injected<IoService>::type::Timer;

  PooledHandlerService(util::Injected<IoService> io, util::Injected<Log> log)
    : mIo(std::move(io))
    , mLog(std::move(log))
  {
    // initialize the handler free store
    mFreeStack.reserve(MaxNumHandlers);
    for (std::size_t i = 0; i < MaxNumHandlers; ++i)
    {
      mFreeStack.push_back(reinterpret_cast<void*>(mRaw + i));
    }
  }

  Timer makeTimer()
  {
    return mIo->makeTimer();
  }

  template <typename Handler>
  void post(Handler handler)
  {
    try
    {
      mIo->post(HandlerWrapper<Handler>{*this, std::move(handler)});
    }
    catch (std::bad_alloc)
    {
      warning(*mLog) << "Handler dropped due to low memory pool";
    }
  }

  template <typename Handler>
  struct HandlerWrapper
  {
    HandlerWrapper(PooledHandlerService& service, Handler handler)
      : mService(service)
      , mHandler(std::move(handler))
    {
    }

    void operator()()
    {
      mHandler();
    }

    // Use pooled allocation so that posting handlers will not cause
    // system allocation
    friend void* asio_handler_allocate(
      const std::size_t size,
      HandlerWrapper* const pHandler)
    {
      if (size > MaxHandlerSize || pHandler->mService.mFreeStack.empty())
      {
        // Going over the max handler size is a programming error, as
        // this is not a dynamically variable value.
        assert(size <= MaxHandlerSize);
        throw std::bad_alloc();
      }
      else
      {
        const auto p = pHandler->mService.mFreeStack.back();
        pHandler->mService.mFreeStack.pop_back();
        return p;
      }
    }

    friend void asio_handler_deallocate(
      void* const p,
      std::size_t,
      HandlerWrapper* const pHandler)
    {
      pHandler->mService.mFreeStack.push_back(p);
    }

    PooledHandlerService& mService;
    Handler mHandler;
  };

  using MemChunk = typename std::aligned_storage<
    MaxHandlerSize, std::alignment_of<std::max_align_t>::value>::type;
  MemChunk mRaw[MaxNumHandlers];
  std::vector<void*> mFreeStack;

  util::Injected<IoService> mIo;
  util::Injected<Log> mLog;
};

} // namespace util
} // namespace ableton
