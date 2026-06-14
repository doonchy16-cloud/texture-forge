#include "TextureForge/Common.hpp"

namespace textureforge {

void toast(std::string const& message, NotificationIcon icon) {
    Notification::create(message.c_str(), icon, 2.2f)->show();
}

std::string pathString(fs::path const& path) {
    return geode::utils::string::pathToString(path);
}

std::string genericPathString(fs::path const& path) {
    return path.generic_string();
}

std::string lowerString(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool startsWith(std::string const& value, std::string const& prefix) {
    return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
}

bool endsWith(std::string const& value, std::string const& suffix) {
    return value.size() >= suffix.size() && value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string sanitizePart(std::string value) {
    std::string out;
    out.reserve(value.size());
    for (auto ch : value) {
        auto c = static_cast<unsigned char>(ch);
        if (std::isalnum(c)) out.push_back(static_cast<char>(std::tolower(c)));
        else if (ch == '-' || ch == '_' || ch == '.' || std::isspace(c)) out.push_back('-');
    }

    out.erase(std::unique(out.begin(), out.end(), [](char a, char b) {
        return a == '-' && b == '-';
    }), out.end());
    while (!out.empty() && out.front() == '-') out.erase(out.begin());
    while (!out.empty() && out.back() == '-') out.pop_back();
    return out.empty() ? "pack" : out;
}

std::string iconID(int number) {
    return fmt::format("{:02}", number);
}

fs::path packsDir() {
    return Mod::get()->getConfigDir() / "packs";
}

fs::path exportsDir() {
    return Mod::get()->getConfigDir() / "exports";
}

fs::path packInboxDir() {
    return Mod::get()->getConfigDir() / "pack-imports";
}

fs::path resourcePath(fs::path const& relative) {
    return geode::dirs::getResourcesDir() / relative;
}

Result<> ensureDir(fs::path const& path) {
    return geode::utils::file::createDirectoryAll(path);
}

fs::path stagedResourcesDir(PackSummary const& pack) {
    return pack.path / "staged";
}

fs::path appliedResourcesDir(PackSummary const& pack) {
    return pack.path / "resources";
}

fs::path stagedDirtyMarkerPath(PackSummary const& pack) {
    return pack.path / ".staged-dirty";
}

bool hasStagedDirtyMarker(PackSummary const& pack) {
    std::error_code ec;
    return fs::exists(stagedDirtyMarkerPath(pack), ec);
}

Result<> markStagedDirty(PackSummary const& pack) {
    GEODE_UNWRAP(ensureDir(pack.path));
    return geode::utils::file::writeStringSafe(stagedDirtyMarkerPath(pack), "1\n");
}

Result<> clearStagedDirty(PackSummary const& pack) {
    std::error_code ec;
    fs::remove(stagedDirtyMarkerPath(pack), ec);
    if (ec) return Err("Unable to clear staged marker: {}", ec.message());
    return Ok();
}

bool directoryHasFiles(fs::path const& path) {
    std::error_code ec;
    if (!fs::exists(path, ec)) return false;
    for (auto const& entry : fs::recursive_directory_iterator(path, fs::directory_options::skip_permission_denied, ec)) {
        if (entry.is_regular_file(ec)) return true;
    }
    return false;
}

Result<> clearDirectory(fs::path const& path) {
    GEODE_UNWRAP(ensureDir(path));
    std::error_code ec;
    for (auto const& entry : fs::directory_iterator(path, fs::directory_options::skip_permission_denied, ec)) {
        fs::remove_all(entry.path(), ec);
        if (ec) return Err("Unable to clear {}: {}", pathString(path.filename()), ec.message());
    }
    return Ok();
}

Result<> copyDirectoryContents(fs::path const& source, fs::path const& destination) {
    std::error_code ec;
    if (!fs::exists(source, ec)) return Ok();
    GEODE_UNWRAP(ensureDir(destination));

    for (
        auto it = fs::recursive_directory_iterator(source, fs::directory_options::skip_permission_denied, ec);
        it != fs::recursive_directory_iterator();
        it.increment(ec)
    ) {
        if (ec) return Err("Unable to scan {}: {}", pathString(source.filename()), ec.message());
        auto rel = fs::relative(it->path(), source, ec);
        if (ec) return Err("Unable to copy pack file: {}", ec.message());
        auto target = destination / rel;
        if (it->is_directory(ec)) {
            GEODE_UNWRAP(ensureDir(target));
        }
        else if (it->is_regular_file(ec)) {
            GEODE_UNWRAP(ensureDir(target.parent_path()));
            fs::copy_file(it->path(), target, fs::copy_options::overwrite_existing, ec);
            if (ec) return Err("Unable to copy {}: {}", pathString(rel), ec.message());
        }
    }
    return Ok();
}

Result<> ensurePackFolders(PackSummary const& pack) {
    GEODE_UNWRAP(ensureDir(appliedResourcesDir(pack)));
    GEODE_UNWRAP(ensureDir(stagedResourcesDir(pack)));
    GEODE_UNWRAP(ensureDir(pack.path / "imports"));
    GEODE_UNWRAP(ensureDir(pack.path / "editor" / "saves"));
    GEODE_UNWRAP(ensureDir(pack.path / "sources"));
    if (!hasStagedDirtyMarker(pack) && !directoryHasFiles(stagedResourcesDir(pack)) && directoryHasFiles(appliedResourcesDir(pack))) {
        GEODE_UNWRAP(copyDirectoryContents(appliedResourcesDir(pack), stagedResourcesDir(pack)));
    }
    return Ok();
}

Result<> commitStagedResources(PackSummary const& pack) {
    GEODE_UNWRAP(ensurePackFolders(pack));
    auto stagedHasFiles = directoryHasFiles(stagedResourcesDir(pack));
    auto stagedDirty = hasStagedDirtyMarker(pack);
    if (!stagedHasFiles && !stagedDirty) return Ok();
    GEODE_UNWRAP(clearDirectory(appliedResourcesDir(pack)));
    if (stagedHasFiles) {
        GEODE_UNWRAP(copyDirectoryContents(stagedResourcesDir(pack), appliedResourcesDir(pack)));
    }
    GEODE_UNWRAP(clearStagedDirty(pack));
    return Ok();
}

fs::path uniqueDestination(fs::path destination) {
    std::error_code ec;
    if (!fs::exists(destination, ec)) return destination;

    auto parent = destination.parent_path();
    auto stem = destination.stem();
    auto extension = destination.extension();
    for (auto index = 2; index < 1000; ++index) {
        auto candidate = parent / fmt::format("{}-{}{}", pathString(stem), index, pathString(extension));
        if (!fs::exists(candidate, ec)) return candidate;
    }
    return parent / fmt::format("{}-{}{}", pathString(stem), std::chrono::steady_clock::now().time_since_epoch().count(), pathString(extension));
}

std::string uniquePackID(std::string const& name) {
    auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return fmt::format("doonc.texture-forge.{}.{}", sanitizePart(name), stamp);
}

std::string normalizedPathString(fs::path const& path) {
    std::error_code ec;
    auto absolute = fs::absolute(path, ec);
    if (ec) absolute = path;
    return pathString(absolute.lexically_normal());
}

bool isImageImport(fs::path const& path) {
    auto ext = lowerString(pathString(path.extension()));
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg";
}

bool isSupportedImport(fs::path const& path) {
    return isImageImport(path);
}

bool isPackArchive(fs::path const& path) {
    auto ext = lowerString(pathString(path.extension()));
    return ext == ".zip" || ext == ".textureforge";
}
std::optional<std::string> readStringKey(matjson::Value const& json, char const* key) {
    if (!json.contains(key)) return std::nullopt;
    auto value = json[key].asString();
    if (!value) return std::nullopt;
    return value.unwrap();
}
int clampIndex(int index, size_t count) {
    if (count == 0) return 0;
    if (index < 0) return static_cast<int>(count - 1);
    if (static_cast<size_t>(index) >= count) return 0;
    return index;
}

} // namespace textureforge
