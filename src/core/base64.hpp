#pragma once

#include "core/types.hpp"
#include "export.hpp"

namespace metaagent::core {

METAAGENT_API core::String base64_encode(const uint8_t* data, size_t size);

METAAGENT_API core::String base64_encode(const core::Array<uint8_t>& data);

} // namespace metaagent::core
