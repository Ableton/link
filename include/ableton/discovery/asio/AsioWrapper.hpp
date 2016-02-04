// Copyright: 2016, Ableton AG, Berlin. All rights reserved.

#pragma once

/*!
 * \brief Wrapper file for AsioStandalone library
 *
 * This file includes all necessary headers from the AsioStandalone library which are used
 * in Link, and also disables some compiler warnings which are specific to that library.
 */

// Clang
#if defined(__clang__)
#pragma clang diagnostic push
// warning: implicit conversion loses integer precision: 'type1' to 'type2'
#pragma clang diagnostic ignored "-Wconversion"
// warning: default label in switch which covers all enumeration values
#pragma clang diagnostic ignored "-Wcovered-switch-default"
// warning: empty paragraph passed to 'directive' command
#pragma clang diagnostic ignored "-Wdocumentation"
// warning: use of old-style cast
#pragma clang diagnostic ignored "-Wold-style-cast"
// warning: implicit conversion changes signedness: 'type1' to 'type2'
#pragma clang diagnostic ignored "-Wsign-conversion"
// warning: 'symbol' is not defined, evaluates to 0
#pragma clang diagnostic ignored "-Wundef"
// warning: unused typedef 'argument'
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#endif

// Visual Studio
#if defined(_MSC_VER)
#pragma warning(push)
// C4191: 'type cast': unsafe conversion from 'function pointer type' to 'function pointer
// type'. Calling this function through the result pointer may cause your program to fail
#pragma warning(disable: 4191)
// C4242: 'identifier': conversion from 'type1' to 'type2', possible loss of data
#pragma warning(disable: 4242)
// C4365: 'action': conversion from 'type1' to 'type2', signed/unsigned mismatch
#pragma warning(disable: 4365)
// C4548: expression before comma has no effect; expected expression with side-effect
#pragma warning(disable: 4548)
// C4619: #pragma warning: there is no warning number 'number'
#pragma warning(disable: 4619)
// C4668: 'symbol' is not defined as a preprocessor macro, replacing with '0' for
// 'directives'
#pragma warning(disable: 4668)
#endif


#include <asio.hpp>
#include <asio/system_timer.hpp>


// Clang
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

// Visual Studio
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
