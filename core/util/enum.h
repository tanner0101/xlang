#pragma once

#include <array>
#include <string>

#define ENUM_CLASS(name, ...)                                                  \
    /* NOLINTBEGIN */                                                          \
    enum class name { __VA_ARGS__, count };                                    \
    inline auto name##_to_string(name value) -> std::string {                  \
        constexpr const char* s_raw_names = #__VA_ARGS__;                      \
        static const std::array<std::string, static_cast<size_t>(name::count)> \
            s_names_array = [] {                                               \
                std::array<std::string, static_cast<size_t>(name::count)>      \
                    names_array;                                               \
                std::string current_name;                                      \
                size_t idx = 0;                                                \
                for (char c : std::string_view{s_raw_names}) {                 \
                    if (c == ',') {                                            \
                        names_array[idx++] = std::move(current_name);          \
                        current_name.clear();                                  \
                    } else if (!std::isspace(static_cast<unsigned char>(c))) { \
                        current_name += c;                                     \
                    }                                                          \
                }                                                              \
                if (!current_name.empty()) {                                   \
                    names_array[idx] = std::move(current_name);                \
                }                                                              \
                return names_array;                                            \
            }();                                                               \
        return s_names_array[static_cast<size_t>(value)];                      \
    }                                                                          \
    inline auto operator<<(std::ostream& os, name value) -> std::ostream& {    \
        return os << name##_to_string(value);                                  \
    }                                                                          \
    /* NOLINTEND */
