#include <Geode/Geode.hpp>
#include <Geode/modify/CCSpriteFrameCache.hpp>

#include "TextureForge/PackManager.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>

using namespace geode::prelude;

namespace {

std::string slashPath(std::string text) {
    std::replace(text.begin(), text.end(), '\\', '/');
    return text;
}

std::string joinRuntimePath(std::string dir, std::string file) {
    dir = slashPath(std::move(dir));
    file = slashPath(std::move(file));
    while (!dir.empty() && dir.back() == '/') dir.pop_back();
    while (!file.empty() && file.front() == '/') file.erase(file.begin());
    if (dir.empty()) return file;
    if (file.empty()) return dir;
    return dir + "/" + file;
}

bool startsWith(std::string_view text, std::string_view prefix) {
    return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}

std::string plistForQuality(std::string plist, cocos2d::TextureQuality quality) {
    auto dot = plist.find_last_of('.');
    if (dot == std::string::npos) dot = plist.size();

    auto stem = plist.substr(0, dot);
    auto hasHD = stem.size() >= 3 && stem.compare(stem.size() - 3, 3, "-hd") == 0;
    auto hasUHD = stem.size() >= 4 && stem.compare(stem.size() - 4, 4, "-uhd") == 0;
    if (hasHD || hasUHD) return plist;

    if (quality == cocos2d::TextureQuality::kTextureQualityHigh) plist.insert(dot, "-uhd");
    else if (quality == cocos2d::TextureQuality::kTextureQualityMedium) plist.insert(dot, "-hd");
    return plist;
}

std::string pngForQuality(std::string const& plist, cocos2d::TextureQuality quality) {
    auto result = plistForQuality(plist, quality);
    auto dot = result.find_last_of('.');
    if (dot == std::string::npos) return result + ".png";
    result.replace(dot, result.size() - dot, ".png");
    return result;
}

std::string pngForPlist(std::string plist) {
    auto dot = plist.find_last_of('.');
    if (dot == std::string::npos) return plist + ".png";
    plist.replace(dot, plist.size() - dot, ".png");
    return plist;
}

size_t addMissingFramesFromDictionary(CCDictionary* dictionary, std::string const& texturePath) {
    if (!dictionary) return 0;

    auto* cache = CCSpriteFrameCache::get();
    auto* frames = typeinfo_cast<CCDictionary*>(dictionary->objectForKey("frames"));
    if (!cache || !frames) return 0;

    auto* metadata = typeinfo_cast<CCDictionary*>(dictionary->objectForKey("metadata"));
    auto format = metadata ? metadata->valueForKey("format")->intValue() : 0;
    if (format < 0 || format > 3) {
        log::warn("Texture Forge plist loader skipped unsupported plist format {} for {}", format, texturePath);
        return 0;
    }

    auto* texture = static_cast<CCTexture2D*>(nullptr);
    auto added = size_t { 0 };

    for (auto const& [frameName, frameObject] : CCDictionaryExt<std::string, CCObject*>(frames)) {
        auto* frameDict = typeinfo_cast<CCDictionary*>(frameObject);
        if (!frameDict || frameName.empty()) continue;
        if (cache->m_pSpriteFrames && cache->m_pSpriteFrames->objectForKey(frameName)) continue;

        if (!texture) {
            texture = CCTextureCache::sharedTextureCache()->addImage(texturePath.c_str(), false);
            if (!texture) return added;
        }

        auto* frame = new CCSpriteFrame();
        switch (format) {
            case 0: {
                auto x = frameDict->valueForKey("x")->floatValue();
                auto y = frameDict->valueForKey("y")->floatValue();
                auto w = frameDict->valueForKey("width")->floatValue();
                auto h = frameDict->valueForKey("height")->floatValue();
                auto ox = frameDict->valueForKey("offsetX")->floatValue();
                auto oy = frameDict->valueForKey("offsetY")->floatValue();
                auto ow = std::abs(frameDict->valueForKey("originalWidth")->intValue());
                auto oh = std::abs(frameDict->valueForKey("originalHeight")->intValue());
                frame->initWithTexture(texture, CCRectMake(x, y, w, h), false, CCPointMake(ox, oy), CCSizeMake(ow, oh));
                break;
            }
            case 1:
            case 2: {
                auto rect = CCRectFromString(frameDict->valueForKey("frame")->getCString());
                auto rotated = format == 2 && frameDict->valueForKey("rotated")->boolValue();
                auto offset = CCPointFromString(frameDict->valueForKey("offset")->getCString());
                auto sourceSize = CCSizeFromString(frameDict->valueForKey("sourceSize")->getCString());
                frame->initWithTexture(texture, rect, rotated, offset, sourceSize);
                break;
            }
            case 3: {
                auto spriteSize = CCSizeFromString(frameDict->valueForKey("spriteSize")->getCString());
                auto spriteOffset = CCPointFromString(frameDict->valueForKey("spriteOffset")->getCString());
                auto spriteSourceSize = CCSizeFromString(frameDict->valueForKey("spriteSourceSize")->getCString());
                auto textureRect = CCRectFromString(frameDict->valueForKey("textureRect")->getCString());
                auto rotated = frameDict->valueForKey("textureRotated")->boolValue();

                if (auto* aliases = typeinfo_cast<CCArray*>(frameDict->objectForKey("aliases"))) {
                    auto* key = new CCString(frameName);
                    for (auto index = 0u; index < aliases->count(); ++index) {
                        if (auto* alias = typeinfo_cast<CCString*>(aliases->objectAtIndex(index))) {
                            cache->m_pSpriteFramesAliases->setObject(key, alias->getCString());
                        }
                    }
                    key->release();
                }

                frame->initWithTexture(
                    texture,
                    CCRectMake(textureRect.origin.x, textureRect.origin.y, spriteSize.width, spriteSize.height),
                    rotated,
                    spriteOffset,
                    spriteSourceSize
                );
                break;
            }
        }

        cache->m_pSpriteFrames->setObject(frame, frameName);
        frame->release();
        ++added;
    }

    return added;
}

std::optional<std::string> metadataTexturePath(CCDictionary* dict, std::string const& searchPath) {
    auto* metadata = dict ? typeinfo_cast<CCDictionary*>(dict->objectForKey("metadata")) : nullptr;
    if (!metadata) return std::nullopt;
    auto* textureName = metadata->valueForKey("textureFileName");
    if (!textureName || !textureName->getCString()) return std::nullopt;
    return joinRuntimePath(searchPath, textureName->getCString());
}

} // namespace

class $modify(TextureForgeRuntimePlistLoader, CCSpriteFrameCache) {
    static void onModify(auto& self) {
        if (!self.setHookPriorityPre("CCSpriteFrameCache::addSpriteFramesWithFile", Priority::Last)) {
            log::warn("Texture Forge could not set plist loader hook priority");
        }
    }

    void addSpriteFramesWithFile(char const* plist) {
        if (!plist || !*plist) return CCSpriteFrameCache::addSpriteFramesWithFile(plist);

        auto plistText = std::string(plist);
        if (startsWith(slashPath(plistText), "icons/")) {
            return CCSpriteFrameCache::addSpriteFramesWithFile(plist);
        }

        auto* fileUtils = CCFileUtils::get();
        if (!fileUtils || fileUtils->isAbsolutePath(plistText)) {
            return CCSpriteFrameCache::addSpriteFramesWithFile(plist);
        }

        if (m_pLoadedFileNames && !m_pLoadedFileNames->insert(plist).second) return;

        auto qualityPlist = plistForQuality(plistText, CCDirector::get()->getLoadedTextureQuality());
        auto qualityPng = pngForQuality(plistText, CCDirector::get()->getLoadedTextureQuality());
        auto totalAdded = size_t { 0 };

        auto const& searchPaths = fileUtils->getSearchPaths();
        for (auto const& rawSearchPath : searchPaths) {
            auto searchPath = std::string(rawSearchPath.c_str());
            auto physicalPlist = joinRuntimePath(searchPath, qualityPlist);
            auto texturePath = std::string();

            if (!fileUtils->isFileExist(physicalPlist)) {
                auto physicalPng = joinRuntimePath(searchPath, qualityPng);
                if (!fileUtils->isFileExist(physicalPng)) continue;

                texturePath = qualityPng;
                for (auto it = searchPaths.rbegin(); it != searchPaths.rend(); ++it) {
                    auto fallbackPlist = joinRuntimePath(std::string(it->c_str()), qualityPlist);
                    if (fileUtils->isFileExist(fallbackPlist)) {
                        physicalPlist = fallbackPlist;
                        break;
                    }
                }
            }

            auto dict = geode::Ref<CCDictionary>::adopt(CCDictionary::createWithContentsOfFileThreadSafe(physicalPlist.c_str()));
            if (!dict) continue;

            if (texturePath.empty()) {
                texturePath = metadataTexturePath(dict, searchPath).value_or(pngForPlist(qualityPlist));
            }

            auto added = addMissingFramesFromDictionary(dict, texturePath);
            totalAdded += added;
            if (added > 0) {
                log::debug(
                    "Texture Forge plist loader added {} fallback frame(s) from {} using {}",
                    added,
                    physicalPlist,
                    texturePath
                );
            }
        }

        if (totalAdded == 0 && m_pLoadedFileNames) {
            m_pLoadedFileNames->erase(gd::string(plistText.c_str()));
            CCSpriteFrameCache::addSpriteFramesWithFile(plist);
        }
    }

    CCSpriteFrame* spriteFrameByName(char const* name) {
        if (name && *name && !textureforge::iconFrameReloadInProgress()) {
            if (textureforge::refreshIconOverrideForFrameName(name)) {
                log::debug("Texture Forge refreshed active icon sheet before sprite frame lookup {}", name);
            }
        }
        return CCSpriteFrameCache::spriteFrameByName(name);
    }
};
