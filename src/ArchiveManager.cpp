#include "TextureForge/ArchiveManager.hpp"
#include "TextureForge/PackManager.hpp"

namespace textureforge {

struct TempDirGuard {
    fs::path path;

    explicit TempDirGuard(fs::path value) : path(std::move(value)) {}

    ~TempDirGuard() {
        if (path.empty()) return;
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

void cleanupPackImportTemps() {
    std::error_code ec;
    auto inbox = packInboxDir();
    if (!fs::exists(inbox, ec)) return;

    for (auto const& entry : fs::directory_iterator(inbox, fs::directory_options::skip_permission_denied, ec)) {
        if (!entry.is_directory(ec)) continue;
        if (!startsWith(pathString(entry.path().filename()), "_tmp-")) continue;
        fs::remove_all(entry.path(), ec);
    }
}

Result<fs::path> exportPack(PackSummary const& pack) {
    GEODE_UNWRAP(ensureDir(exportsDir()));
    auto destination = uniqueDestination(exportsDir() / fmt::format("{}.textureforge", sanitizePart(pack.name)));
    GEODE_UNWRAP_INTO(auto zip, geode::utils::file::Zip::create(destination));
    GEODE_UNWRAP(zip.addAllFrom(pack.path));
    return Ok(destination);
}

bool isSafeArchiveEntry(fs::path const& entry) {
    auto text = genericPathString(entry);
    if (text.empty() || startsWith(text, "/") || startsWith(text, "\\") || text.find(':') != std::string::npos) return false;
    for (auto const& part : entry) {
        auto value = pathString(part);
        if (value == ".." || value == ".") return false;
    }
    return true;
}

Result<fs::path> findExtractedPackRoot(fs::path const& tempDir) {
    std::error_code ec;
    if (fs::exists(tempDir / "pack.json", ec)) return Ok(tempDir);

    std::vector<fs::path> candidates;
    for (auto const& entry : fs::directory_iterator(tempDir, fs::directory_options::skip_permission_denied, ec)) {
        if (entry.is_directory(ec) && fs::exists(entry.path() / "pack.json", ec)) candidates.push_back(entry.path());
    }

    if (candidates.size() == 1) return Ok(candidates.front());
    if (candidates.empty()) return Err("Pack archive is missing pack.json");
    return Err("Pack archive has more than one possible root folder");
}

std::unordered_set<std::string> existingPackIDs() {
    std::unordered_set<std::string> ids;
    for (auto const& pack : scanPacks()) ids.insert(pack.id);
    return ids;
}

Result<fs::path> importPackArchive() {
    GEODE_UNWRAP(ensureDir(packInboxDir()));
    cleanupPackImportTemps();
    std::error_code ec;
    std::vector<fs::path> archives;
    for (auto const& entry : fs::directory_iterator(packInboxDir(), fs::directory_options::skip_permission_denied, ec)) {
        if (entry.is_regular_file(ec) && isPackArchive(entry.path())) archives.push_back(entry.path());
    }
    std::sort(archives.begin(), archives.end());
    if (archives.empty()) return Err("Put a .textureforge or .zip pack in the import folder first");
    if (archives.size() > 1) return Err("Keep one pack archive in Pack Imports so Texture Forge knows which one to import");

    auto archive = archives.front();
    GEODE_UNWRAP_INTO(auto unzip, geode::utils::file::Unzip::create(archive));
    auto entries = unzip.getEntries();
    if (entries.empty()) return Err("Pack archive is empty");
    if (entries.size() > 5000) return Err("Pack archive has too many files");

    auto totalSize = size_t { 0 };
    constexpr auto maxArchiveBytes = size_t { 150 } * 1024 * 1024;
    for (auto const& entry : entries) {
        if (!isSafeArchiveEntry(entry)) return Err("Pack archive contains unsafe path: {}", genericPathString(entry));
        if (pathString(entry.filename()).empty()) continue;
        GEODE_UNWRAP_INTO(auto bytes, unzip.extract(entry));
        totalSize += bytes.size();
        if (totalSize > maxArchiveBytes) return Err("Pack archive is too large");
    }

    auto tempDir = uniqueDestination(packInboxDir() / fmt::format("_tmp-{}", sanitizePart(pathString(archive.stem()))));
    TempDirGuard tempGuard(tempDir);
    GEODE_UNWRAP(ensureDir(tempDir));
    GEODE_UNWRAP(unzip.extractAllTo(tempDir));

    GEODE_UNWRAP_INTO(auto packRoot, findExtractedPackRoot(tempDir));
    GEODE_UNWRAP_INTO(auto json, geode::utils::file::readJson(packRoot / "pack.json"));
    auto schema = readStringKey(json, "textureforge").value_or("");
    if (schema != kPackSchema) return Err("Pack archive schema is not supported");

    auto name = readStringKey(json, "name").value_or(pathString(archive.stem()));
    auto id = readStringKey(json, "id").value_or(uniquePackID(name));
    auto ids = existingPackIDs();
    if (id.empty() || ids.contains(id)) id = uniquePackID(name);

    auto destination = uniqueDestination(packsDir() / sanitizePart(name));
    GEODE_UNWRAP(ensureDir(destination));
    GEODE_UNWRAP(copyDirectoryContents(packRoot, destination));
    auto imported = PackSummary { name, id, destination };
    GEODE_UNWRAP(writePackJson(imported));
    return Ok(destination);
}

} // namespace textureforge
