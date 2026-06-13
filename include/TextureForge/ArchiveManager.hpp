#pragma once

#include "TextureForge/Common.hpp"

namespace textureforge {

Result<fs::path> exportPack(PackSummary const& pack);
Result<fs::path> importPackArchive();

} // namespace textureforge
