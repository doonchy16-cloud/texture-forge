#include <Geode/Geode.hpp>
#include <Geode/modify/MoreVideoOptionsLayer.hpp>

#include <cmath>

#include "TextureForge/PackManager.hpp"
#include "TextureForge/TextureForgePopup.hpp"

using namespace geode::prelude;

namespace {
    constexpr auto kApplyButtonApproxPosition = CCPoint { -18.f, -102.f };
    constexpr auto kApplyFallbackButtonSize = CCSize { 54.f, 30.f };
    constexpr auto kTextureForgeButtonGap = 8.f;
    constexpr auto kTextureForgeLabelScale = .22f;
}

$on_mod(Loaded) {
    textureforge::applySavedPackIfAny();
}

class $modify(TextureForgeMoreVideoOptionsLayer, MoreVideoOptionsLayer) {
    static void onModify(auto& self) {
        std::ignore = self.setHookPriority("MoreVideoOptionsLayer::init", Priority::LatePost);
    }

    CCMenuItemSpriteExtra* findApplyButton() {
        if (!m_buttonMenu) return nullptr;

        auto* children = m_buttonMenu->getChildren();
        if (!children) return nullptr;

        auto* best = static_cast<CCMenuItemSpriteExtra*>(nullptr);
        auto bestScore = 1000000.f;
        for (auto* child : CCArrayExt<CCNode*>(children)) {
            if (child->getID() == "texture-forge-button"_spr || child->getID() == "texture-forge-info-button"_spr) continue;
            auto* item = typeinfo_cast<CCMenuItemSpriteExtra*>(child);
            if (!item) continue;

            auto pos = item->getPosition();
            if (pos.y > -70.f || pos.y < -135.f || pos.x > 45.f || pos.x < -80.f) continue;

            auto delta = pos - kApplyButtonApproxPosition;
            auto score = delta.x * delta.x + delta.y * delta.y;
            if (score < bestScore) {
                best = item;
                bestScore = score;
            }
        }
        return best;
    }

    CCSize applyButtonVisualSize(CCMenuItemSpriteExtra* applyButton) {
        if (!applyButton) return kApplyFallbackButtonSize;
        auto size = applyButton->getContentSize();
        return {
            size.width * applyButton->getScaleX(),
            size.height * applyButton->getScaleY(),
        };
    }

    bool init() {
        if (!MoreVideoOptionsLayer::init()) return false;
        if (m_buttonMenu->getChildByID("texture-forge-button"_spr)) return true;

        auto* applyButton = findApplyButton();
        auto applyPosition = applyButton ? applyButton->getPosition() : kApplyButtonApproxPosition;
        auto applySize = applyButtonVisualSize(applyButton);

        auto* textureForgeBtn = CCMenuItemSpriteExtra::create(
            ButtonSprite::create(
                "Texture Forge",
                static_cast<int>(std::round(applySize.width)),
                true,
                "bigFont.fnt",
                "GJ_button_04.png",
                30,
                kTextureForgeLabelScale
            ),
            this,
            menu_selector(TextureForgeMoreVideoOptionsLayer::onTextureForge)
        );
        textureForgeBtn->setID("texture-forge-button"_spr);

        auto ownSize = textureForgeBtn->getContentSize();
        if (ownSize.width > 0.f && ownSize.height > 0.f) {
            textureForgeBtn->setScaleX(applySize.width / ownSize.width);
            textureForgeBtn->setScaleY(applySize.height / ownSize.height);
        }

        auto textureForgeSize = applyButtonVisualSize(textureForgeBtn);
        textureForgeBtn->setPosition({
            applyPosition.x + (applySize.width + textureForgeSize.width) * .5f + kTextureForgeButtonGap,
            applyPosition.y,
        });
        m_buttonMenu->addChild(textureForgeBtn);

        auto* infoIcon = CCSprite::createWithSpriteFrameName("GJ_infoIcon_001.png");
        if (infoIcon) {
            infoIcon->setScale(.52f);
            auto* infoBtn = CCMenuItemSpriteExtra::create(
                infoIcon,
                this,
                menu_selector(TextureForgeMoreVideoOptionsLayer::onTextureForgeInfo)
            );
            infoBtn->setID("texture-forge-info-button"_spr);
            infoBtn->setPosition({
                textureForgeBtn->getPositionX() - textureForgeSize.width * .5f - 5.f,
                textureForgeBtn->getPositionY() + textureForgeSize.height * .68f,
            });
            m_buttonMenu->addChild(infoBtn);
        }
        return true;
    }

    void onTextureForge(CCObject*) {
        textureforge::openTextureForge();
    }

    void onTextureForgeInfo(CCObject*) {
        FLAlertLayer::create(
            "Texture Forge",
            "Build texture packs in-game. Create or select a pack, import a PNG/JPG, choose a target, then press Apply Pack. Changes are staged until Apply Pack. The normal Textures button is Geometry Dash's own video option; Texture Forge lives here in Advanced.",
            "OK"
        )->show();
    }
};
