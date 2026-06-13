#pragma once

#include "TextureForge/Common.hpp"

namespace textureforge {

Result<ImageData> loadImage(fs::path const& path);
Result<> writeTextureOutput(fs::path const& source, fs::path const& destination, fs::path const& output, bool removeImageBackground = false);

} // namespace textureforge
