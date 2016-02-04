// Copyright: 2016, Ableton AG, Berlin. All rights reserved.

#pragma once

/*!
 * \brief Wrapper file for Catch library
 *
 * This file includes the Catch header for Link, and also disables some compiler warnings
 * which are specific to that library.
 */

// Visual Studio
#if defined(_MSC_VER)
#pragma warning(push)
// C4242: 'identifier': conversion from 'type1' to 'type2', possible loss of data
#pragma warning(disable: 4242)
// C4365: 'action': conversion from 'type1' to 'type2', signed/unsigned mismatch
#pragma warning(disable: 4365)
// C4388: 'operator': signed/unsigned mismatch
#pragma warning(disable: 4388)
// C4668: 'symbol' is not defined as a preprocessor macro, replacing with '0' for
// 'directives'
#pragma warning(disable: 4668)
// C4702: unreachable code
#pragma warning(disable: 4702)
#endif


#include <test/catch/catch.hpp>


// Visual Studio
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

