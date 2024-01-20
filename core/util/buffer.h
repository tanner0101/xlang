#pragma once

#include <cassert>
#include <concepts>
#include <cstddef>
#include <optional>

namespace xlang {

template <typename T>
concept Container = requires(T t, size_t i) {
    { t.size() } -> std::convertible_to<size_t>;
    { t[i] } -> std::convertible_to<typename T::value_type>;
    typename T::value_type;
};

template <Container T> class Buffer {
  private:
    T storage;
    size_t position = 0;

  public:
    Buffer(T s) : storage(std::move(s)) {}

    [[nodiscard]] inline auto peek() const -> T::value_type {
        if (empty()) {
            assert(false);
        }

        return storage[position];
    }

    inline auto pop() -> T::value_type {
        if (empty()) {
            assert(false);
        }

        return storage[position++];
    }

    [[nodiscard]] inline auto empty() const -> bool {
        return position >= storage.size();
    }
};

} // namespace xlang
