#pragma once

#include <array>
#include <string>

#define ENUM_CLASS(name, ...)                                                  \
    enum class name { __VA_ARGS__, count };                                    \
    inline auto name##_toString(name value) -> std::string {                   \
        constexpr const char* s_rawNames = #__VA_ARGS__;                       \
        static const std::array<std::string, static_cast<size_t>(name::count)> \
            s_namesArray = [] {                                                \
                std::array<std::string, static_cast<size_t>(name::count)>      \
                    namesArray;                                                \
                std::string currentName;                                       \
                size_t idx = 0;                                                \
                for (char c : std::string_view{s_rawNames}) {                  \
                    if (c == ',') {                                            \
                        namesArray[idx++] = std::move(currentName);            \
                        currentName.clear();                                   \
                    } else if (!std::isspace(static_cast<unsigned char>(c))) { \
                        currentName += c;                                      \
                    }                                                          \
                }                                                              \
                if (!currentName.empty()) {                                    \
                    namesArray[idx] = std::move(currentName);                  \
                }                                                              \
                return namesArray;                                             \
            }();                                                               \
        return s_namesArray[static_cast<size_t>(value)];                       \
    }                                                                          \
    inline auto operator<<(std::ostream& os, name value) -> std::ostream& {    \
        return os << name##_toString(value);                                   \
    }
