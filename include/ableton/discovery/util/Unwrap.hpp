// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <memory>

namespace ableton
{
namespace util
{

// A metafunction that provides access to the wrapped type for various
// standard wrapping constructs.

template <typename T>
struct unwrap
{
  using type = T;
};

// Traits-like convenience type that provides easy access to member
// types of a wrapped type.
// Ex. typename member_type_of<Foo>::Bar

template <typename T>
using member_type_of = typename unwrap<T>::type;

template <typename T>
struct unwrap<std::reference_wrapper<T>>
{
  using type = T;
};

template <typename T>
struct unwrap<std::shared_ptr<T>>
{
  using type = T;
};

template <typename T>
struct unwrap<std::unique_ptr<T>>
{
  using type = T;
};

} // namespace util
} // namespace ableton
