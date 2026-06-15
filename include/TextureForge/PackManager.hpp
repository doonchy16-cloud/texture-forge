#pragma once

#include "TextureForge/Common.hpp"
#include "TextureForge/TargetCatalog.hpp"

namespace textureforge {

Result<> writePackJson(PackSummary const& pack);
Result<PackSummary> createPack(std::string name);
std::vector<PackSummary> scanPacks();
std::vector<fs::path> importFiles(PackSummary const& pack);
std::vector<fs::path> stagedOverrideFiles(PackSummary const& pack);
Result<> deleteStagedOverride(PackSummary const& pack, fs::path const& relative);
Result<int> importSelectedFileIntoPack(PackSummary const& pack, TargetPreset const& target, fs::path const& source, bool removeImageBackground);
Result<> applyPack(PackSummary const& pack, bool reload);
Result<> resetOverrides(bool reload);
bool lastApplyHadIconReloadWarnings();
bool activePackOverridesIcon(IconType type, int id);
bool refreshActiveIconOverride(IconType type, int id);
bool refreshIconOverrideForFrameName(std::string const& frameName);
bool iconFrameReloadInProgress();
void applySavedPackIfAny();

} // namespace textureforge
