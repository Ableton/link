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

#pragma once

/*!
 * \brief Includes the Asio headers used by Link, with warnings suppressed.
 *
 * This file must not configure Asio. ASIO_VERSION_NAMESPACE, ASIO_NO_TYPEID,
 * ASIO_STANDALONE, INCL_EXTRA_HTON_FUNCTIONS and _WIN32_WINNT all have to be in effect
 * before the first Asio or Winsock header is preprocessed, which a header cannot
 * guarantee: in a translation unit that includes <asio.hpp> before any Link header, a
 * definition made here arrives too late and is silently ignored. They are therefore
 * defined by the build system. See cmake_include/ConfigureAsioStandalone.cmake, and the
 * "Other Build Systems" section of the README for non-CMake projects.
 *
 * Warning suppression is the one thing that has to live here rather than in the build
 * system, because it must apply to the Asio headers only and not to the rest of the
 * translation unit.
 */

#if defined(__clang__)
#pragma clang diagnostic push
#if __has_warning("-Wcomma")
#pragma clang diagnostic ignored "-Wcomma"
#endif
#if __has_warning("-Wshorten-64-to-32")
#pragma clang diagnostic ignored "-Wshorten-64-to-32"
#endif
#if __has_warning("-Wunused-local-typedef")
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#endif
#endif

#if defined(_MSC_VER)
#define _SCL_SECURE_NO_WARNINGS 1
#pragma warning(push, 0)
#pragma warning(disable : 4242)
#pragma warning(disable : 4668)
#pragma warning(disable : 4702)
#pragma warning(disable : 5204)
#pragma warning(disable : 5220)
#endif

#include <asio.hpp>
#include <asio/system_timer.hpp>

#if defined(_MSC_VER)
#pragma warning(pop)
#undef _SCL_SECURE_NO_WARNINGS
#endif

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
