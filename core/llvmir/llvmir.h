#pragma once

#include "core/ir/ir.h"
#include "core/util/diagnostics.h"
#include <string>

namespace xlang::llvmir {

auto print(const ir::Module& module, Diagnostics& diagnostics) -> std::string;

}