#pragma once

#include <Geode/Geode.hpp>
#include <Geode/loader/Dirs.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/utils/cocos.hpp>
#include <Geode/utils/file.hpp>
#include <Geode/utils/string.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <system_error>
#include <unordered_set>
#include <vector>

namespace textureforge {

using namespace geode::prelude;
namespace fs = std::filesystem;

inline constexpr auto kRuntimePackID = "doonc.texture-forge.runtime";
inline constexpr auto kPackSchema = "2.0";

struct PackSummary {
    std::string name;
    std::string id;
    fs::path path;
};

struct TargetPreset {
    std::string label;
    std::vector<fs::path> outputs;
};

struct TargetGroupRange {
    std::string label;
    size_t start = 0;
    size_t count = 0;
    int firstNumber = 0;
    int lastNumber = 0;
    bool numbered = false;
};

struct TargetCatalog {
    std::vector<TargetPreset> presets;
    std::vector<TargetGroupRange> groups;
};

struct IntRect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

struct IconSheetLayout {
    int width = 0;
    int height = 0;
    fs::path pngPath;
    fs::path plistPath;
    std::vector<IntRect> primaryFrames;
    std::vector<IntRect> clearFrames;
};

struct ImageData {
    int width = 0;
    int height = 0;
    std::vector<unsigned char> rgba;
};

void toast(std::string const& message, NotificationIcon icon = NotificationIcon::Info);
std::string pathString(fs::path const& path);
std::string genericPathString(fs::path const& path);
std::string lowerString(std::string value);
bool startsWith(std::string const& value, std::string const& prefix);
bool endsWith(std::string const& value, std::string const& suffix);
std::string sanitizePart(std::string value);
std::string iconID(int number);
fs::path packsDir();
fs::path exportsDir();
fs::path packInboxDir();
fs::path resourcePath(fs::path const& relative);
Result<> ensureDir(fs::path const& path);
fs::path stagedResourcesDir(PackSummary const& pack);
fs::path appliedResourcesDir(PackSummary const& pack);
fs::path stagedDirtyMarkerPath(PackSummary const& pack);
bool hasStagedDirtyMarker(PackSummary const& pack);
Result<> markStagedDirty(PackSummary const& pack);
Result<> clearStagedDirty(PackSummary const& pack);
bool directoryHasFiles(fs::path const& path);
Result<> clearDirectory(fs::path const& path);
Result<> copyDirectoryContents(fs::path const& source, fs::path const& destination);
Result<> ensurePackFolders(PackSummary const& pack);
Result<> commitStagedResources(PackSummary const& pack);
fs::path uniqueDestination(fs::path destination);
std::string uniquePackID(std::string const& name);
std::string normalizedPathString(fs::path const& path);
bool isImageImport(fs::path const& path);
bool isSupportedImport(fs::path const& path);
bool isPackArchive(fs::path const& path);
std::optional<std::string> readStringKey(matjson::Value const& json, char const* key);
int clampIndex(int index, size_t count);
std::string stripScaleSuffix(std::string stem);

} // namespace textureforge
