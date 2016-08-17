# Ableton Link

This is the codebase for Ableton Link, a technology that synchronizes musical beat, tempo,
and phase across multiple applications running on one or more devices. Applications on
devices connected to a local network discover each other automatically and form a musical
session in which each participant can perform independently: anyone can start or stop
while still staying in time. Anyone can change the tempo, the others will follow. Anyone
can join or leave without disrupting the session.

# License

Ableton Link is dual [licensed][license] under GNU GPL version 2 and a proprietary license.

# Building and Running Link Examples

Link relies on `asio-standalone` and `catch` as submodules. After checking out the
main repositories, those submodules have to be loaded using

```
git submodule update --init --recursive
```

Link uses [CMake][cmake] to generate build files for the [Catch][catch]-based
unit-tests and the example applications.

```
$ mkdir build
$ cd build
$ cmake ..
```

In order to build the GUI example application **QLinkHut**, the [Qt][qt] installation
path must be set in the system PATH and `LINK_BUILD_QT_EXAMPLES` must be set:

```
$ cmake -DLINK_BUILD_QT_EXAMPLES=ON ..
```

The output binaries for the example applications and the unit-tests will be placed in a
`bin` subdirectory of the CMake binary directory. Also note that the word size of the Qt
installation must match how Link has been configured. Look for the value of
`LINK_WORD_SIZE` in the CMake output to verify that the word size matches Qt's.

When running QLinkHut on Windows, the Qt binary path must be in the system `PATH` before
launching the executable. So to launch QLinkHut from Visual Studio, one must go to the
QLinkHut Properties -> Debugging -> Environment, and set it to:

```
PATH=$(Path);C:\path\to\Qt\5.5\msvc64_bin\bin
```

# Integrating Link in your Application

## Test Plan

To make sure users have the best possible experience using Link it is important all apps
supporting Link behave consistently. This includes for example playing in sync with other
apps as well as not hijacking a jams tempo when joining. To make sure your app behaves as
intended make sure it complies to the [Test Plan](TEST-PLAN.md).

## Building Link

Link is a header-only library, so it should be straightforward to integrate into your
application.

### CMake-based Projects

If you are using CMake, then you can simply add the following to your CMakeLists.txt file:

```cmake
include($PATH_TO_LINK/AbletonLinkConfig.cmake)
target_link_libraries($YOUR_TARGET Ableton::Link)

```

You can optionally have your build target depend on `${link_HEADERS}`, which will make
the Link headers visible in your IDE. This variable exported to the `PARENT_SCOPE` by
Link's CMakeLists.txt.

### Other Build Systems

If you are not using CMake, then you must do the following:

 - Add the `link/include` directory to your list of include paths
 - Define `LINK_PLATFORM_MACOSX=1`, `LINK_PLATFORM_LINUX=1`, or `LINK_PLATFORM_WINDOWS=1`,
   depending on which platform you are building on.

If you get any compiler errors/warnings, have a look at
[compile-flags.cmake](cmake_include/ConfigireCompileFlags.cmake), which might provide some
insight as to the compiler flags needed to build Link.

### Build Requirements

| Platform | Minimum Required | Optional               |
|----------|------------------|------------------------|
| **All**  | CMake 3.0        | Qt 5.5                 |
| Windows  | MSVC 2013        |                        |
| Mac      | Xcode 7.0        |                        |
| Linux    | Clang 3.6        | libportaudio19-dev     |
| Linux    | GCC 5.2          |                        |

Other compilers with good C++11 support should work, but are not verified.

iOS developers should not use this repo. See http://ableton.github.io/linkkit for
information on the LinkKit SDK for iOS.

# Documentation

The [Link.hpp](include/ableton/Link.hpp) header contains the full Link public interface.
See the LinkHut and QLinkHut projects for an example usage of the `Link` type.

## Link Concepts

Link is different from other approaches to synchronizing electronic instruments that you
may be familiar with. It is not designed to orchestrate multiple instruments so that they
play together in lock-step along a shared timeline. In fact, Link-enabled apps each have
their own independent timelines. The Link library maintains a temporal relationship
between these independent timelines that provides the experience of playing in time
without the timelines being identical.

Playing "in time" is an intentionally vague term that might have different meanings for
different musical contexts and different applications. As a developer, you must decide
the most natural way to map your application's musical concepts onto Link's
synchronization model. For this reason, it's important to gain an intuitive understanding
of how Link synchronizes **tempo**, **beat**, and **phase**.

### Tempo Synchronization

Tempo is a well understood parameter that represents the velocity of a beat timeline with
respect to real time, giving it a unit of beats/time. Tempo synchronization is achieved
when the beat timelines of all participants in a session are advancing at the same rate.

With Link, any participant can propose a change to the session tempo at any time. No
single participant is responsible for maintaining the shared session tempo. Rather, each
participant chooses to adopt the last tempo value that they've seen proposed on the
network. This means that it is possible for participants' tempi to diverge during periods
of tempo modification (especially during simultaneous modification by multiple
participants), but this state is only temporary. The session will converge quickly to a
common tempo after any modification. The Link approach to tempo relies on group
adaptation to changes made by independent, autonomous actors - much like a group of
traditional instrumentalists playing together.

### Beat Alignment

It's conceivable that for certain musical situations, participants would wish to only
synchronize tempo and not other musical parameters. But for the vast majority of cases,
playing with synchronized tempo in the absence of beat alignment would not be perceived
as playing "in time." In this scenario, participants' beat timelines would advance at the
same rate, but the relationship between values on those beat timelines would be undefined.

In most cases, we want to provide a stronger timing property for a session than just
tempo synchronization - we also want beat alignment. When a session is in a state of beat
alignment, an integral value on any participant's beat timeline corresponds to an
integral value on all other participants' beat timelines. This property says nothing
about the magnitude of beat values on each timeline, which can be different, just that
any two timelines must only differ by an integral offset. For example, beat 1 on one
participant's timeline might correspond to beat 3 or beat 4 on another's, but it cannot
correspond to beat 3.5.

Note that in order for a session to maintain a state of beat alignment, it must have
synchronized tempo.

### Phase Synchronization

Beat alignment is a necessary condition for playing "in time" in most circumstances, but
it's often not enough. When working with bars or loops, a user may expect that the beat
position within such a construct (the phase) be synchronized, resulting in alignment of
bar or loop boundaries across participants.

In order to enable the desired bar and loop alignment, an application provides a quantum
value to Link that specifies, in beats, the desired unit of phase synchronization. Link
guarantees that session participants with the same quantum value will be phase aligned,
meaning that if two participants have a 4 beat quantum, beat 3 on one participant's
timeline could correspond to beat 11 on another's, but not beat 12. It also guarantees
the expected relationship between sessions in which one participant has a multiple of
another's quantum. So if one app has an 8-beat loop with a quantum of 8 and another has a
4-beat loop with a quantum of 4, then the beginning of an 8-beat loop will always
correspond to the beginning of a 4-beat loop, whereas a 4-beat loop may align with the
beginning or the middle of an 8-beat loop.

Specifying the quantum value and the handling of phase synchronization is the aspect of
Link integration that leads to the greatest diversity of approaches among developers.
There's no one-size-fits-all recommendation about how to do this, it is very
application-specific. Some applications have a constant quantum that never changes.
Others allow it to change to match a changing value in their app, such as loop length or
time signature. In Ableton Live, it is directly tied to the "Global Quantization"
control, so it may be useful to explore how different values affect the behavior of Live
in order to gain intuition about the quantum.

In order to maintain phase synchronization, the vast majority of Link-enabled
applications (including Live) perform a quantized launch when the user starts transport.
This means that the user sees some sort of count-in animation or flashing play button
until starting at the next quantum boundary. This is a very satisfying interaction
because it allows multiple users on different devices to start exactly together just by
pressing play at roughly the same time. We strongly recommend that developers implement
quantized launching in Link-enabled applications.

## Link API

### Timeline

In Link, a timeline is represented as a triple of `(beat, time, tempo)`, which defines a
bijection between the sets of all beat and time values. Converting between beats and time
is the most basic service that Link provides to integrating applications - an application
will generally want to know what beat value corresponds to a given moment in time. The
timeline implements this and all other timing-related queries and modifications available
to Link clients.

Of course, tempo and beat/time mapping may change over time. A timeline value only
represents a snapshot of the state of the system at a particular moment. Link provides
clients the ability to 'capture' such a snapshot. This is the only mechanism for
obtaining a timeline value.

Once a timeline value is captured, clients may query its properties or modify it by
changing its tempo or its beat/time mapping. Modifications to the captured timeline are
*not* propagated to the Link session automatically - clients must 'commit' the modified
timeline back to Link for it to take effect.

A major benefit of the capture-commit model for timelines is that a captured timeline can
be known to have a consistent value for the duration of a computation in which it is
used. If clients queried timing information from the Link object directly without
capturing a timeline, the results could be inconsistent during the course of a
computation because of asynchronous changes coming from other participants in the Link
session.

### Timelines and Threads

Audio application developers know that the thread that computes and fills audio buffers
has special timing constraints that must be managed carefully. It's usually necessary to
query Link timing data while computing audio, so the Link API provides a realtime-safe
timeline capture/commit function pair. This allows clients to query and modify the Link
timeline directly from the audio callback. It's important that this audio-thread specific
interface *only* be used from the audio thread.

It is also often convenient to be able to access the current Link timeline from the main
thread or other application threads, for example when rendering the current beat time in
a GUI. For this reason, Link also provides a timeline capture/commit function pair to be
used on application threads. These versions must not be used from the audio thread as
they may block.

While it is possible to commit timeline modifications from an application thread, this
should generally be avoided by clients that implement a custom audio callback and use
Link from the audio thread. Because of the real-time constraints in the audio thread,
changes made to the Link timeline from an application thread are processed asynchronously
and are not immediately visible to the audio thread. The same is true for changes made
from the audio thread - the new timeline will eventually be visible to the application
thread, but clients cannot rely on exactly when. It's especially important to take this
into account when combining Link timeline modifications with other cross-thread
communication mechanisms employed by the application. For example, if a timeline is
committed from an application thread and in the next line an atomic flag is set, the
audio thread will almost certainly observe the flag being set before observing the new
timeline value.

In order to avoid these complexities, it is recommended that applications that implement
a custom audio callback only modify the Link timeline from the audio thread. Application
threads may query the timeline but should not modify it. This approach also leads to
better timing accuracy because timeline changes can be specified to occur at buffer
boundaries or even at specific samples, which is not possible from an application thread.

### Time and Clocks

Link works by calculating a relationship between the system clocks of devices in a session.
Since the mechanism for obtaining a system time value and the unit of these values differ
across platforms, Link defines a `Clock` abstraction with platform-specific
implementations. Please see:
- `Link::clock()` method in [Link.hpp](include/ableton/Link.hpp)
- OSX and iOS clock implementation in
[platforms/darwin/Clock.hpp](include/ableton/platforms/darwin/Clock.hpp)
- Windows clock implementation in
[platforms/windows/Clock.hpp](include/ableton/platforms/windows/Clock.hpp)
- C++ standard library `std::chrono::high_resolution_clock`-based implementation in
[platforms/stl/Clock.hpp](include/ableton/platforms/stl/Clock.hpp)

Using the system time correctly in the context of an audio callback gets a little
complicated. Audio devices generally have a sample clock that is independent of the system
Clock. Link maintains a mapping between system time and beat time and therefore can't use
the sample time provided by the audio system directly.

On OSX and iOS, the CoreAudio render callback is passed an `AudioTimeStamp` structure with
a `mHostTime` member that represents the system time at which the audio buffer will be
passed to the audio hardware. This is precisely the information needed to derive the beat
time values corresponding to samples in the buffer using Link. Unfortunately, not all
platforms provide this data to the audio callback.

When a system timestamp is not provided with the audio buffer, the best a client can do in
the audio callback is to get the current system time and filter it based on the provided
sample time. Filtering is necessary because the audio callback will not be invoked at a
perfectly regular interval and therefore the queried system time will exhibit jitter
relative to the sample clock. The Link library provides a
[HostTimeFilter](include/ableton/link/HostTimeFilter.hpp) utility class that performs a
linear regression between system time and sample time in order to improve the accuracy of
system time values used in an audio callback. See the audio callback implementations for
the various [platforms](examples/linkaudio) used in the examples to see how this is used
in practice. Note that for Windows-based systems, we recommend using the [ASIO][asio]
audio driver.

### Latency Compensation

As discussed in the previous section, the system time that a client is provided in an
audio callback either represents the time at which the buffer will be submitted to the
audio hardware (for OSX/iOS) or the time at which the callback was invoked (when the
code in the callback queries the system time). Note that neither of these is what we
actually want to synchronize between devices in order to play in time.

In order for multiple devices to play in time, we need to synchronize the moment at which
their signals hit the speaker or output cable. If this compensation is not performed,
the output signals from devices with different output latencies will exhibit a persistent
offset from each other. For this reason, the audio system's output latency should be added
to system time values before passing them to Link methods. Examples of this latency
compensation can be found in the [platform](examples/linkaudio) implementations of the
example apps.

[asio]: https://www.steinberg.net/en/company/developers.html
[catch]: https://github.com/philsquared/Catch
[cmake]: https://www.cmake.org
[license]: LICENSE.md
[qt]: https://www.qt.io
