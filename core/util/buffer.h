#pragma once

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

    [[nodiscard]] inline auto safe_peek() const
        -> std::optional<typename T::value_type> {
        if (empty()) {
            return std::nullopt;
        }

        return storage[position];
    }

    inline auto safe_pop() -> std::optional<typename T::value_type> {
        if (empty()) {
            return std::nullopt;
        }

        return storage[position++];
    }

    [[nodiscard]] inline auto peek() const -> T::value_type {
        return safe_peek().value();
    }

    inline auto pop() -> T::value_type { return safe_pop().value(); }

    [[nodiscard]] inline auto empty() const -> bool {
        return position >= storage.size();
    }
};

} // namespace xlang
