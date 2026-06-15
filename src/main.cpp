#include <Geode/Geode.hpp>
#include <Geode/binding/GameManager.hpp>
#include <Geode/modify/GameManager.hpp>
#include <Geode/modify/MoreVideoOptionsLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/SimplePlayer.hpp>

#include <cmath>

#include "TextureForge/PackManager.hpp"
#include "TextureForge/TextureForgePopup.hpp"

using namespace geode::prelude;

namespace {
    constexpr auto kApplyButtonApproxPosition = CCPoint { -18.f, -102.f };
    constexpr auto kApplyFallbackButtonSize = CCSize { 54.f, 30.f };
    constexpr auto kTextureForgeButtonGap = 8.f;
    constexpr auto kTextureForgeLabelScale = .22f;
    constexpr auto kExactTextureColor = ccColor3B { 255, 255, 255 };

    void forceNodeTreeWhite(CCNode* node) {
        if (!node) return;
        if (auto* sprite = typeinfo_cast<CCSprite*>(node)) {
            sprite->setColor(kExactTextureColor);
        }

        auto* children = node->getChildren();
        if (!children) return;
        for (auto* child : CCArrayExt<CCNode*>(children)) {
            forceNodeTreeWhite(child);
        }
    }

    void forceSimplePlayerTextureColors(SimplePlayer* player) {
        if (!player) return;
        forceNodeTreeWhite(player->m_firstLayer);
        forceNodeTreeWhite(player->m_secondLayer);
        forceNodeTreeWhite(player->m_birdDome);
        forceNodeTreeWhite(player->m_outlineSprite);
        forceNodeTreeWhite(player->m_detailSprite);
        forceNodeTreeWhite(player->m_robotSprite);
        forceNodeTreeWhite(player->m_spiderSprite);
    }

    void forcePlayerObjectTextureColors(PlayerObject* player) {
        if (!player) return;
        forceNodeTreeWhite(player->m_iconSprite);
        forceNodeTreeWhite(player->m_iconSpriteSecondary);
        forceNodeTreeWhite(player->m_iconSpriteWhitener);
        forceNodeTreeWhite(player->m_iconGlow);
        forceNodeTreeWhite(player->m_vehicleSprite);
        forceNodeTreeWhite(player->m_vehicleSpriteSecondary);
        forceNodeTreeWhite(player->m_birdVehicle);
        forceNodeTreeWhite(player->m_vehicleSpriteWhitener);
        forceNodeTreeWhite(player->m_vehicleGlow);
        forceNodeTreeWhite(player->m_robotSprite);
        forceNodeTreeWhite(player->m_spiderSprite);
    }

    bool activeManagerIconOverride(IconType type) {
        auto* manager = GameManager::get();
        if (!manager) return false;

        switch (type) {
            case IconType::Cube: return textureforge::activePackOverridesIcon(type, manager->getPlayerFrame());
            case IconType::Ship: return textureforge::activePackOverridesIcon(type, manager->getPlayerShip());
            case IconType::Ball: return textureforge::activePackOverridesIcon(type, manager->getPlayerBall());
            case IconType::Ufo: return textureforge::activePackOverridesIcon(type, manager->getPlayerBird());
            case IconType::Wave: return textureforge::activePackOverridesIcon(type, manager->getPlayerDart());
            case IconType::Robot: return textureforge::activePackOverridesIcon(type, manager->getPlayerRobot());
            case IconType::Spider: return textureforge::activePackOverridesIcon(type, manager->getPlayerSpider());
            case IconType::Swing: return textureforge::activePackOverridesIcon(type, manager->getPlayerSwing());
            case IconType::Jetpack: return textureforge::activePackOverridesIcon(type, manager->getPlayerJetpack());
            default: return false;
        }
    }
}

$on_mod(Loaded) {
    textureforge::applySavedPackIfAny();
}

class $modify(TextureForgeGameManager, GameManager) {
    cocos2d::CCTexture2D* loadIcon(int id, int type, int requestID) {
        auto* texture = GameManager::loadIcon(id, type, requestID);
        auto iconType = static_cast<IconType>(type);
        if (texture) texture->retain();
        if (textureforge::refreshActiveIconOverride(iconType, id)) {
            log::info(
                "Texture Forge reapplied active icon frames after GameManager::loadIcon id={} type={} requestID={}",
                id,
                type,
                requestID
            );
        }
        if (texture) texture->autorelease();
        return texture;
    }
};

class $modify(TextureForgeSimplePlayer, SimplePlayer) {
    struct Fields {
        bool m_textureForgeExactIcon = false;
    };

    void updatePlayerFrame(int id, IconType type) {
        auto refreshed = textureforge::refreshActiveIconOverride(type, id);
        SimplePlayer::updatePlayerFrame(id, type);
        m_fields->m_textureForgeExactIcon = textureforge::activePackOverridesIcon(type, id);
        if (m_fields->m_textureForgeExactIcon) {
            if (refreshed) {
                log::info("Texture Forge refreshed SimplePlayer frame before assignment id={}", id);
            }
            forceSimplePlayerTextureColors(this);
        }
    }

    void updateColors() {
        SimplePlayer::updateColors();
        if (m_fields->m_textureForgeExactIcon) {
            forceSimplePlayerTextureColors(this);
        }
    }

    void setColor(ccColor3B const& color) {
        SimplePlayer::setColor(m_fields->m_textureForgeExactIcon ? kExactTextureColor : color);
        if (m_fields->m_textureForgeExactIcon) {
            forceSimplePlayerTextureColors(this);
        }
    }
};

class $modify(TextureForgePlayerObject, PlayerObject) {
    struct Fields {
        bool m_cubeTextureForge = false;
        bool m_shipTextureForge = false;
        bool m_ballTextureForge = false;
        bool m_ufoTextureForge = false;
        bool m_waveTextureForge = false;
        bool m_swingTextureForge = false;
        bool m_jetpackTextureForge = false;
    };

    bool currentModeUsesTextureForgeIcon() {
        if (m_isShip) return m_fields->m_shipTextureForge;
        if (m_isBird) return m_fields->m_ufoTextureForge;
        if (m_isBall) return m_fields->m_ballTextureForge;
        if (m_isDart) return m_fields->m_waveTextureForge;
        if (m_isSwing) return m_fields->m_swingTextureForge;
        if (m_isRobot) return activeManagerIconOverride(IconType::Robot);
        if (m_isSpider) return activeManagerIconOverride(IconType::Spider);
        return m_fields->m_cubeTextureForge || (m_defaultMiniIcon && m_fields->m_jetpackTextureForge);
    }

    void refreshTextureForgeColors() {
        if (currentModeUsesTextureForgeIcon()) {
            forcePlayerObjectTextureColors(this);
        }
    }

    void update(float dt) {
        PlayerObject::update(dt);
        refreshTextureForgeColors();
    }

    void setColor(ccColor3B const& color) {
        PlayerObject::setColor(currentModeUsesTextureForgeIcon() ? kExactTextureColor : color);
        refreshTextureForgeColors();
    }

    void setSecondColor(ccColor3B const& color) {
        PlayerObject::setSecondColor(currentModeUsesTextureForgeIcon() ? kExactTextureColor : color);
        refreshTextureForgeColors();
    }

    void updatePlayerFrame(int frame) {
        (void)textureforge::refreshActiveIconOverride(IconType::Cube, frame);
        PlayerObject::updatePlayerFrame(frame);
        m_fields->m_cubeTextureForge = textureforge::activePackOverridesIcon(IconType::Cube, frame);
        refreshTextureForgeColors();
    }

    void updatePlayerShipFrame(int frame) {
        (void)textureforge::refreshActiveIconOverride(IconType::Ship, frame);
        PlayerObject::updatePlayerShipFrame(frame);
        m_fields->m_shipTextureForge = textureforge::activePackOverridesIcon(IconType::Ship, frame);
        refreshTextureForgeColors();
    }

    void updatePlayerRollFrame(int frame) {
        (void)textureforge::refreshActiveIconOverride(IconType::Ball, frame);
        PlayerObject::updatePlayerRollFrame(frame);
        m_fields->m_ballTextureForge = textureforge::activePackOverridesIcon(IconType::Ball, frame);
        refreshTextureForgeColors();
    }

    void updatePlayerBirdFrame(int frame) {
        (void)textureforge::refreshActiveIconOverride(IconType::Ufo, frame);
        PlayerObject::updatePlayerBirdFrame(frame);
        m_fields->m_ufoTextureForge = textureforge::activePackOverridesIcon(IconType::Ufo, frame);
        refreshTextureForgeColors();
    }

    void updatePlayerDartFrame(int frame) {
        (void)textureforge::refreshActiveIconOverride(IconType::Wave, frame);
        PlayerObject::updatePlayerDartFrame(frame);
        m_fields->m_waveTextureForge = textureforge::activePackOverridesIcon(IconType::Wave, frame);
        refreshTextureForgeColors();
    }

    void updatePlayerSwingFrame(int frame) {
        (void)textureforge::refreshActiveIconOverride(IconType::Swing, frame);
        PlayerObject::updatePlayerSwingFrame(frame);
        m_fields->m_swingTextureForge = textureforge::activePackOverridesIcon(IconType::Swing, frame);
        refreshTextureForgeColors();
    }

    void updatePlayerJetpackFrame(int frame) {
        (void)textureforge::refreshActiveIconOverride(IconType::Jetpack, frame);
        PlayerObject::updatePlayerJetpackFrame(frame);
        m_fields->m_jetpackTextureForge = textureforge::activePackOverridesIcon(IconType::Jetpack, frame);
        refreshTextureForgeColors();
    }
};

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
