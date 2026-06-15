#include "TextureForge/PackManager.hpp"
#include "TextureForge/ImageProcessor.hpp"

#include <string_view>

namespace textureforge {

std::vector<fs::path> relativeFilesIn(fs::path const& root, std::string const& extension = "") {
    std::vector<fs::path> files;
    std::error_code ec;
    if (!fs::exists(root, ec)) return files;

    for (
        auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec);
        it != fs::recursive_directory_iterator();
        it.increment(ec)
    ) {
        if (ec) break;
        if (!it->is_regular_file(ec)) continue;
        if (!extension.empty() && lowerString(pathString(it->path().extension())) != extension) continue;
        auto relative = fs::relative(it->path(), root, ec);
        if (!ec) files.push_back(fs::path(relative.generic_string()));
    }
    std::sort(files.begin(), files.end());
    files.erase(std::unique(files.begin(), files.end()), files.end());
    return files;
}

std::vector<fs::path> relativePlistsIn(fs::path const& root) {
    return relativeFilesIn(root, ".plist");
}

std::vector<fs::path> relativePngsIn(fs::path const& root) {
    return relativeFilesIn(root, ".png");
}

void mergePaths(std::vector<fs::path>& destination, std::vector<fs::path> const& source) {
    destination.insert(destination.end(), source.begin(), source.end());
    std::sort(destination.begin(), destination.end());
    destination.erase(std::unique(destination.begin(), destination.end()), destination.end());
}

std::vector<fs::path> plistTexturePngs(std::vector<fs::path> const& relativePlists) {
    std::vector<fs::path> pngs;
    pngs.reserve(relativePlists.size());
    for (auto const& relativePlist : relativePlists) {
        auto relativePng = relativePlist;
        relativePng.replace_extension(".png");
        pngs.push_back(fs::path(relativePng.generic_string()));
    }
    std::sort(pngs.begin(), pngs.end());
    pngs.erase(std::unique(pngs.begin(), pngs.end()), pngs.end());
    return pngs;
}

bool isIconResource(fs::path const& relative) {
    return lowerString(genericPathString(relative.parent_path())) == "icons";
}

struct RuntimeRefreshStatus {
    bool iconSheetsSeen = false;
    bool iconReloadVerified = true;
    size_t iconSheetsReloaded = 0;
};

bool sLastApplyHadIconReloadWarnings = false;
bool sIconFrameReloadInProgress = false;

struct IconFrameReloadGuard {
    bool m_previous = false;

    IconFrameReloadGuard() : m_previous(sIconFrameReloadInProgress) {
        sIconFrameReloadInProgress = true;
    }

    ~IconFrameReloadGuard() {
        sIconFrameReloadInProgress = m_previous;
    }
};

bool iconFrameReloadInProgress() {
    return sIconFrameReloadInProgress;
}

std::string textureQualitySuffix() {
    auto* director = CCDirector::get();
    auto quality = director ? director->getLoadedTextureQuality() : kTextureQualityHigh;
    switch (quality) {
        case kTextureQualityLow: return "";
        case kTextureQualityMedium: return "-hd";
        case kTextureQualityHigh: return "-uhd";
        default: return "-uhd";
    }
}

std::string textureQualityName() {
    auto* director = CCDirector::get();
    auto quality = director ? director->getLoadedTextureQuality() : kTextureQualityHigh;
    switch (quality) {
        case kTextureQualityLow: return "low";
        case kTextureQualityMedium: return "medium";
        case kTextureQualityHigh: return "high";
        default: return "unknown";
    }
}

fs::path pathWithStemSuffix(fs::path const& path, std::string const& suffix) {
    auto out = path;
    out.replace_filename(pathString(path.stem()) + suffix + pathString(path.extension()));
    return fs::path(out.generic_string());
}

fs::path logicalBasePlistFor(fs::path const& relativePlist) {
    auto out = relativePlist;
    out.replace_filename(stripScaleSuffix(pathString(relativePlist.stem())) + pathString(relativePlist.extension()));
    return fs::path(out.generic_string());
}

fs::path qualityPlistFor(fs::path const& logicalBasePlist) {
    return pathWithStemSuffix(logicalBasePlist, textureQualitySuffix());
}

fs::path qualityPngFor(fs::path const& logicalBasePlist) {
    auto png = qualityPlistFor(logicalBasePlist);
    png.replace_extension(".png");
    return fs::path(png.generic_string());
}

std::vector<fs::path> qualityPlistVariantsFor(fs::path const& logicalBasePlist) {
    return {
        logicalBasePlist,
        pathWithStemSuffix(logicalBasePlist, "-hd"),
        pathWithStemSuffix(logicalBasePlist, "-uhd"),
    };
}

std::vector<fs::path> qualityPngVariantsFor(fs::path const& logicalBasePlist) {
    auto variants = qualityPlistVariantsFor(logicalBasePlist);
    for (auto& variant : variants) variant.replace_extension(".png");
    return variants;
}

bool isIconPlist(fs::path const& relative) {
    return isIconResource(relative) && lowerString(pathString(relative.extension())) == ".plist";
}

std::vector<fs::path> logicalIconBasePlists(std::vector<fs::path> const& relativePlists) {
    std::vector<fs::path> bases;
    for (auto const& relativePlist : relativePlists) {
        if (!isIconPlist(relativePlist)) continue;
        bases.push_back(logicalBasePlistFor(relativePlist));
    }
    std::sort(bases.begin(), bases.end());
    bases.erase(std::unique(bases.begin(), bases.end()), bases.end());
    return bases;
}

std::vector<fs::path> nonIconPlists(std::vector<fs::path> const& relativePlists) {
    std::vector<fs::path> out;
    for (auto const& relativePlist : relativePlists) {
        if (!isIconPlist(relativePlist)) out.push_back(relativePlist);
    }
    return out;
}

std::string slashNormalizedLower(std::string text) {
    std::replace(text.begin(), text.end(), '\\', '/');
    while (!text.empty() && text.back() == '/') text.pop_back();
    return lowerString(text);
}

bool pathIsInside(fs::path const& child, fs::path const& parent) {
    auto childText = slashNormalizedLower(normalizedPathString(child));
    auto parentText = slashNormalizedLower(normalizedPathString(parent));
    return childText == parentText || startsWith(childText, parentText + "/");
}

std::string resolvedPathFor(fs::path const& lookup) {
    auto* fileUtils = CCFileUtils::get();
    if (!fileUtils) return {};
    auto resolved = fileUtils->fullPathForFilename(genericPathString(lookup).c_str(), false);
    return std::string(resolved.c_str());
}

std::vector<std::string> frameKeysFromPlist(fs::path const& physicalPlist) {
    std::vector<std::string> keys;
    auto pathText = normalizedPathString(physicalPlist);
    auto dict = geode::Ref<CCDictionary>::adopt(CCDictionary::createWithContentsOfFileThreadSafe(pathText.c_str()));
    if (!dict) {
        log::warn("Texture Forge could not parse plist frames from {}", pathText);
        return keys;
    }

    auto* framesObject = dict->objectForKey("frames");
    auto* frames = static_cast<CCDictionary*>(framesObject);
    if (!frames) {
        log::warn("Texture Forge plist has no frames dictionary: {}", pathText);
        return keys;
    }

    for (auto [key, value] : CCDictionaryExt<std::string, CCObject*>(frames)) {
        if (!key.empty()) keys.push_back(key);
    }
    std::sort(keys.begin(), keys.end());
    keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
    return keys;
}

std::vector<std::string> frameKeysFromExistingVariants(fs::path const& root, fs::path const& logicalBasePlist) {
    std::vector<std::string> keys;
    std::error_code ec;
    for (auto const& variant : qualityPlistVariantsFor(logicalBasePlist)) {
        auto physical = root / variant;
        if (!fs::exists(physical, ec)) continue;
        auto variantKeys = frameKeysFromPlist(physical);
        keys.insert(keys.end(), variantKeys.begin(), variantKeys.end());
    }
    std::sort(keys.begin(), keys.end());
    keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
    return keys;
}

size_t removeSpriteFrameKeys(CCSpriteFrameCache* frameCache, std::vector<std::string> const& frameKeys) {
    if (!frameCache) return 0;

    auto removed = size_t { 0 };
    for (auto const& key : frameKeys) {
        auto lookup = gd::string(key.c_str());
        auto existed = (frameCache->m_pSpriteFrames && frameCache->m_pSpriteFrames->objectForKey(lookup)) ||
            (frameCache->m_pSpriteFramesAliases && frameCache->m_pSpriteFramesAliases->objectForKey(lookup));
        frameCache->removeSpriteFrameByName(key.c_str());
        if (existed) ++removed;
    }
    return removed;
}

void removeTextureKeysFor(CCTextureCache* textureCache, fs::path const& relativePng, fs::path const* appliedRoot);

void forgetLoadedSpriteFrameFiles(CCSpriteFrameCache* frameCache, fs::path const* appliedRoot, fs::path const& logicalBasePlist) {
    if (!frameCache || !frameCache->m_pLoadedFileNames) return;

    for (auto const& variant : qualityPlistVariantsFor(logicalBasePlist)) {
        std::vector<std::string> names {
            genericPathString(variant),
            pathString(variant),
            genericPathString(variant.filename()),
            pathString(variant.filename()),
            normalizedPathString(resourcePath(variant)),
            genericPathString(resourcePath(variant)),
        };
        if (appliedRoot) {
            auto physical = *appliedRoot / variant;
            names.push_back(normalizedPathString(physical));
            names.push_back(genericPathString(physical));
        }

        for (auto const& name : names) {
            frameCache->m_pLoadedFileNames->erase(gd::string(name.c_str()));
        }
    }
}

void removeTextureKeysForVariants(CCTextureCache* textureCache, fs::path const& logicalBasePlist, fs::path const* appliedRoot) {
    if (!textureCache) return;

    for (auto const& png : qualityPngVariantsFor(logicalBasePlist)) {
        removeTextureKeysFor(textureCache, png, appliedRoot);
        textureCache->removeTextureForKey(genericPathString(png.filename()).c_str());
        textureCache->removeTextureForKey(pathString(png.filename()).c_str());
    }
}

bool verifySpriteFrameKeys(CCSpriteFrameCache* frameCache, std::vector<std::string> const& frameKeys) {
    if (!frameCache || frameKeys.empty()) return false;
    auto samples = std::min<size_t>(frameKeys.size(), 5);
    for (auto index = size_t { 0 }; index < samples; ++index) {
        if (!frameCache->spriteFrameByName(frameKeys[index].c_str())) return false;
    }
    return true;
}

RuntimeRefreshStatus reloadIconSpriteFramesFromRoot(
    fs::path const& root,
    fs::path const* expectedAppliedRoot,
    std::vector<fs::path> const& affectedPlists
) {
    IconFrameReloadGuard reloadGuard;
    RuntimeRefreshStatus status;
    auto* frameCache = CCSpriteFrameCache::get();
    auto* textureCache = CCTextureCache::get();
    auto bases = logicalIconBasePlists(affectedPlists);
    if (bases.empty()) return status;
    status.iconSheetsSeen = true;

    log::info(
        "Texture Forge exact icon reload begin: {} logical sheet(s), quality={} suffix='{}', root={}",
        bases.size(),
        textureQualityName(),
        textureQualitySuffix().empty() ? "(none)" : textureQualitySuffix(),
        normalizedPathString(root)
    );

    std::error_code ec;
    for (auto const& base : bases) {
        auto qualityPlist = qualityPlistFor(base);
        auto qualityPng = qualityPngFor(base);
        auto physicalPlist = root / qualityPlist;
        auto physicalPng = root / qualityPng;

        if (!fs::exists(physicalPlist, ec) || !fs::exists(physicalPng, ec)) {
            status.iconReloadVerified = false;
            log::warn(
                "Texture Forge icon reload skipped missing quality files plist={} png={}",
                normalizedPathString(physicalPlist),
                normalizedPathString(physicalPng)
            );
            continue;
        }

        if (expectedAppliedRoot) {
            auto resolvedPlist = resolvedPathFor(qualityPlist);
            auto resolvedPng = resolvedPathFor(qualityPng);
            log::info(
                "Texture Forge CCFileUtils resolved {} -> {} and {} -> {}",
                genericPathString(qualityPlist),
                resolvedPlist.empty() ? "(empty)" : resolvedPlist,
                genericPathString(qualityPng),
                resolvedPng.empty() ? "(empty)" : resolvedPng
            );
            if (resolvedPlist.empty() || !pathIsInside(fs::path(resolvedPlist), *expectedAppliedRoot) ||
                resolvedPng.empty() || !pathIsInside(fs::path(resolvedPng), *expectedAppliedRoot)) {
                status.iconReloadVerified = false;
                log::warn(
                    "Texture Forge runtime pack did not resolve icon files inside applied pack {}",
                    normalizedPathString(*expectedAppliedRoot)
                );
            }
        }

        auto frameKeys = frameKeysFromExistingVariants(root, base);
        if (frameKeys.empty()) {
            status.iconReloadVerified = false;
            log::warn("Texture Forge icon reload found no frame keys for {}", genericPathString(base));
            continue;
        }

        auto removed = removeSpriteFrameKeys(frameCache, frameKeys);
        forgetLoadedSpriteFrameFiles(frameCache, expectedAppliedRoot, base);
        removeTextureKeysForVariants(textureCache, base, expectedAppliedRoot);
        log::info(
            "Texture Forge removed {} cached icon frame(s) for {} ({} frame key(s) parsed)",
            removed,
            genericPathString(base),
            frameKeys.size()
        );
        for (auto index = size_t { 0 }; index < std::min<size_t>(frameKeys.size(), 5); ++index) {
            log::info("Texture Forge frame key sample {}: {}", index + 1, frameKeys[index]);
        }

        frameCache->addSpriteFramesWithFile(normalizedPathString(physicalPlist).c_str(), normalizedPathString(physicalPng).c_str());
        auto verified = verifySpriteFrameKeys(frameCache, frameKeys);
        if (!verified) {
            removeSpriteFrameKeys(frameCache, frameKeys);
            forgetLoadedSpriteFrameFiles(frameCache, expectedAppliedRoot, base);
            frameCache->addSpriteFramesWithFile(genericPathString(qualityPlist).c_str());
            verified = verifySpriteFrameKeys(frameCache, frameKeys);
        }

        if (verified) {
            ++status.iconSheetsReloaded;
            log::info(
                "Texture Forge verified icon sprite frames for {} from {}",
                genericPathString(base),
                normalizedPathString(physicalPlist)
            );
        }
        else {
            status.iconReloadVerified = false;
            log::warn(
                "Texture Forge could not verify reloaded icon sprite frames for {}",
                genericPathString(base)
            );
        }
    }
    return status;
}

void clearIconSpriteFramesFromRoot(
    fs::path const& root,
    fs::path const* appliedRoot,
    std::vector<fs::path> const& affectedPlists
) {
    auto* frameCache = CCSpriteFrameCache::get();
    auto* textureCache = CCTextureCache::get();
    for (auto const& base : logicalIconBasePlists(affectedPlists)) {
        auto frameKeys = frameKeysFromExistingVariants(root, base);
        auto removed = removeSpriteFrameKeys(frameCache, frameKeys);
        forgetLoadedSpriteFrameFiles(frameCache, appliedRoot, base);
        removeTextureKeysForVariants(textureCache, base, appliedRoot);
        log::info(
            "Texture Forge cleared {} cached icon frame(s) for {} before removing applied resources",
            removed,
            genericPathString(base)
        );
    }
}

std::vector<fs::path> runtimeLookupNamesFor(fs::path const& relative) {
    std::vector<fs::path> names { fs::path(relative.generic_string()) };
    if (isIconResource(relative)) {
        names.push_back(fs::path(relative.filename().generic_string()));
    }
    return names;
}

fs::path resourceFileForLookup(fs::path const& root, fs::path const& relative, fs::path const& lookupName) {
    if (isIconResource(relative) && lookupName == relative.filename()) {
        return root / "icons" / relative.filename();
    }
    return root / relative;
}

bool runtimeLookupExists(std::string const& lookupText) {
    auto* fileUtils = CCFileUtils::get();
    if (!fileUtils) return false;

    auto fullPath = fileUtils->fullPathForFilename(lookupText.c_str(), false);
    auto fullText = std::string(fullPath.c_str());
    if (fullText.empty()) return false;

    std::error_code ec;
    return fs::exists(fs::path(fullText), ec);
}

void removeSpriteFramesFromExistingFiles(
    CCSpriteFrameCache* frameCache,
    fs::path const& root,
    std::vector<fs::path> const& relativePlists
) {
    if (!frameCache) return;

    std::unordered_set<std::string> removed;
    std::error_code ec;
    for (auto const& relativePlist : relativePlists) {
        for (auto const& lookupName : runtimeLookupNamesFor(relativePlist)) {
            auto physical = resourceFileForLookup(root, relativePlist, lookupName);
            auto physicalText = normalizedPathString(physical);
            if (removed.contains(physicalText)) continue;
            removed.insert(physicalText);
            if (fs::exists(physical, ec)) {
                frameCache->removeSpriteFramesFromFile(physicalText.c_str());
            }
        }
    }
}

void removeSpriteFrameLookups(CCSpriteFrameCache* frameCache, std::vector<fs::path> const& relativePlists) {
    if (!frameCache) return;

    std::unordered_set<std::string> removed;
    for (auto const& relativePlist : relativePlists) {
        for (auto const& lookupName : runtimeLookupNamesFor(relativePlist)) {
            auto lookupText = genericPathString(lookupName);
            if (removed.contains(lookupText)) continue;
            removed.insert(lookupText);
            frameCache->removeSpriteFramesFromFile(lookupText.c_str());
        }
    }
}

void addSpriteFramesFromPhysicalFiles(
    CCSpriteFrameCache* frameCache,
    fs::path const& root,
    std::vector<fs::path> const& relativePlists
) {
    if (!frameCache) return;

    std::unordered_set<std::string> added;
    std::error_code ec;
    for (auto const& relativePlist : relativePlists) {
        auto physicalPlist = root / relativePlist;
        auto physicalPng = physicalPlist;
        physicalPng.replace_extension(".png");
        if (!fs::exists(physicalPlist, ec) || !fs::exists(physicalPng, ec)) continue;

        auto plistText = normalizedPathString(physicalPlist);
        if (added.contains(plistText)) continue;
        added.insert(plistText);

        frameCache->removeSpriteFramesFromFile(plistText.c_str());
        log::info(
            "Reloading Texture Forge sprite frames from {} using {}",
            normalizedPathString(physicalPlist),
            normalizedPathString(physicalPng)
        );
        frameCache->addSpriteFramesWithFile(plistText.c_str(), normalizedPathString(physicalPng).c_str());
    }
}

void noteIconSpriteFramesWereRefreshed() {
    log::info("Texture Forge refreshed icon sprite frames without changing selected player icons");
}

void removeTextureKeysFor(CCTextureCache* textureCache, fs::path const& relativePng, fs::path const* appliedRoot) {
    if (!textureCache) return;

    auto vanillaPng = resourcePath(relativePng);

    for (auto const& lookupName : runtimeLookupNamesFor(relativePng)) {
        auto lookupText = genericPathString(lookupName);
        textureCache->removeTextureForKey(lookupText.c_str());
    }
    textureCache->removeTextureForKey(pathString(vanillaPng).c_str());
    textureCache->removeTextureForKey(genericPathString(vanillaPng).c_str());

    if (appliedRoot) {
        auto appliedPng = *appliedRoot / relativePng;
        textureCache->removeTextureForKey(pathString(appliedPng).c_str());
        textureCache->removeTextureForKey(genericPathString(appliedPng).c_str());
    }
}

RuntimeRefreshStatus refreshRuntimeCaches(
    std::vector<fs::path> relativePlists,
    std::vector<fs::path> relativePngs,
    fs::path const* appliedRoot
) {
    RuntimeRefreshStatus status;
    auto* frameCache = CCSpriteFrameCache::get();
    auto* textureCache = CCTextureCache::get();
    if (!frameCache || !textureCache) {
        status.iconReloadVerified = false;
        return status;
    }

    mergePaths(relativePngs, plistTexturePngs(relativePlists));
    auto root = appliedRoot ? *appliedRoot : geode::dirs::getResourcesDir();
    auto passthroughPlists = nonIconPlists(relativePlists);
    log::info(
        "Refreshing Texture Forge runtime caches from {} ({} png lookup(s), {} plist file(s))",
        normalizedPathString(root),
        relativePngs.size(),
        relativePlists.size()
    );

    for (auto const& relativePng : relativePngs) {
        removeTextureKeysFor(textureCache, relativePng, appliedRoot);
    }

    removeSpriteFrameLookups(frameCache, passthroughPlists);
    addSpriteFramesFromPhysicalFiles(frameCache, root, passthroughPlists);

    auto iconStatus = reloadIconSpriteFramesFromRoot(root, appliedRoot, relativePlists);
    status.iconSheetsSeen = iconStatus.iconSheetsSeen;
    status.iconReloadVerified = iconStatus.iconReloadVerified;
    status.iconSheetsReloaded = iconStatus.iconSheetsReloaded;
    return status;
}

void refreshSpriteFramesFor(std::vector<fs::path> const& relativePlists) {
    (void)refreshRuntimeCaches(relativePlists, {}, nullptr);
}

std::optional<PackSummary> activeRuntimePack() {
    auto activePath = Mod::get()->getSavedValue<std::string>("active-pack-path", "");
    auto activeID = Mod::get()->getSavedValue<std::string>("active-pack", "");
    if (activePath.empty() && activeID.empty()) return std::nullopt;

    for (auto const& pack : scanPacks()) {
        if (!activePath.empty() && normalizedPathString(pack.path) == activePath) return pack;
        if (!activeID.empty() && pack.id == activeID) return pack;
    }
    return std::nullopt;
}

std::optional<std::string> iconPrefixForType(IconType type) {
    switch (type) {
        case IconType::Cube: return "player";
        case IconType::Ship: return "ship";
        case IconType::Ball: return "player_ball";
        case IconType::Ufo: return "bird";
        case IconType::Wave: return "dart";
        case IconType::Robot: return "robot";
        case IconType::Spider: return "spider";
        case IconType::Swing: return "swing";
        case IconType::Jetpack: return "jetpack";
        default: return std::nullopt;
    }
}

struct IconFrameTarget {
    IconType type;
    int id = 0;
};

std::optional<IconFrameTarget> iconTargetForFrameName(std::string frameName) {
    frameName = lowerString(std::move(frameName));
    if (!endsWith(frameName, ".png")) return std::nullopt;

    auto const mappings = std::array<std::pair<std::string_view, IconType>, 9> {
        std::pair { "player_ball", IconType::Ball },
        std::pair { "jetpack", IconType::Jetpack },
        std::pair { "player", IconType::Cube },
        std::pair { "spider", IconType::Spider },
        std::pair { "robot", IconType::Robot },
        std::pair { "swing", IconType::Swing },
        std::pair { "ship", IconType::Ship },
        std::pair { "bird", IconType::Ufo },
        std::pair { "dart", IconType::Wave },
    };

    for (auto const& [prefix, type] : mappings) {
        auto prefixText = std::string(prefix) + "_";
        if (!startsWith(frameName, prefixText)) continue;

        auto rest = frameName.substr(prefixText.size());
        auto separator = rest.find('_');
        if (separator == std::string::npos || separator == 0) return std::nullopt;

        auto numberText = rest.substr(0, separator);
        if (!std::all_of(numberText.begin(), numberText.end(), [](unsigned char c) {
            return std::isdigit(c);
        })) {
            return std::nullopt;
        }

        auto resourceNumber = 0;
        try {
            resourceNumber = std::stoi(numberText);
        }
        catch (...) {
            return std::nullopt;
        }

        auto id = resourceNumber;
        if (type == IconType::Cube || type == IconType::Ball) id = resourceNumber + 1;
        if (id <= 0) return std::nullopt;
        return IconFrameTarget { type, id };
    }
    return std::nullopt;
}

int iconResourceNumberForType(IconType type, int id) {
    if (type == IconType::Cube || type == IconType::Ball) return std::max(0, id - 1);
    return std::max(1, id);
}

bool activePackOverridesIcon(IconType type, int id) {
    auto prefix = iconPrefixForType(type);
    if (!prefix) return false;

    auto pack = activeRuntimePack();
    if (!pack) return false;

    auto number = iconResourceNumberForType(type, id);
    auto appliedRoot = appliedResourcesDir(*pack);
    std::error_code ec;
    for (auto suffix : { "", "-hd", "-uhd" }) {
        auto relative = fs::path("icons") / fmt::format("{}_{}{}.png", *prefix, iconID(number), suffix);
        if (fs::exists(appliedRoot / relative, ec)) return true;
    }
    return false;
}

bool refreshActiveIconOverride(IconType type, int id) {
    auto prefix = iconPrefixForType(type);
    if (!prefix) return false;

    auto pack = activeRuntimePack();
    if (!pack) return false;

    auto number = iconResourceNumberForType(type, id);
    auto logicalPlist = fs::path("icons") / fmt::format("{}_{}.plist", *prefix, iconID(number));
    auto appliedRoot = appliedResourcesDir(*pack);

    std::error_code ec;
    auto hasAnyVariant = false;
    for (auto const& plist : qualityPlistVariantsFor(logicalPlist)) {
        auto png = plist;
        png.replace_extension(".png");
        if (fs::exists(appliedRoot / plist, ec) && fs::exists(appliedRoot / png, ec)) {
            hasAnyVariant = true;
            break;
        }
    }
    if (!hasAnyVariant) return false;

    auto status = reloadIconSpriteFramesFromRoot(appliedRoot, &appliedRoot, { logicalPlist });
    auto refreshed = status.iconSheetsSeen && status.iconReloadVerified && status.iconSheetsReloaded > 0;
    log::info(
        "Texture Forge active icon refresh {} for {} {} ({} sheet(s) reloaded)",
        refreshed ? "succeeded" : "failed",
        *prefix,
        id,
        status.iconSheetsReloaded
    );
    return refreshed;
}

bool refreshIconOverrideForFrameName(std::string const& frameName) {
    if (iconFrameReloadInProgress()) return false;

    auto target = iconTargetForFrameName(frameName);
    if (!target) return false;
    if (!activePackOverridesIcon(target->type, target->id)) return false;

    return refreshActiveIconOverride(target->type, target->id);
}

std::string packJson(std::string const& id, std::string const& name) {
    auto json = matjson::Value::object();
    json["textureforge"] = kPackSchema;
    json["name"] = name;
    json["id"] = id;
    json["version"] = "1.0.0";
    json["author"] = "Doonc";
    return json.dump(4) + "\n";
}

Result<> writePackJson(PackSummary const& pack) {
    GEODE_UNWRAP(ensureDir(pack.path));
    return geode::utils::file::writeStringSafe(pack.path / "pack.json", packJson(pack.id, pack.name));
}

Result<PackSummary> createPack(std::string name) {
    GEODE_UNWRAP(ensureDir(packsDir()));
    auto id = uniquePackID(name);
    auto basePath = packsDir() / sanitizePart(name);
    auto path = basePath;
    auto suffix = 2;

    std::error_code ec;
    while (fs::exists(path, ec)) {
        path = fs::path(pathString(basePath) + "-" + std::to_string(suffix++));
    }

    auto pack = PackSummary { name, id, path };
    GEODE_UNWRAP(writePackJson(pack));
    GEODE_UNWRAP(ensurePackFolders(pack));
    return Ok(pack);
}

std::vector<PackSummary> scanPacks() {
    std::vector<PackSummary> packs;
    std::unordered_set<std::string> seenIDs;
    auto dir = packsDir();
    std::error_code ec;

    if (!fs::exists(dir, ec)) {
        (void)ensureDir(dir);
        return packs;
    }

    for (auto const& entry : fs::directory_iterator(dir, fs::directory_options::skip_permission_denied, ec)) {
        if (!entry.is_directory(ec)) continue;
        auto packPath = entry.path();
        auto packName = pathString(packPath.filename());
        auto packID = uniquePackID(packName);

        if (auto json = geode::utils::file::readJson(packPath / "pack.json")) {
            auto parsed = json.unwrap();
            if (auto name = readStringKey(parsed, "name")) packName = *name;
            if (auto id = readStringKey(parsed, "id")) packID = *id;
        }
        auto pack = PackSummary { packName, packID, packPath };
        (void)ensurePackFolders(pack);
        if (pack.id.empty() || seenIDs.contains(pack.id)) {
            pack.id = uniquePackID(pack.name);
            (void)writePackJson(pack);
        }
        seenIDs.insert(pack.id);
        packs.push_back(pack);
    }

    std::sort(packs.begin(), packs.end(), [](auto const& a, auto const& b) {
        return a.name < b.name;
    });
    return packs;
}
std::vector<fs::path> importFiles(PackSummary const& pack) {
    std::vector<fs::path> files;
    std::error_code ec;
    for (auto const& dir : { pack.path / "imports", pack.path / "editor" / "saves" }) {
        if (!fs::exists(dir, ec)) {
            (void)ensureDir(dir);
            continue;
        }

        for (auto const& entry : fs::directory_iterator(dir, fs::directory_options::skip_permission_denied, ec)) {
            if (entry.is_regular_file(ec) && isSupportedImport(entry.path())) {
                files.push_back(entry.path());
            }
        }
    }
    std::sort(files.begin(), files.end());
    files.erase(std::unique(files.begin(), files.end()), files.end());
    return files;
}

std::vector<fs::path> stagedOverrideFiles(PackSummary const& pack) {
    std::vector<fs::path> files;
    auto root = stagedResourcesDir(pack);
    std::error_code ec;
    if (!fs::exists(root, ec)) {
        (void)ensurePackFolders(pack);
        return files;
    }

    for (auto const& entry : fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec)) {
        if (!entry.is_regular_file(ec)) continue;
        auto relative = fs::relative(entry.path(), root, ec);
        if (!ec && lowerString(pathString(relative.extension())) == ".plist") continue;
        if (!ec) files.push_back(fs::path(relative.generic_string()));
    }
    std::sort(files.begin(), files.end());
    return files;
}

Result<> deleteStagedOverride(PackSummary const& pack, fs::path const& relative) {
    if (relative.empty() || relative.is_absolute()) return Err("Invalid override path");
    for (auto const& part : relative) {
        auto text = pathString(part);
        if (text == ".." || text == ".") return Err("Invalid override path");
    }

    auto root = stagedResourcesDir(pack);
    auto target = (root / relative).lexically_normal();
    auto rootText = normalizedPathString(root);
    auto targetText = normalizedPathString(target);
    if (!endsWith(rootText, "\\") && !endsWith(rootText, "/")) rootText += "\\";
    if (!startsWith(targetText, rootText)) return Err("Invalid override path");

    std::error_code ec;
    if (!fs::exists(target, ec)) return Err("Override does not exist");
    fs::remove(target, ec);
    if (ec) return Err("Unable to remove override: {}", ec.message());

    auto extension = lowerString(pathString(target.extension()));
    auto paired = target;
    if (extension == ".png") {
        paired.replace_extension(".plist");
        if (fs::exists(paired, ec)) fs::remove(paired, ec);
    }
    else if (extension == ".plist") {
        paired.replace_extension(".png");
        if (fs::exists(paired, ec)) fs::remove(paired, ec);
    }
    GEODE_UNWRAP(markStagedDirty(pack));
    return Ok();
}

Result<int> importSelectedFileIntoPack(PackSummary const& pack, TargetPreset const& target, fs::path const& source, bool removeImageBackground) {
    if (!fs::exists(source)) return Err("Selected file no longer exists");
    if (!isSupportedImport(source)) return Err("Unsupported file type");
    if (target.outputs.empty()) return Err("Selected target has no output textures");

    auto sourceCopyName = uniqueDestination(pack.path / "sources" / source.filename()).filename();
    auto sourceCopy = pack.path / "sources" / sourceCopyName;
    GEODE_UNWRAP(ensureDir(sourceCopy.parent_path()));
    std::error_code ec;
    fs::copy_file(source, sourceCopy, fs::copy_options::overwrite_existing, ec);
    if (ec) return Err("Unable to copy source into pack: {}", ec.message());

    if (!isImageImport(sourceCopy)) return Err("This target needs an image");
    log::info(
        "Texture Forge staging '{}' into pack '{}' target '{}' with {} output(s), background={}",
        normalizedPathString(sourceCopy),
        pack.name,
        target.label,
        target.outputs.size(),
        removeImageBackground ? "transparent" : "kept"
    );

    auto count = 0;
    for (auto const& output : target.outputs) {
        auto destination = stagedResourcesDir(pack) / output;
        GEODE_UNWRAP(writeTextureOutput(sourceCopy, destination, output, removeImageBackground));
        if (!fs::exists(destination, ec)) {
            return Err("Staged output was not written: {}", genericPathString(output));
        }
        auto bytes = fs::file_size(destination, ec);
        if (ec || bytes == 0) {
            return Err("Staged output is empty: {}", genericPathString(output));
        }
        log::info(
            "Texture Forge staged output {} -> {} ({} bytes)",
            genericPathString(output),
            normalizedPathString(destination),
            bytes
        );
        ++count;
    }
    GEODE_UNWRAP(markStagedDirty(pack));
    log::info("Texture Forge staged target '{}' in '{}' with {} output file(s)", target.label, pack.name, count);
    return Ok(count);
}

Result<> installRuntimeTexturePack(PackSummary const& pack) {
    auto appliedRoot = appliedResourcesDir(pack);
    GEODE_UNWRAP(ensureDir(appliedRoot));
    auto* fileUtils = CCFileUtils::get();
    if (!fileUtils) return Err("Unable to access Geometry Dash file utilities");

    std::vector<std::string> runtimePaths { pathString(appliedRoot) };
    std::error_code ec;
    if (fs::exists(appliedRoot / "icons", ec)) {
        runtimePaths.push_back(pathString(appliedRoot / "icons"));
    }

    fileUtils->removeTexturePack(kRuntimePackID);
    fileUtils->addTexturePack(CCTexturePack {
        .m_id = kRuntimePackID,
        .m_paths = runtimePaths,
    });
    fileUtils->purgeCachedEntries();
    for (auto const& runtimePath : runtimePaths) {
        log::info("Mounted Texture Forge runtime path: {}", runtimePath);
    }
    return Ok();
}

Result<> mountAppliedPack(PackSummary const& pack, bool reload) {
    auto appliedRoot = appliedResourcesDir(pack);
    GEODE_UNWRAP(ensureDir(appliedRoot));
    GEODE_UNWRAP(normalizeIconOutputsInRoot(appliedRoot));
    auto affectedPlists = relativePlistsIn(appliedRoot);
    auto affectedPngs = relativePngsIn(appliedRoot);
    auto affectedFiles = relativeFilesIn(appliedRoot);

    GEODE_UNWRAP(installRuntimeTexturePack(pack));
    Mod::get()->setSavedValue("active-pack", pack.id);
    Mod::get()->setSavedValue("active-pack-path", normalizedPathString(pack.path));
    log::info(
        "Applied Texture Forge pack '{}' from {} ({} file(s), {} png(s), {} plist(s))",
        pack.name,
        pathString(appliedRoot),
        affectedFiles.size(),
        affectedPngs.size(),
        affectedPlists.size()
    );
    RuntimeRefreshStatus status;
    if (reload) {
        geode::cocos::reloadTextures();
        GEODE_UNWRAP(installRuntimeTexturePack(pack));
        status = refreshRuntimeCaches(affectedPlists, affectedPngs, &appliedRoot);
    }
    else {
        status = refreshRuntimeCaches(affectedPlists, affectedPngs, &appliedRoot);
    }
    sLastApplyHadIconReloadWarnings = status.iconSheetsSeen && !status.iconReloadVerified;
    if (status.iconSheetsSeen) {
        noteIconSpriteFramesWereRefreshed();
        log::info(
            "Texture Forge icon reload finished: {} sheet(s) verified, warnings={}",
            status.iconSheetsReloaded,
            sLastApplyHadIconReloadWarnings ? "yes" : "no"
        );
    }
    return Ok();
}

Result<> applyPack(PackSummary const& pack, bool reload) {
    sLastApplyHadIconReloadWarnings = false;
    GEODE_UNWRAP(normalizeIconOutputsInRoot(stagedResourcesDir(pack)));
    GEODE_UNWRAP(commitStagedResources(pack));
    GEODE_UNWRAP(normalizeIconOutputsInRoot(appliedResourcesDir(pack)));
    return mountAppliedPack(pack, reload);
}

bool lastApplyHadIconReloadWarnings() {
    return sLastApplyHadIconReloadWarnings;
}

Result<> resetOverrides(bool reload) {
    auto packs = scanPacks();
    std::vector<fs::path> affectedPlists;
    std::vector<fs::path> affectedPngs;
    size_t clearedPacks = 0;
    size_t clearedFiles = 0;

    for (auto const& pack : packs) {
        auto appliedRoot = appliedResourcesDir(pack);
        auto packFiles = relativeFilesIn(appliedRoot);
        if (packFiles.empty()) continue;

        auto packPlists = relativePlistsIn(appliedRoot);
        auto packPngs = relativePngsIn(appliedRoot);
        mergePaths(affectedPlists, packPlists);
        mergePaths(affectedPngs, packPngs);
        clearIconSpriteFramesFromRoot(appliedRoot, &appliedRoot, packPlists);
        removeSpriteFramesFromExistingFiles(CCSpriteFrameCache::get(), appliedRoot, nonIconPlists(packPlists));
        for (auto const& relativePng : packPngs) {
            removeTextureKeysFor(CCTextureCache::get(), relativePng, &appliedRoot);
        }
        clearedFiles += packFiles.size();
        ++clearedPacks;
    }

    auto* fileUtils = CCFileUtils::get();
    if (!fileUtils) return Err("Unable to access Geometry Dash file utilities");
    fileUtils->removeTexturePack(kRuntimePackID);
    fileUtils->purgeCachedEntries();

    for (auto const& pack : packs) {
        GEODE_UNWRAP(clearDirectory(appliedResourcesDir(pack)));
    }

    Mod::get()->setSavedValue("active-pack", std::string());
    Mod::get()->setSavedValue("active-pack-path", std::string());
    log::info(
        "Reset Texture Forge overrides ({} pack(s), {} file(s), {} png(s), {} plist(s))",
        clearedPacks,
        clearedFiles,
        affectedPngs.size(),
        affectedPlists.size()
    );
    if (reload) {
        geode::cocos::reloadTextures();
        auto status = refreshRuntimeCaches(affectedPlists, affectedPngs, nullptr);
        if (status.iconSheetsSeen) noteIconSpriteFramesWereRefreshed();
    }
    else {
        (void)refreshRuntimeCaches(affectedPlists, affectedPngs, nullptr);
    }
    sLastApplyHadIconReloadWarnings = false;
    return Ok();
}

void applySavedPackIfAny() {
    auto activePath = Mod::get()->getSavedValue<std::string>("active-pack-path", "");
    if (!activePath.empty()) {
        for (auto const& pack : scanPacks()) {
            if (normalizedPathString(pack.path) == activePath) {
                if (auto result = mountAppliedPack(pack, false); !result) {
                    log::warn("Unable to restore active Texture Forge pack: {}", result.unwrapErr());
                }
                return;
            }
        }
    }

    auto activeID = Mod::get()->getSavedValue<std::string>("active-pack", "");
    if (activeID.empty()) return;
    for (auto const& pack : scanPacks()) {
        if (pack.id == activeID) {
            if (auto result = mountAppliedPack(pack, false); !result) {
                log::warn("Unable to restore active Texture Forge pack: {}", result.unwrapErr());
            }
            return;
        }
    }
}

} // namespace textureforge
