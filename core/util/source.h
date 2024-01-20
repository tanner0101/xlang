#pragma once

#include <iostream>

struct Source {
    int line;
    int column;
};

inline auto operator<<(std::ostream& os, Source source) -> std::ostream& {
    return os << source.line << "," << source.column;
};
