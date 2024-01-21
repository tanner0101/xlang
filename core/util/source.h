#pragma once

#include <iostream>

namespace xlang {

struct Source {
    int line;
    int column;

    auto operator==(const Source& other) const -> bool = default;
};

inline auto operator<<(std::ostream& os, Source source) -> std::ostream& {
    return os << source.line << "," << source.column;
};

} // namespace xlang
