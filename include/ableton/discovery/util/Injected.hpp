// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#pragma once

#include <memory>

namespace ableton
{
namespace util
{

// Utility type for aiding in dependency injection.

// Base template and implementation for injected valued
template <typename T>
struct Injected
{
  using type = T;

  Injected(T t)
    : val(std::move(t))
  {
  }

  Injected(const Injected&) = default;
  Injected& operator=(const Injected&) = default;

  Injected(Injected&& rhs)
    : val(std::move(rhs.val))
  {
  }

  Injected& operator=(Injected&& rhs)
  {
    val = std::move(rhs.val);
    return *this;
  }

  T* operator->()
  {
    return &val;
  }

  const T* operator->() const
  {
    return &val;
  }

  T& operator*()
  {
    return val;
  }

  const T& operator*() const
  {
    return val;
  }

  T val;
};

// Utility function for injecting values
template <typename T>
Injected<T> injectVal(T t)
{
  return {std::move(t)};
}

// Specialization for injected references
template <typename T>
struct Injected<T&>
{
  using type = T;

  Injected(T& t)
    : ref(std::ref(t))
  {
  }

  Injected(const Injected&) = default;
  Injected& operator=(const Injected&) = default;

  Injected(Injected&& rhs)
    : ref(std::move(rhs.ref))
  {
  }

  Injected& operator=(Injected&& rhs)
  {
    ref = std::move(rhs.ref);
    return *this;
  }

  T* operator->()
  {
    return &ref.get();
  }

  const T* operator->() const
  {
    return &ref.get();
  }

  T& operator*()
  {
    return ref;
  }

  const T& operator*() const
  {
    return ref;
  }

  std::reference_wrapper<T> ref;
};

// Utility function for injecting references
template <typename T>
Injected<T&> injectRef(T& t)
{
  return {t};
}

// Specialization for injected shared_ptr
template <typename T>
struct Injected<std::shared_ptr<T>>
{
  using type = T;

  Injected(std::shared_ptr<T> pT)
    : shared(std::move(pT))
  {
  }

  Injected(const Injected&) = default;
  Injected& operator=(const Injected&) = default;

  Injected(Injected&& rhs)
    : shared(std::move(rhs.shared))
  {
  }

  Injected& operator=(Injected&& rhs)
  {
    shared = std::move(rhs.shared);
    return *this;
  }

  T* operator->()
  {
    return shared.get();
  }

  const T* operator->() const
  {
    return shared.get();
  }

  T& operator*()
  {
    return *shared;
  }

  const T& operator*() const
  {
    return *shared;
  }

  std::shared_ptr<T> shared;
};

// Utility function for injected shared_ptr
template <typename T>
Injected<std::shared_ptr<T>> injectShared(std::shared_ptr<T> shared)
{
  return {std::move(shared)};
}

// Specialization for injected unique_ptr
template <typename T>
struct Injected<std::unique_ptr<T>>
{
  using type = T;

  Injected(std::unique_ptr<T> pT)
    : unique(std::move(pT))
  {
  }

  Injected(const Injected&) = default;
  Injected& operator=(const Injected&) = default;

  Injected(Injected&& rhs)
    : unique(std::move(rhs.unique))
  {
  }

  Injected& operator=(Injected&& rhs)
  {
    unique = std::move(rhs.unique);
    return *this;
  }

  T* operator->()
  {
    return unique.get();
  }

  const T* operator->() const
  {
    return unique.get();
  }

  T& operator*()
  {
    return *unique;
  }

  const T& operator*() const
  {
    return *unique;
  }

  std::unique_ptr<T> unique;
};

// Utility function for injected unique_ptr
template <typename T>
Injected<std::unique_ptr<T>> injectUnique(std::unique_ptr<T> unique)
{
  return {std::move(unique)};
}

} // namespace util
} // namespace ableton
