#include "TextureForge/TextureForgePopup.hpp"

#include "TextureForge/ArchiveManager.hpp"
#include "TextureForge/IconEditorPopup.hpp"
#include "TextureForge/PackManager.hpp"
#include "TextureForge/TargetCatalog.hpp"

#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>

namespace textureforge {

CCLabelBMFont* label(std::string const& text, float scale, CCPoint position, char const* font = "bigFont.fnt") {
    auto* node = CCLabelBMFont::create(text.c_str(), font);
    node->setScale(scale);
    node->setPosition(position);
    return node;
}

CCMenuItemSpriteExtra* button(
    CCObject* target,
    SEL_MenuHandler callback,
    char const* text,
    float width,
    CCPoint position,
    char const* bg = "GJ_button_01.png",
    float height = 26.f,
    float scale = 0.55f
) {
    auto* sprite = ButtonSprite::create(text, width, true, "goldFont.fnt", bg, height, scale);
    auto* item = CCMenuItemSpriteExtra::create(sprite, target, callback);
    item->setPosition(position);
    return item;
}

class TextureForgePopup : public Popup {
protected:
    enum class Page {
        Home,
        Packs,
        Import,
        Targets,
        Apply,
        Folders,
        Overrides,
    };

    std::vector<PackSummary> m_packs;
    int m_packIndex = 0;
    int m_targetIndex = 0;
    int m_targetGroupIndex = 0;
    int m_targetPage = 0;
    int m_importFileIndex = 0;
    int m_overrideIndex = 0;
    Page m_page = Page::Home;
    CCNode* m_contentLayer = nullptr;
    CCMenu* m_pageMenu = nullptr;
    std::optional<fs::path> m_pendingFile;
    bool m_pendingRemoveBackground = false;
    bool m_renaming = false;
    bool m_editorTargetMode = false;
    std::string m_renameDraft;

    bool init() {
        if (!Popup::init(420.f, 260.f)) return false;
        setTitle("Texture Forge", "goldFont.fnt", .66f, 18.f);
        m_packIndex = Mod::get()->getSavedValue<int>("pack-index", 0);
        m_targetIndex = Mod::get()->getSavedValue<int>("target-index", 0);
        m_targetGroupIndex = Mod::get()->getSavedValue<int>("target-group-index", 0);
        m_targetPage = Mod::get()->getSavedValue<int>("target-page", 0);

        m_contentLayer = CCNode::create();
        m_contentLayer->setContentSize(m_size);
        m_mainLayer->addChild(m_contentLayer);

        m_pageMenu = CCMenu::create();
        m_pageMenu->setContentSize(m_size);
        m_pageMenu->setPosition({ 0.f, 0.f });
        m_mainLayer->addChild(m_pageMenu);

        refreshPacks();
        showHome();
        return true;
    }

    void refreshPacks() {
        (void)ensureDir(packsDir());
        (void)ensureDir(exportsDir());
        (void)ensureDir(packInboxDir());
        m_packs = scanPacks();
        m_packIndex = clampIndex(m_packIndex, m_packs.size());
        m_targetIndex = clampIndex(m_targetIndex, targetPresets().size());
        m_targetGroupIndex = clampIndex(m_targetGroupIndex, targetGroups().size());
        clampTargetPage();
        saveState();
    }

    void saveState() {
        Mod::get()->setSavedValue("pack-index", m_packIndex);
        Mod::get()->setSavedValue("target-index", m_targetIndex);
        Mod::get()->setSavedValue("target-group-index", m_targetGroupIndex);
        Mod::get()->setSavedValue("target-page", m_targetPage);
    }

    PackSummary* activePack() {
        if (m_packs.empty()) return nullptr;
        return &m_packs.at(static_cast<size_t>(m_packIndex));
    }

    TargetPreset const& activeTarget() const {
        return targetPresets().at(static_cast<size_t>(m_targetIndex));
    }

    TargetGroupRange const& activeTargetGroup() const {
        return targetGroups().at(static_cast<size_t>(m_targetGroupIndex));
    }

    int targetPageCount() const {
        constexpr auto perPage = 9;
        auto count = activeTargetGroup().count;
        return std::max(1, static_cast<int>((count + perPage - 1) / perPage));
    }

    void clampTargetPage() {
        m_targetPage = clampIndex(m_targetPage, static_cast<size_t>(targetPageCount()));
    }

    void clearPage(Page page) {
        m_page = page;
        m_contentLayer->removeAllChildren();
        m_pageMenu->removeAllChildren();
    }

    void addText(std::string const& text, float scale, CCPoint pos, char const* font = "bigFont.fnt", float maxWidth = 360.f) {
        auto* node = label(text, scale, pos, font);
        node->limitLabelWidth(maxWidth, scale, .18f);
        m_contentLayer->addChild(node);
    }

    CCMenuItemSpriteExtra* addButton(std::string const& text, SEL_MenuHandler callback, float width, CCPoint pos, char const* bg = "GJ_button_01.png") {
        auto* item = button(this, callback, text.c_str(), width, pos, bg);
        m_pageMenu->addChild(item);
        return item;
    }

    void addBackButton() {
        addButton("Back", menu_selector(TextureForgePopup::onHome), 70.f, { 55.f, 222.f }, "GJ_button_04.png");
    }

    void addPackStatus(float y = 178.f) {
        if (auto* pack = activePack()) {
            auto activeID = Mod::get()->getSavedValue<std::string>("active-pack", "");
            auto activeTag = pack->id == activeID ? "  [Applied]" : "";
            addText(fmt::format("{} / {}  {}{}", m_packIndex + 1, m_packs.size(), pack->name, activeTag), .37f, { m_size.width / 2.f, y }, "bigFont.fnt", 350.f);
        }
        else {
            addText("No packs yet - create one to start", .34f, { m_size.width / 2.f, y }, "chatFont.fnt", 330.f);
        }
    }

    void addSelectedFileStatus(float y = 146.f) {
        if (m_pendingFile) {
            auto text = pathString(m_pendingFile->filename());
            if (isImageImport(*m_pendingFile)) text += m_pendingRemoveBackground ? " | transparent" : " | keep bg";
            addText(text, .30f, { m_size.width / 2.f, y }, "chatFont.fnt", 330.f);
        }
        else {
            addText("No file selected", .30f, { m_size.width / 2.f, y }, "chatFont.fnt", 330.f);
        }
    }

    std::vector<fs::path> activeImportFiles() {
        if (auto* pack = activePack()) return importFiles(*pack);
        return {};
    }

    std::vector<fs::path> activeOverrideFiles() {
        if (auto* pack = activePack()) return stagedOverrideFiles(*pack);
        return {};
    }

    void addImportPreview(fs::path const& file) {
        if (!isImageImport(file)) return;
        auto* sprite = CCSprite::create(pathString(file).c_str());
        if (!sprite) return;
        auto size = sprite->getContentSize();
        auto scale = std::min(62.f / std::max(1.f, size.width), 36.f / std::max(1.f, size.height));
        sprite->setScale(scale);
        sprite->setPosition({ m_size.width / 2.f, 124.f });
        m_contentLayer->addChild(sprite);
    }

    void addImportBrowserStatus(float y = 134.f) {
        auto files = activeImportFiles();
        if (files.empty()) {
            addText("0 import files", .26f, { m_size.width / 2.f, y }, "chatFont.fnt", 180.f);
            return;
        }

        m_importFileIndex = clampIndex(m_importFileIndex, files.size());
        auto file = files.at(static_cast<size_t>(m_importFileIndex));
        addText(fmt::format("File {} / {}: {}", m_importFileIndex + 1, files.size(), pathString(file.filename())), .24f, { m_size.width / 2.f, y }, "chatFont.fnt", 330.f);
        addImportPreview(file);
    }

    void resetLocalState() {
        m_packIndex = 0;
        m_targetIndex = 0;
        m_targetGroupIndex = 0;
        m_targetPage = 0;
        m_importFileIndex = 0;
        m_overrideIndex = 0;
        m_pendingFile.reset();
        m_pendingRemoveBackground = false;
        saveState();
    }

    void showHome() {
        clearPage(Page::Home);
        addPackStatus(178.f);
        addSelectedFileStatus(155.f);
        addButton("Import", menu_selector(TextureForgePopup::onImportPage), 118.f, { 115.f, 112.f }, "GJ_button_01.png");
        addButton("Packs", menu_selector(TextureForgePopup::onPacksPage), 118.f, { 305.f, 112.f }, "GJ_button_01.png");
        addButton("Apply", menu_selector(TextureForgePopup::onApplyPage), 118.f, { 115.f, 66.f }, "GJ_button_02.png");
        addButton("Folders", menu_selector(TextureForgePopup::onFoldersPage), 118.f, { 305.f, 66.f }, "GJ_button_04.png");
        addButton("Overrides", menu_selector(TextureForgePopup::onOverridesPage), 130.f, { 115.f, 24.f }, "GJ_button_04.png");
        addButton("Reset All", menu_selector(TextureForgePopup::onResetOverrides), 118.f, { 305.f, 24.f }, "GJ_button_04.png");
    }

    void showPacks() {
        clearPage(Page::Packs);
        addBackButton();
        addText("Packs", .52f, { m_size.width / 2.f, 212.f }, "goldFont.fnt");
        addPackStatus(178.f);

        addButton("<", menu_selector(TextureForgePopup::onPrevPack), 42.f, { 52.f, 178.f }, "GJ_button_04.png");
        addButton(">", menu_selector(TextureForgePopup::onNextPack), 42.f, { 368.f, 178.f }, "GJ_button_04.png");

        if (m_renaming && activePack()) {
            auto* input = TextInput::create(230.f, "Pack Name", "chatFont.fnt");
            input->setString(m_renameDraft);
            input->setCommonFilter(CommonFilter::Name);
            input->setMaxCharCount(32);
            input->setCallback([this](std::string const& value) {
                m_renameDraft = value;
            });
            input->setPosition({ m_size.width / 2.f, 132.f });
            m_contentLayer->addChild(input);
            addButton("Save Name", menu_selector(TextureForgePopup::onSaveRename), 120.f, { m_size.width / 2.f, 92.f }, "GJ_button_02.png");
        }
        else {
            addButton("New Pack", menu_selector(TextureForgePopup::onNewPack), 112.f, { 115.f, 126.f });
            addButton("Rename", menu_selector(TextureForgePopup::onRenamePack), 112.f, { 305.f, 126.f }, "GJ_button_02.png");
            addButton("Export", menu_selector(TextureForgePopup::onExportPack), 112.f, { 115.f, 76.f }, "GJ_button_04.png");
            addButton("Import Pack", menu_selector(TextureForgePopup::onImportPack), 112.f, { 305.f, 76.f }, "GJ_button_04.png");
            addButton("Open Pack", menu_selector(TextureForgePopup::onOpenPack), 112.f, { m_size.width / 2.f, 34.f }, "GJ_button_04.png");
        }
    }

    void showImport() {
        clearPage(Page::Import);
        m_editorTargetMode = false;
        addBackButton();
        addText("Import", .52f, { m_size.width / 2.f, 212.f }, "goldFont.fnt");
        addPackStatus(184.f);
        if (m_pendingFile) addSelectedFileStatus(160.f);
        addImportBrowserStatus(m_pendingFile ? 144.f : 154.f);
        addButton("<", menu_selector(TextureForgePopup::onPrevImportFile), 42.f, { 62.f, 92.f }, "GJ_button_04.png");
        addButton("Use File", menu_selector(TextureForgePopup::onPickFile), 118.f, { 210.f, 92.f }, "GJ_button_02.png");
        addButton(">", menu_selector(TextureForgePopup::onNextImportFile), 42.f, { 358.f, 92.f }, "GJ_button_04.png");
        addButton("Open Folder", menu_selector(TextureForgePopup::onOpenImport), 126.f, { 115.f, 48.f }, "GJ_button_04.png");
        addButton("Icon Editor", menu_selector(TextureForgePopup::onEditorTargetPage), 150.f, { 305.f, 48.f }, "GJ_button_01.png");
    }

    void showTargets() {
        clearPage(Page::Targets);
        constexpr auto perPage = 9;
        addBackButton();
        addText(m_editorTargetMode ? "Edit Target" : "Choose Target", .48f, { m_size.width / 2.f, 214.f }, "goldFont.fnt");
        if (m_editorTargetMode) {
            addText("Pick what this editor PNG should be made for.", .24f, { m_size.width / 2.f, 192.f }, "chatFont.fnt", 330.f);
        }
        else {
            addSelectedFileStatus(192.f);
        }

        auto const& group = activeTargetGroup();
        clampTargetPage();
        addButton("<", menu_selector(TextureForgePopup::onPrevTargetGroup), 42.f, { 54.f, 164.f }, "GJ_button_04.png");
        addButton(">", menu_selector(TextureForgePopup::onNextTargetGroup), 42.f, { 366.f, 164.f }, "GJ_button_04.png");
        auto groupText = group.numbered ? fmt::format("{}  {}-{}", group.label, group.firstNumber, group.lastNumber) : group.label;
        addText(groupText, .34f, { m_size.width / 2.f, 164.f }, "goldFont.fnt", 260.f);
        if (!group.numbered) {
            addText("Raw target: may replace a whole sprite sheet.", .20f, { m_size.width / 2.f, 146.f }, "chatFont.fnt", 335.f);
        }

        auto xPositions = std::array<float, 3> { 82.f, 210.f, 338.f };
        auto yPositions = std::array<float, 3> { 126.f, 88.f, 50.f };
        auto start = group.start + static_cast<size_t>(m_targetPage * perPage);
        auto end = std::min(group.start + group.count, start + perPage);
        for (auto index = start; index < end; ++index) {
            auto local = index - start;
            auto col = local % xPositions.size();
            auto row = local / xPositions.size();
            auto const& preset = targetPresets().at(index);
            auto* item = addButton(preset.label, menu_selector(TextureForgePopup::onChooseTarget), 102.f, { xPositions[col], yPositions[row] }, "GJ_button_01.png");
            item->setTag(static_cast<int>(index));
        }
        addButton("<", menu_selector(TextureForgePopup::onPrevTargetPage), 50.f, { 80.f, 22.f }, "GJ_button_04.png");
        addButton(">", menu_selector(TextureForgePopup::onNextTargetPage), 50.f, { 340.f, 22.f }, "GJ_button_04.png");
        addText(fmt::format("Page {} / {}", m_targetPage + 1, targetPageCount()), .28f, { m_size.width / 2.f, 22.f }, "chatFont.fnt", 140.f);
    }

    void showApply() {
        clearPage(Page::Apply);
        addBackButton();
        addText("Apply", .52f, { m_size.width / 2.f, 212.f }, "goldFont.fnt");
        addPackStatus(174.f);
        addText("Texture changes are staged until Apply is pressed.", .26f, { m_size.width / 2.f, 140.f }, "chatFont.fnt", 330.f);
        addButton("Apply Pack", menu_selector(TextureForgePopup::onApplyPack), 150.f, { m_size.width / 2.f, 100.f }, "GJ_button_02.png");
        addButton("Reset Overrides", menu_selector(TextureForgePopup::onResetOverrides), 150.f, { m_size.width / 2.f, 54.f }, "GJ_button_04.png");
    }

    void showFolders() {
        clearPage(Page::Folders);
        addBackButton();
        addText("Folders", .52f, { m_size.width / 2.f, 212.f }, "goldFont.fnt");
        addPackStatus(174.f);
        addButton("Pack Folder", menu_selector(TextureForgePopup::onOpenPack), 126.f, { 115.f, 126.f });
        addButton("Import Files", menu_selector(TextureForgePopup::onOpenImport), 126.f, { 305.f, 126.f });
        addButton("All Packs", menu_selector(TextureForgePopup::onOpenAllPacks), 126.f, { 115.f, 76.f }, "GJ_button_04.png");
        addButton("Exports", menu_selector(TextureForgePopup::onOpenExports), 126.f, { 305.f, 76.f }, "GJ_button_04.png");
        addButton("Pack Imports", menu_selector(TextureForgePopup::onOpenPackInbox), 150.f, { m_size.width / 2.f, 34.f }, "GJ_button_04.png");
    }

    void showOverrides() {
        clearPage(Page::Overrides);
        addBackButton();
        addText("Overrides", .52f, { m_size.width / 2.f, 212.f }, "goldFont.fnt");
        addPackStatus(178.f);

        auto files = activeOverrideFiles();
        if (files.empty()) {
            addText("No staged overrides", .32f, { m_size.width / 2.f, 122.f }, "chatFont.fnt", 260.f);
            addText("Import a file first, then return here to remove one texture.", .24f, { m_size.width / 2.f, 92.f }, "chatFont.fnt", 330.f);
            return;
        }

        m_overrideIndex = clampIndex(m_overrideIndex, files.size());
        auto selected = files.at(static_cast<size_t>(m_overrideIndex));
        addText(fmt::format("{} / {}", m_overrideIndex + 1, files.size()), .30f, { m_size.width / 2.f, 146.f }, "chatFont.fnt", 180.f);
        addText(genericPathString(selected), .28f, { m_size.width / 2.f, 118.f }, "chatFont.fnt", 330.f);
        addButton("<", menu_selector(TextureForgePopup::onPrevOverride), 50.f, { 80.f, 76.f }, "GJ_button_04.png");
        addButton("Delete One", menu_selector(TextureForgePopup::onDeleteOverride), 150.f, { m_size.width / 2.f, 76.f }, "GJ_button_06.png");
        addButton(">", menu_selector(TextureForgePopup::onNextOverride), 50.f, { 340.f, 76.f }, "GJ_button_04.png");
        addText("Removal is staged until Apply Pack.", .25f, { m_size.width / 2.f, 34.f }, "chatFont.fnt", 300.f);
    }

    void confirmPickedFile(fs::path const& path) {
        auto filename = pathString(path.filename());
        createQuickPopup(
            "Use File?",
            filename,
            "Cancel",
            "Use",
            [this, path](FLAlertLayer*, bool use) {
                if (!use) return;
                if (!isImageImport(path)) {
                    m_pendingFile = path;
                    m_pendingRemoveBackground = false;
                    showTargets();
                    return;
                }
                createQuickPopup(
                    "Transparent BG?",
                    "Remove this image's background before applying it?",
                    "No",
                    "Yes",
                    [this, path](FLAlertLayer*, bool remove) {
                        m_pendingFile = path;
                        m_pendingRemoveBackground = remove;
                        showTargets();
                    },
                    true,
                    true
                );
            },
            true,
            true
        );
    }

    bool selectedTargetIsRaw() const {
        return !activeTargetGroup().numbered;
    }

    void stagePendingTarget() {
        auto* pack = activePack();
        if (!pack) {
            toast("Create a pack first", NotificationIcon::Warning);
            return;
        }
        if (!m_pendingFile) {
            toast("Pick a file first", NotificationIcon::Warning);
            showImport();
            return;
        }

        auto result = importSelectedFileIntoPack(*pack, activeTarget(), *m_pendingFile, m_pendingRemoveBackground);
        if (!result) {
            toast(result.unwrapErr(), NotificationIcon::Error);
            showTargets();
            return;
        }
        toast(fmt::format("Staged {}", activeTarget().label), NotificationIcon::Success);
        showApply();
    }

public:
    static TextureForgePopup* create() {
        auto ret = new TextureForgePopup;
        if (ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    void onHome(CCObject*) { m_renaming = false; m_editorTargetMode = false; showHome(); }
    void onPacksPage(CCObject*) { showPacks(); }
    void onImportPage(CCObject*) { showImport(); }
    void onApplyPage(CCObject*) { showApply(); }
    void onFoldersPage(CCObject*) { showFolders(); }
    void onOverridesPage(CCObject*) { showOverrides(); }

    void onTargetsPage(CCObject*) {
        m_editorTargetMode = false;
        if (!m_pendingFile) {
            toast("Pick a file first", NotificationIcon::Warning);
            showImport();
            return;
        }
        showTargets();
    }

    void onEditorTargetPage(CCObject*) {
        if (!activePack()) {
            toast("Create a pack first", NotificationIcon::Warning);
            return;
        }
        m_editorTargetMode = true;
        showTargets();
    }

    void onPrevTargetGroup(CCObject*) {
        m_targetGroupIndex = clampIndex(m_targetGroupIndex - 1, targetGroups().size());
        m_targetPage = 0;
        saveState();
        showTargets();
    }

    void onNextTargetGroup(CCObject*) {
        m_targetGroupIndex = clampIndex(m_targetGroupIndex + 1, targetGroups().size());
        m_targetPage = 0;
        saveState();
        showTargets();
    }

    void onPrevTargetPage(CCObject*) {
        m_targetPage = clampIndex(m_targetPage - 1, static_cast<size_t>(targetPageCount()));
        saveState();
        showTargets();
    }

    void onNextTargetPage(CCObject*) {
        m_targetPage = clampIndex(m_targetPage + 1, static_cast<size_t>(targetPageCount()));
        saveState();
        showTargets();
    }

    void onPrevPack(CCObject*) {
        m_packIndex = clampIndex(m_packIndex - 1, m_packs.size());
        saveState();
        showPacks();
    }

    void onNextPack(CCObject*) {
        m_packIndex = clampIndex(m_packIndex + 1, m_packs.size());
        saveState();
        showPacks();
    }

    void onPrevImportFile(CCObject*) {
        auto files = activeImportFiles();
        if (files.empty()) {
            toast("No import files", NotificationIcon::Warning);
            showImport();
            return;
        }
        m_importFileIndex = clampIndex(m_importFileIndex - 1, files.size());
        showImport();
    }

    void onNextImportFile(CCObject*) {
        auto files = activeImportFiles();
        if (files.empty()) {
            toast("No import files", NotificationIcon::Warning);
            showImport();
            return;
        }
        m_importFileIndex = clampIndex(m_importFileIndex + 1, files.size());
        showImport();
    }

    void onPrevOverride(CCObject*) {
        auto files = activeOverrideFiles();
        if (files.empty()) {
            showOverrides();
            return;
        }
        m_overrideIndex = clampIndex(m_overrideIndex - 1, files.size());
        showOverrides();
    }

    void onNextOverride(CCObject*) {
        auto files = activeOverrideFiles();
        if (files.empty()) {
            showOverrides();
            return;
        }
        m_overrideIndex = clampIndex(m_overrideIndex + 1, files.size());
        showOverrides();
    }

    void onDeleteOverride(CCObject*) {
        auto* pack = activePack();
        if (!pack) {
            toast("Create a pack first", NotificationIcon::Warning);
            return;
        }
        auto files = activeOverrideFiles();
        if (files.empty()) {
            showOverrides();
            return;
        }
        m_overrideIndex = clampIndex(m_overrideIndex, files.size());
        auto selected = files.at(static_cast<size_t>(m_overrideIndex));
        createQuickPopup(
            "Delete Override?",
            fmt::format("Remove {} from staged pack edits?", genericPathString(selected)),
            "Cancel",
            "Delete",
            [this, selected](FLAlertLayer*, bool remove) {
                if (!remove) return;
                auto* pack = activePack();
                if (!pack) return;
                if (auto result = deleteStagedOverride(*pack, selected); !result) {
                    toast(result.unwrapErr(), NotificationIcon::Error);
                }
                else {
                    toast("Override removal staged", NotificationIcon::Success);
                }
                showOverrides();
            },
            true,
            true
        );
    }

    void onNewPack(CCObject*) {
        auto result = createPack(fmt::format("Texture Forge Pack {}", m_packs.size() + 1));
        if (!result) {
            toast(result.unwrapErr(), NotificationIcon::Error);
            return;
        }
        auto createdPath = result.unwrap().path;
        refreshPacks();
        auto it = std::find_if(m_packs.begin(), m_packs.end(), [&](auto const& pack) {
            return pack.path == createdPath;
        });
        if (it != m_packs.end()) m_packIndex = static_cast<int>(std::distance(m_packs.begin(), it));
        toast("Pack created", NotificationIcon::Success);
        showPacks();
    }

    void onRenamePack(CCObject*) {
        auto* pack = activePack();
        if (!pack) {
            toast("Create a pack first", NotificationIcon::Warning);
            return;
        }
        m_renaming = true;
        m_renameDraft = pack->name;
        showPacks();
    }

    void onSaveRename(CCObject*) {
        auto* pack = activePack();
        if (!pack) return;
        auto trimmed = sanitizePart(m_renameDraft);
        if (trimmed.empty()) {
            toast("Name cannot be empty", NotificationIcon::Warning);
            return;
        }
        pack->name = m_renameDraft.empty() ? "Texture Forge Pack" : m_renameDraft;
        if (auto result = writePackJson(*pack); !result) {
            toast(result.unwrapErr(), NotificationIcon::Error);
            return;
        }
        m_renaming = false;
        refreshPacks();
        toast("Pack renamed", NotificationIcon::Success);
        showPacks();
    }

    void onOpenPack(CCObject*) {
        if (auto* pack = activePack()) {
            (void)ensureDir(pack->path);
            geode::utils::file::openFolder(pack->path);
        }
        else {
            toast("Create a pack first", NotificationIcon::Warning);
        }
    }

    void onOpenAllPacks(CCObject*) {
        auto path = packsDir();
        (void)ensureDir(path);
        geode::utils::file::openFolder(path);
    }

    void onOpenImport(CCObject*) {
        if (auto* pack = activePack()) {
            auto path = pack->path / "imports";
            (void)ensureDir(path);
            geode::utils::file::openFolder(path);
            showImport();
        }
        else {
            toast("Create a pack first", NotificationIcon::Warning);
        }
    }

    void onOpenExports(CCObject*) {
        (void)ensureDir(exportsDir());
        geode::utils::file::openFolder(exportsDir());
    }

    void onOpenPackInbox(CCObject*) {
        (void)ensureDir(packInboxDir());
        geode::utils::file::openFolder(packInboxDir());
    }

    void onExportPack(CCObject*) {
        auto* pack = activePack();
        if (!pack) {
            toast("Create a pack first", NotificationIcon::Warning);
            return;
        }
        auto result = exportPack(*pack);
        if (!result) {
            toast(result.unwrapErr(), NotificationIcon::Error);
            return;
        }
        toast("Pack exported", NotificationIcon::Success);
        geode::utils::file::openFolder(result.unwrap().parent_path());
    }

    void onImportPack(CCObject*) {
        auto result = importPackArchive();
        if (!result) {
            toast(result.unwrapErr(), NotificationIcon::Warning);
            geode::utils::file::openFolder(packInboxDir());
            return;
        }
        refreshPacks();
        toast("Pack imported", NotificationIcon::Success);
        showPacks();
    }

    void onPickFile(CCObject*) {
        auto* pack = activePack();
        if (!pack) {
            toast("Create a pack first", NotificationIcon::Warning);
            return;
        }
        auto files = importFiles(*pack);
        if (files.empty()) {
            m_pendingFile.reset();
            m_pendingRemoveBackground = false;
            toast("Put files in the import folder first", NotificationIcon::Warning);
            showImport();
            return;
        }
        m_importFileIndex = clampIndex(m_importFileIndex, files.size());
        confirmPickedFile(files.at(static_cast<size_t>(m_importFileIndex)));
    }

    void onChooseTarget(CCObject* sender) {
        auto* pack = activePack();
        if (!pack) {
            toast("Create a pack first", NotificationIcon::Warning);
            return;
        }
        if (!m_pendingFile) {
            toast("Pick a file first", NotificationIcon::Warning);
            showImport();
            return;
        }
        auto index = clampIndex(static_cast<CCNode*>(sender)->getTag(), targetPresets().size());
        m_targetIndex = index;
        saveState();

        if (m_editorTargetMode) {
            auto* pack = activePack();
            if (!pack) {
                toast("Create a pack first", NotificationIcon::Warning);
                return;
            }
            auto selectedPack = *pack;
            auto selectedTarget = activeTarget();
            openIconEditor(selectedPack, selectedTarget, [this, selectedPack, selectedTarget](fs::path const& savedFile) {
                m_packIndex = clampIndex(m_packIndex, m_packs.size());
                m_targetIndex = clampIndex(m_targetIndex, targetPresets().size());
                auto result = importSelectedFileIntoPack(selectedPack, selectedTarget, savedFile, false);
                if (!result) {
                    toast(result.unwrapErr(), NotificationIcon::Error);
                    return;
                }
                m_pendingFile = savedFile;
                m_pendingRemoveBackground = false;
                m_editorTargetMode = false;
                toast(fmt::format("Staged editor PNG for {}", selectedTarget.label), NotificationIcon::Success);
                refreshPacks();
                showApply();
            });
            return;
        }

        if (selectedTargetIsRaw()) {
            createQuickPopup(
                "Raw Target?",
                "This may replace a full sprite sheet. Parts of the game can disappear if the image is not made for that resource.",
                "Back",
                "Continue",
                [this](FLAlertLayer*, bool proceed) {
                    if (!proceed) {
                        showTargets();
                        return;
                    }
                    stagePendingTarget();
                },
                true,
                true
            );
            return;
        }
        stagePendingTarget();
    }

    void onApplyPack(CCObject*) {
        auto* pack = activePack();
        if (!pack) {
            toast("Create a pack first", NotificationIcon::Warning);
            return;
        }
        createQuickPopup(
            "Apply Pack?",
            fmt::format("Apply {} now? The game will reload textures.", pack->name),
            "Cancel",
            "Apply",
            [this](FLAlertLayer*, bool apply) {
                if (!apply) return;
                auto* pack = activePack();
                if (!pack) return;
                if (auto result = applyPack(*pack, true); !result) {
                    toast(result.unwrapErr(), NotificationIcon::Error);
                    return;
                }
                if (lastApplyHadIconReloadWarnings()) {
                    toast("Pack applied, but icon reload needs a restart", NotificationIcon::Warning);
                    return;
                }
                toast("Pack applied", NotificationIcon::Success);
            },
            true,
            true
        );
    }

    void onResetOverrides(CCObject*) {
        createQuickPopup(
            "Reset Overrides?",
            "Remove the currently applied Texture Forge pack and reload default textures?",
            "Cancel",
            "Reset",
            [](FLAlertLayer*, bool reset) {
                if (!reset) return;
                if (auto result = resetOverrides(true); !result) {
                    toast(result.unwrapErr(), NotificationIcon::Error);
                    return;
                }
                toast("Default textures restored", NotificationIcon::Success);
            },
            true,
            true
        );
    }
};

void openTextureForge() {
    if (auto* popup = TextureForgePopup::create()) popup->show();
}


} // namespace textureforge
