#pragma once

#include "TextureForge\Common.hpp"

#include <functional>

namespace textureforge {

using EditorSaveCallback = std::function<void(fs::path const&)>;

void openIconEditor(PackSummary pack, TargetPreset target, EditorSaveCallback onSave);

} // namespace textureforge
