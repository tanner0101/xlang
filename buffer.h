#include <concepts>
#include <cstddef>
#include <optional>

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
    Buffer(const T &s) : storage(s) {}

    std::optional<typename T::value_type> peek() const {
        if (eof()) {
            return std::nullopt;
        }

        return storage[position];
    }

    std::optional<typename T::value_type> pop() {
        if (eof()) {
            return std::nullopt;
        }

        return storage[position++];
    }

    bool eof() const { return position >= storage.size(); }
};
