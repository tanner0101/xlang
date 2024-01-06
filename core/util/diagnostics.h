#pragma once

#include "source.h"
#include <string>
#include <vector>

enum class DiagnosticType {
    error,
    warning,
    note,
};

struct Diagnostic {
    DiagnosticType type;
    std::string message;
    Source source;
};

class Diagnostics {
  public:
    inline auto push_error(std::string message, Source source) -> void {
        diagnostics.push_back({DiagnosticType::error, message, source});
    }

    inline auto push_warning(std::string message, Source source) -> void {
        diagnostics.push_back({DiagnosticType::warning, message, source});
    }

    inline auto push_note(std::string message, Source source) -> void {
        diagnostics.push_back({DiagnosticType::note, message, source});
    }

    inline auto begin() -> std::vector<Diagnostic>::iterator {
        return diagnostics.begin();
    }

    inline auto end() -> std::vector<Diagnostic>::iterator {
        return diagnostics.end();
    }

    [[nodiscard]] inline auto size() const
        -> std::vector<Diagnostic>::size_type {
        return diagnostics.size();
    }

  private:
    std::vector<Diagnostic> diagnostics;
};
