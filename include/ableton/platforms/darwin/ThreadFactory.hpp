/* Copyright 2021, Ableton AG, Berlin. All rights reserved.
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

#include <mach/mach.h>
#include <mach/mach_time.h>
#include <pthread.h>
#include <thread>
#include <utility>

namespace ableton
{
namespace platforms
{
namespace darwin
{

struct HighThreadPriority
{
  void operator()() const
  {
    mach_timebase_info_data_t info;
    mach_timebase_info(&info);
    const auto machRatio =
      static_cast<double>(info.denom) / static_cast<double>(info.numer);
    const auto millisecond = machRatio * 1000000;

    struct thread_time_constraint_policy policy;

    // The nominal time interval between the beginnings of two consecutive duty cycles. It
    // defines how often the thread expects to run. A nonzero value specifies the thread's
    // periodicity.
    policy.period = millisecond * 1; // link_audio::MainController's timer period

    // The amount of CPU time the thread needs during each period. This is the actual
    // execution time required per cycle. Needs to be less or equal to the period.
    policy.computation = millisecond * 0.2;

    // The maximum real time that may elapse from the start of a period to the end of
    // computation. This sets an upper bound on the allowed delay for completing the
    // computation. It cannot be less than computation.
    policy.constraint = millisecond * 1;

    // A boolean value indicating whether the thread's computation can be interrupted
    // (preempted) by other threads. If set to 1, the thread can be preempted; if 0, it
    // should run to completion within its constraint.
    policy.preemptible = 1;

    thread_policy_set(mach_thread_self(),
                      THREAD_TIME_CONSTRAINT_POLICY,
                      (thread_policy_t)&policy,
                      THREAD_TIME_CONSTRAINT_POLICY_COUNT);
  }
};

struct ThreadFactory
{
  template <typename Callable, typename... Args>
  static std::thread makeThread(std::string name, Callable&& f, Args&&... args)
  {
    return std::thread{[](std::string name, Callable&& f, Args&&... args)
                       {
                         pthread_setname_np(name.c_str());
                         f(args...);
                       },
                       std::move(name),
                       std::forward<Callable>(f),
                       std::forward<Args>(args)...};
  }
};

} // namespace darwin
} // namespace platforms
} // namespace ableton
