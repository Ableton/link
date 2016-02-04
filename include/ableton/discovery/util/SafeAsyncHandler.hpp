// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <memory>

namespace ableton
{
namespace util
{

// A utility handler for passing to async functions that may call the
// handler past the lifetime of the wrapped delegate object.
// The need for this is particularly driven by boost::asio timer
// objects, which explicitly document that they may be called without
// an error code after they have been cancelled. This has led to
// several crashes. This handler wrapper implements a useful idiom for
// avoiding this problem.

template <typename Delegate>
struct SafeAsyncHandler
{
  SafeAsyncHandler(const std::shared_ptr<Delegate>& pDelegate)
    : mpDelegate(pDelegate)
  {
  }

  template <typename... T>
  void operator()(T&&... t) const
  {
    std::shared_ptr<Delegate> pDelegate = mpDelegate.lock();
    if (pDelegate)
    {
      (*pDelegate)(std::forward<T>(t)...);
    }
  }

  std::weak_ptr<Delegate> mpDelegate;
};

// Factory function for easily wrapping a shared_ptr to a handler
template <typename Delegate>
SafeAsyncHandler<Delegate> makeAsyncSafe(const std::shared_ptr<Delegate>& pDelegate)
{
  return {pDelegate};
}

} // namespace util
} // namespace ableton
