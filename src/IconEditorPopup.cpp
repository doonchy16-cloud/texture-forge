#include "TextureForge/IconEditorPopup.hpp"

#include "TextureForge/ImageProcessor.hpp"
#include "vendor/stb_image.h"
#include "vendor/stb_image_write.h"

#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>

#include <deque>
#include <queue>

namespace textureforge {

namespace {

struct Pixel {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 0;
};

enum class EditorTool {
    Pencil,
    Eraser,
    Fill,
    Line,
    Box,
    Circle,
};

enum class EditorShape {
    Square,
    Rectangle,
    Circle,
};

struct CanvasSnapshot {
    int width = 16;
    int height = 16;
    std::vector<Pixel> pixels;
};

CCLabelBMFont* smallLabel(std::string const& text, float scale, CCPoint pos, char const* font = "chatFont.fnt", float maxWidth = 160.f) {
    auto* node = CCLabelBMFont::create(text.c_str(), font);
    node->setScale(scale);
    node->setPosition(pos);
    node->limitLabelWidth(maxWidth, scale, .14f);
    return node;
}

CCMenuItemSpriteExtra* editorButton(
    CCObject* target,
    SEL_MenuHandler callback,
    char const* text,
    float width,
    CCPoint pos,
    char const* bg = "GJ_button_04.png",
    float height = 24.f,
    float scale = .46f
) {
    auto* sprite = ButtonSprite::create(text, width, true, "goldFont.fnt", bg, height, scale);
    auto* item = CCMenuItemSpriteExtra::create(sprite, target, callback);
    item->setPosition(pos);
    return item;
}

std::string cleanEditorFileStem(std::string value) {
    std::string out;
    out.reserve(value.size());
    for (auto ch : value) {
        auto c = static_cast<unsigned char>(ch);
        if (std::isalnum(c)) out.push_back(static_cast<char>(std::tolower(c)));
        else if (std::isspace(c) || ch == '-' || ch == '_') out.push_back('-');
    }
    out.erase(std::unique(out.begin(), out.end(), [](char a, char b) {
        return a == '-' && b == '-';
    }), out.end());
    while (!out.empty() && out.front() == '-') out.erase(out.begin());
    while (!out.empty() && out.back() == '-') out.pop_back();
    return out.empty() ? "icon-editor-save" : out;
}

std::string hexFor(Pixel color) {
    return fmt::format("{:02X}{:02X}{:02X}", color.r, color.g, color.b);
}

std::optional<Pixel> parseHexColor(std::string value) {
    value.erase(std::remove_if(value.begin(), value.end(), [](unsigned char c) {
        return std::isspace(c);
    }), value.end());
    if (!value.empty() && value.front() == '#') value.erase(value.begin());
    if (value.size() != 6) return std::nullopt;
    if (!std::all_of(value.begin(), value.end(), [](unsigned char c) {
        return std::isxdigit(c);
    })) {
        return std::nullopt;
    }
    auto byte = [&](size_t pos) -> std::uint8_t {
        return static_cast<std::uint8_t>(std::stoi(value.substr(pos, 2), nullptr, 16));
    };
    return Pixel { byte(0), byte(2), byte(4), 255 };
}

fs::path editorSavesDir(PackSummary const& pack) {
    return pack.path / "editor" / "saves";
}

EditorShape shapeFor(TargetPreset const& target) {
    auto label = lowerString(target.label);
    if (label.find("ball") != std::string::npos) return EditorShape::Circle;
    if (target.outputs.empty()) return EditorShape::Square;

    auto output = target.outputs.front();
    if (lowerString(genericPathString(output.parent_path())) == "icons") return EditorShape::Square;

    int width = 0;
    int height = 0;
    int channels = 0;
    auto original = resourcePath(output);
    if (stbi_info(pathString(original).c_str(), &width, &height, &channels) && width > 0 && height > 0) {
        auto ratio = static_cast<float>(width) / static_cast<float>(height);
        if (ratio > 1.25f || ratio < .8f) return EditorShape::Rectangle;
    }
    return EditorShape::Square;
}

std::pair<int, int> defaultCanvasSizeFor(TargetPreset const& target, EditorShape shape) {
    if (shape != EditorShape::Rectangle || target.outputs.empty()) return { 16, 16 };
    int width = 0;
    int height = 0;
    int channels = 0;
    auto original = resourcePath(target.outputs.front());
    if (!stbi_info(pathString(original).c_str(), &width, &height, &channels) || width <= 0 || height <= 0) {
        return { 32, 16 };
    }

    auto ratio = static_cast<float>(width) / static_cast<float>(height);
    auto canvasWidth = std::clamp(static_cast<int>(std::round(16.f * ratio)), 4, 256);
    return { std::max(4, canvasWidth), 16 };
}

bool shapeAllowsPixel(EditorShape shape, int x, int y, int width, int height) {
    if (shape != EditorShape::Circle) return true;
    auto cx = (static_cast<float>(width) - 1.f) * .5f;
    auto cy = (static_cast<float>(height) - 1.f) * .5f;
    auto rx = std::max(.5f, static_cast<float>(width) * .5f);
    auto ry = std::max(.5f, static_cast<float>(height) * .5f);
    auto nx = (static_cast<float>(x) - cx) / rx;
    auto ny = (static_cast<float>(y) - cy) / ry;
    return nx * nx + ny * ny <= 1.f;
}

class PixelCanvasLayer : public CCLayerColor {
public:
    std::function<void(CCPoint const&, bool, bool)> m_callback;

    static PixelCanvasLayer* create(CCSize size, std::function<void(CCPoint const&, bool, bool)> callback) {
        auto* ret = new PixelCanvasLayer;
        if (ret && ret->initWithSize(size, std::move(callback))) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    bool initWithSize(CCSize size, std::function<void(CCPoint const&, bool, bool)> callback) {
        if (!CCLayerColor::initWithColor({ 255, 255, 255, 22 }, size.width, size.height)) return false;
        m_callback = std::move(callback);
        setTouchEnabled(true);
        setTouchMode(kCCTouchesOneByOne);
        setTouchPriority(-501);
        setAnchorPoint({ 0.f, 0.f });
        return true;
    }

    bool ccTouchBegan(CCTouch* touch, CCEvent*) override {
        auto point = convertTouchToNodeSpace(touch);
        auto size = getContentSize();
        if (point.x < 0.f || point.y < 0.f || point.x > size.width || point.y > size.height) return false;
        if (m_callback) m_callback(point, true, false);
        return true;
    }

    void ccTouchMoved(CCTouch* touch, CCEvent*) override {
        if (m_callback) m_callback(convertTouchToNodeSpace(touch), false, false);
    }

    void ccTouchEnded(CCTouch* touch, CCEvent*) override {
        if (m_callback) m_callback(convertTouchToNodeSpace(touch), false, true);
    }
};

} // namespace

class IconEditorPopup : public Popup {
protected:
    PackSummary m_pack;
    TargetPreset m_target;
    EditorSaveCallback m_onSave;
    EditorTool m_tool = EditorTool::Pencil;
    EditorShape m_shape = EditorShape::Square;
    int m_width = 16;
    int m_height = 16;
    int m_brush = 1;
    int m_zoomStep = 0;
    bool m_filledShapes = false;
    bool m_showGrid = true;
    bool m_unsaved = false;
    bool m_dragging = false;
    CCPoint m_dragStart = { 0.f, 0.f };
    CCPoint m_lastPoint = { -1.f, -1.f };
    Pixel m_color { 39, 219, 227, 255 };
    std::string m_fileStem = "icon-editor-save";
    std::string m_hexDraft = "27DBE3";
    std::vector<Pixel> m_pixels;
    std::vector<Pixel> m_clipboard;
    std::deque<CanvasSnapshot> m_undo;
    std::deque<CanvasSnapshot> m_redo;
    PixelCanvasLayer* m_canvasLayer = nullptr;
    CCNode* m_canvasContent = nullptr;
    CCMenu* m_menu = nullptr;
    CCLabelBMFont* m_statusLabel = nullptr;

    bool init(PackSummary pack, TargetPreset target, EditorSaveCallback onSave) {
        if (!Popup::init(440.f, 280.f)) return false;
        m_pack = std::move(pack);
        m_target = std::move(target);
        m_onSave = std::move(onSave);
        m_shape = shapeFor(m_target);
        auto [width, height] = defaultCanvasSizeFor(m_target, m_shape);
        resizeCanvas(width, height, false);
        setTitle("Icon Editor", "goldFont.fnt", .62f, 18.f);
        setKeyboardEnabled(true);

        m_menu = CCMenu::create();
        m_menu->setContentSize(m_size);
        m_menu->setPosition({ 0.f, 0.f });
        m_mainLayer->addChild(m_menu);

        buildUI();
        return true;
    }

    void buildUI() {
        m_mainLayer->removeChildByID("editor-body");
        m_menu->removeAllChildren();

        auto* body = CCNode::create();
        body->setID("editor-body");
        body->setContentSize(m_size);
        m_mainLayer->addChild(body);

        auto* targetLabel = smallLabel(fmt::format("Target: {}", m_target.label), .24f, { 220.f, 231.f }, "chatFont.fnt", 340.f);
        body->addChild(targetLabel);

        auto boardSize = currentBoardSize();
        auto boardOrigin = CCPoint { 32.f, 62.f };
        m_canvasLayer = PixelCanvasLayer::create({ boardSize, boardSize }, [this](CCPoint const& point, bool begin, bool end) {
            handleCanvasTouch(point, begin, end);
        });
        m_canvasLayer->setPosition(boardOrigin);
        body->addChild(m_canvasLayer);

        m_canvasContent = CCNode::create();
        m_canvasContent->setContentSize({ boardSize, boardSize });
        m_canvasContent->setPosition({ 0.f, 0.f });
        m_canvasLayer->addChild(m_canvasContent);
        redrawCanvas();

        auto* boardFrame = CCScale9Sprite::create("square02_001.png");
        boardFrame->setContentSize({ boardSize + 8.f, boardSize + 8.f });
        boardFrame->setOpacity(90);
        boardFrame->setPosition({ boardOrigin.x + boardSize / 2.f, boardOrigin.y + boardSize / 2.f });
        body->addChild(boardFrame, -1);

        auto* fileInput = TextInput::create(146.f, "filename", "chatFont.fnt");
        fileInput->setString(m_fileStem);
        fileInput->setCommonFilter(CommonFilter::Name);
        fileInput->setMaxCharCount(32);
        fileInput->setCallback([this](std::string const& value) {
            m_fileStem = cleanEditorFileStem(value);
        });
        fileInput->setPosition({ 324.f, 206.f });
        body->addChild(fileInput);

        auto* hexInput = TextInput::create(82.f, "hex", "chatFont.fnt");
        hexInput->setString(m_hexDraft);
        hexInput->setMaxCharCount(7);
        hexInput->setCallback([this](std::string const& value) {
            m_hexDraft = value;
        });
        hexInput->setPosition({ 292.f, 174.f });
        body->addChild(hexInput);

        m_statusLabel = smallLabel(statusText(), .22f, { 330.f, 149.f }, "chatFont.fnt", 190.f);
        body->addChild(m_statusLabel);

        auto add = [&](std::string const& text, SEL_MenuHandler cb, float width, CCPoint pos, char const* bg = "GJ_button_04.png") {
            m_menu->addChild(editorButton(this, cb, text.c_str(), width, pos, bg));
        };

        add("Pencil", menu_selector(IconEditorPopup::onPencil), 58.f, { 228.f, 116.f }, m_tool == EditorTool::Pencil ? "GJ_button_01.png" : "GJ_button_04.png");
        add("Erase", menu_selector(IconEditorPopup::onEraser), 58.f, { 292.f, 116.f }, m_tool == EditorTool::Eraser ? "GJ_button_01.png" : "GJ_button_04.png");
        add("Fill", menu_selector(IconEditorPopup::onFill), 58.f, { 356.f, 116.f }, m_tool == EditorTool::Fill ? "GJ_button_01.png" : "GJ_button_04.png");
        add("Line", menu_selector(IconEditorPopup::onLine), 54.f, { 222.f, 84.f }, m_tool == EditorTool::Line ? "GJ_button_01.png" : "GJ_button_04.png");
        add("Box", menu_selector(IconEditorPopup::onBox), 54.f, { 282.f, 84.f }, m_tool == EditorTool::Box ? "GJ_button_01.png" : "GJ_button_04.png");
        add("Circle", menu_selector(IconEditorPopup::onCircle), 64.f, { 348.f, 84.f }, m_tool == EditorTool::Circle ? "GJ_button_01.png" : "GJ_button_04.png");
        add(m_filledShapes ? "Solid" : "Outline", menu_selector(IconEditorPopup::onToggleFilled), 78.f, { 254.f, 52.f }, m_filledShapes ? "GJ_button_02.png" : "GJ_button_04.png");
        add(m_showGrid ? "Grid On" : "Grid Off", menu_selector(IconEditorPopup::onToggleGrid), 78.f, { 344.f, 52.f }, m_showGrid ? "GJ_button_02.png" : "GJ_button_04.png");

        add("Set", menu_selector(IconEditorPopup::onSetHex), 44.f, { 374.f, 174.f }, "GJ_button_02.png");
        add("Save", menu_selector(IconEditorPopup::onSave), 74.f, { 326.f, 24.f }, "GJ_button_01.png");
        add("Undo", menu_selector(IconEditorPopup::onUndo), 54.f, { 200.f, 24.f });
        add("Redo", menu_selector(IconEditorPopup::onRedo), 54.f, { 258.f, 24.f });

        add("B-", menu_selector(IconEditorPopup::onBrushDown), 42.f, { 210.f, 174.f });
        add("B+", menu_selector(IconEditorPopup::onBrushUp), 42.f, { 250.f, 174.f });
        add("S-", menu_selector(IconEditorPopup::onSizeDown), 42.f, { 210.f, 206.f });
        add("S+", menu_selector(IconEditorPopup::onSizeUp), 42.f, { 250.f, 206.f });
        add("Mirror", menu_selector(IconEditorPopup::onMirror), 66.f, { 108.f, 42.f });
        add("Rotate", menu_selector(IconEditorPopup::onRotate), 66.f, { 42.f, 42.f });
        add("Copy", menu_selector(IconEditorPopup::onCopy), 54.f, { 42.f, 18.f });
        add("Paste", menu_selector(IconEditorPopup::onPaste), 60.f, { 104.f, 18.f });
    }

    float currentBoardSize() const {
        auto base = 146.f;
        return std::clamp(base + static_cast<float>(m_zoomStep) * 16.f, 110.f, 178.f);
    }

    std::string statusText() const {
        auto shape = m_shape == EditorShape::Circle ? "Circle" : (m_shape == EditorShape::Rectangle ? "Rect" : "Square");
        return fmt::format("{}x{} {} | Brush {} | #{}", m_width, m_height, shape, m_brush, hexFor(m_color));
    }

    int pixelIndex(int x, int y) const {
        return y * m_width + x;
    }

    bool inCanvas(int x, int y) const {
        return x >= 0 && y >= 0 && x < m_width && y < m_height && shapeAllowsPixel(m_shape, x, y, m_width, m_height);
    }

    std::pair<int, int> pointToPixel(CCPoint const& point) const {
        auto size = currentBoardSize();
        auto x = std::clamp(static_cast<int>(std::floor(point.x * m_width / std::max(1.f, size))), 0, m_width - 1);
        auto y = std::clamp(m_height - 1 - static_cast<int>(std::floor(point.y * m_height / std::max(1.f, size))), 0, m_height - 1);
        return { x, y };
    }

    void pushUndo() {
        m_undo.push_back(CanvasSnapshot { m_width, m_height, m_pixels });
        while (m_undo.size() > 15) m_undo.pop_front();
        m_redo.clear();
    }

    void restoreSnapshot(CanvasSnapshot const& snapshot) {
        m_width = snapshot.width;
        m_height = snapshot.height;
        m_pixels = snapshot.pixels;
        m_brush = std::min(m_brush, maxBrush());
        markUnsaved();
        buildUI();
    }

    void markUnsaved() {
        m_unsaved = true;
    }

    int maxBrush() const {
        return std::max(1, static_cast<int>(std::ceil(std::max(m_width, m_height) * .75f)));
    }

    void resizeCanvas(int width, int height, bool keep) {
        width = std::clamp(width, 4, 256);
        height = std::clamp(height, 4, 256);
        std::vector<Pixel> next(static_cast<size_t>(width) * height);
        if (keep && !m_pixels.empty()) {
            for (auto y = 0; y < height; ++y) {
                auto oldY = std::clamp(static_cast<int>((y + .5f) * m_height / height), 0, m_height - 1);
                for (auto x = 0; x < width; ++x) {
                    auto oldX = std::clamp(static_cast<int>((x + .5f) * m_width / width), 0, m_width - 1);
                    next[static_cast<size_t>(y) * width + x] = m_pixels[static_cast<size_t>(oldY) * m_width + oldX];
                }
            }
        }
        m_width = width;
        m_height = height;
        m_pixels = std::move(next);
        enforceShapeMask();
        m_brush = std::min(m_brush, maxBrush());
    }

    void enforceShapeMask() {
        for (auto y = 0; y < m_height; ++y) {
            for (auto x = 0; x < m_width; ++x) {
                if (!shapeAllowsPixel(m_shape, x, y, m_width, m_height)) {
                    m_pixels[static_cast<size_t>(pixelIndex(x, y))] = {};
                }
            }
        }
    }

    void redrawCanvas() {
        if (!m_canvasContent) return;
        m_canvasContent->removeAllChildren();
        auto board = currentBoardSize();
        auto render = std::vector<std::uint8_t>(static_cast<size_t>(m_width) * m_height * 4);
        for (auto y = 0; y < m_height; ++y) {
            for (auto x = 0; x < m_width; ++x) {
                auto pixel = m_pixels[static_cast<size_t>(pixelIndex(x, y))];
                auto out = render.data() + static_cast<size_t>(pixelIndex(x, y)) * 4;
                if (pixel.a == 0) {
                    auto light = ((x + y) % 2) == 0 ? 238 : 205;
                    out[0] = static_cast<std::uint8_t>(light);
                    out[1] = static_cast<std::uint8_t>(light);
                    out[2] = static_cast<std::uint8_t>(light);
                    out[3] = 255;
                }
                else {
                    out[0] = pixel.r;
                    out[1] = pixel.g;
                    out[2] = pixel.b;
                    out[3] = pixel.a;
                }
            }
        }

        auto* texture = new CCTexture2D;
        if (texture->initWithData(render.data(), kCCTexture2DPixelFormat_RGBA8888, m_width, m_height, { static_cast<float>(m_width), static_cast<float>(m_height) })) {
            texture->setAliasTexParameters();
            texture->autorelease();
            auto* sprite = CCSprite::createWithTexture(texture);
            sprite->setAnchorPoint({ 0.f, 0.f });
            sprite->setScaleX(board / std::max(1, m_width));
            sprite->setScaleY(board / std::max(1, m_height));
            m_canvasContent->addChild(sprite);
        }
        else {
            delete texture;
        }

        if (m_showGrid && m_width <= 64 && m_height <= 64) {
            auto* grid = CCDrawNode::create();
            auto color = ccc4f(0.f, 0.f, 0.f, .18f);
            for (auto x = 0; x <= m_width; ++x) {
                auto px = board * x / m_width;
                grid->drawSegment({ px, 0.f }, { px, board }, .22f, color);
            }
            for (auto y = 0; y <= m_height; ++y) {
                auto py = board * y / m_height;
                grid->drawSegment({ 0.f, py }, { board, py }, .22f, color);
            }
            m_canvasContent->addChild(grid);
        }
    }

    void setPixel(int x, int y, Pixel pixel) {
        if (!inCanvas(x, y)) return;
        m_pixels[static_cast<size_t>(pixelIndex(x, y))] = pixel;
    }

    void drawBrushAt(int x, int y, Pixel pixel) {
        auto radius = std::max(0, m_brush - 1);
        for (auto yy = y - radius; yy <= y + radius; ++yy) {
            for (auto xx = x - radius; xx <= x + radius; ++xx) {
                auto dx = xx - x;
                auto dy = yy - y;
                if (dx * dx + dy * dy <= radius * radius + radius) setPixel(xx, yy, pixel);
            }
        }
    }

    void drawLine(int x0, int y0, int x1, int y1, Pixel pixel) {
        auto dx = std::abs(x1 - x0);
        auto sx = x0 < x1 ? 1 : -1;
        auto dy = -std::abs(y1 - y0);
        auto sy = y0 < y1 ? 1 : -1;
        auto err = dx + dy;
        while (true) {
            drawBrushAt(x0, y0, pixel);
            if (x0 == x1 && y0 == y1) break;
            auto e2 = 2 * err;
            if (e2 >= dy) {
                err += dy;
                x0 += sx;
            }
            if (e2 <= dx) {
                err += dx;
                y0 += sy;
            }
        }
    }

    void drawBox(int x0, int y0, int x1, int y1, Pixel pixel) {
        auto minX = std::min(x0, x1);
        auto maxX = std::max(x0, x1);
        auto minY = std::min(y0, y1);
        auto maxY = std::max(y0, y1);
        if (m_filledShapes) {
            for (auto y = minY; y <= maxY; ++y) {
                for (auto x = minX; x <= maxX; ++x) setPixel(x, y, pixel);
            }
        }
        else {
            drawLine(minX, minY, maxX, minY, pixel);
            drawLine(maxX, minY, maxX, maxY, pixel);
            drawLine(maxX, maxY, minX, maxY, pixel);
            drawLine(minX, maxY, minX, minY, pixel);
        }
    }

    void drawEllipse(int x0, int y0, int x1, int y1, Pixel pixel) {
        auto minX = std::min(x0, x1);
        auto maxX = std::max(x0, x1);
        auto minY = std::min(y0, y1);
        auto maxY = std::max(y0, y1);
        auto cx = (minX + maxX) * .5f;
        auto cy = (minY + maxY) * .5f;
        auto rx = std::max(.5f, (maxX - minX + 1) * .5f);
        auto ry = std::max(.5f, (maxY - minY + 1) * .5f);
        for (auto y = minY; y <= maxY; ++y) {
            for (auto x = minX; x <= maxX; ++x) {
                auto nx = (x - cx) / rx;
                auto ny = (y - cy) / ry;
                auto value = nx * nx + ny * ny;
                if (m_filledShapes) {
                    if (value <= 1.f) setPixel(x, y, pixel);
                }
                else if (std::abs(value - 1.f) <= std::max(.14f, 1.8f / std::max(rx, ry))) {
                    drawBrushAt(x, y, pixel);
                }
            }
        }
    }

    void floodFill(int startX, int startY, Pixel replacement) {
        if (!inCanvas(startX, startY)) return;
        auto target = m_pixels[static_cast<size_t>(pixelIndex(startX, startY))];
        if (std::memcmp(&target, &replacement, sizeof(Pixel)) == 0) return;

        std::queue<std::pair<int, int>> queue;
        std::vector<std::uint8_t> seen(static_cast<size_t>(m_width) * m_height, 0);
        queue.push({ startX, startY });
        seen[static_cast<size_t>(pixelIndex(startX, startY))] = 1;
        while (!queue.empty()) {
            auto [x, y] = queue.front();
            queue.pop();
            auto index = static_cast<size_t>(pixelIndex(x, y));
            auto current = m_pixels[index];
            if (std::memcmp(&current, &target, sizeof(Pixel)) != 0) continue;
            m_pixels[index] = replacement;
            for (auto [nx, ny] : { std::pair { x + 1, y }, std::pair { x - 1, y }, std::pair { x, y + 1 }, std::pair { x, y - 1 } }) {
                if (!inCanvas(nx, ny)) continue;
                auto nextIndex = static_cast<size_t>(pixelIndex(nx, ny));
                if (seen[nextIndex]) continue;
                seen[nextIndex] = 1;
                queue.push({ nx, ny });
            }
        }
    }

    void handleCanvasTouch(CCPoint const& point, bool begin, bool end) {
        auto [x, y] = pointToPixel(point);
        auto pixel = m_tool == EditorTool::Eraser ? Pixel {} : m_color;
        if (begin) {
            pushUndo();
            m_dragging = true;
            m_dragStart = point;
            m_lastPoint = CCPoint(static_cast<float>(x), static_cast<float>(y));
            if (m_tool == EditorTool::Fill) {
                floodFill(x, y, pixel);
                finishStroke();
                return;
            }
            if (m_tool == EditorTool::Pencil || m_tool == EditorTool::Eraser) {
                drawBrushAt(x, y, pixel);
                markUnsaved();
                redrawCanvas();
            }
            return;
        }

        if (!m_dragging) return;
        if (end) {
            auto [sx, sy] = pointToPixel(m_dragStart);
            if (m_tool == EditorTool::Line) drawLine(sx, sy, x, y, pixel);
            else if (m_tool == EditorTool::Box) drawBox(sx, sy, x, y, pixel);
            else if (m_tool == EditorTool::Circle) drawEllipse(sx, sy, x, y, pixel);
            finishStroke();
            return;
        }

        if (m_tool == EditorTool::Pencil || m_tool == EditorTool::Eraser) {
            auto lx = static_cast<int>(m_lastPoint.x);
            auto ly = static_cast<int>(m_lastPoint.y);
            drawLine(lx, ly, x, y, pixel);
            m_lastPoint = CCPoint(static_cast<float>(x), static_cast<float>(y));
            markUnsaved();
            redrawCanvas();
        }
    }

    void finishStroke() {
        m_dragging = false;
        markUnsaved();
        redrawCanvas();
    }

    Result<fs::path> writeSaveFile(bool overwrite) {
        auto stem = cleanEditorFileStem(m_fileStem);
        auto destination = editorSavesDir(m_pack) / fmt::format("{}.png", stem);
        std::error_code ec;
        if (fs::exists(destination, ec) && !overwrite) return Err("duplicate");
        GEODE_UNWRAP(ensureDir(destination.parent_path()));

        std::vector<std::uint8_t> out(static_cast<size_t>(m_width) * m_height * 4);
        for (auto y = 0; y < m_height; ++y) {
            for (auto x = 0; x < m_width; ++x) {
                auto pixel = m_pixels[static_cast<size_t>(pixelIndex(x, y))];
                if (!shapeAllowsPixel(m_shape, x, y, m_width, m_height)) pixel = {};
                auto* dst = out.data() + static_cast<size_t>(pixelIndex(x, y)) * 4;
                dst[0] = pixel.r;
                dst[1] = pixel.g;
                dst[2] = pixel.b;
                dst[3] = pixel.a;
            }
        }

        if (!stbi_write_png(pathString(destination).c_str(), m_width, m_height, 4, out.data(), m_width * 4)) {
            return Err("Unable to save editor PNG");
        }
        m_fileStem = stem;
        m_unsaved = false;
        return Ok(destination);
    }

    void saveThenUse(bool overwrite) {
        auto result = writeSaveFile(overwrite);
        if (!result) {
            if (result.unwrapErr() == "duplicate") {
                createQuickPopup(
                    "Replace File?",
                    "That editor file already exists. Rename it or replace the older version?",
                    "Rename",
                    "Replace",
                    [this](FLAlertLayer*, bool replace) {
                        if (replace) saveThenUse(true);
                    },
                    true,
                    true
                );
                return;
            }
            toast(result.unwrapErr(), NotificationIcon::Error);
            return;
        }

        auto saved = result.unwrap();
        createQuickPopup(
            "Use Drawing?",
            fmt::format("Save complete. Stage this PNG for {}?", m_target.label),
            "Later",
            "Use",
            [this, saved](FLAlertLayer*, bool use) {
                if (!use) {
                    toast("Drawing saved", NotificationIcon::Success);
                    return;
                }
                if (m_onSave) m_onSave(saved);
                onClose(nullptr);
            },
            true,
            true
        );
    }

public:
    static IconEditorPopup* create(PackSummary pack, TargetPreset target, EditorSaveCallback onSave) {
        auto* ret = new IconEditorPopup;
        if (ret && ret->init(std::move(pack), std::move(target), std::move(onSave))) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    void keyDown(enumKeyCodes key, double dt) override {
        if (key == KEY_Z) onUndo(nullptr);
        else if (key == KEY_Y) onRedo(nullptr);
        else if (key == KEY_C) onCopy(nullptr);
        else if (key == KEY_V) onPaste(nullptr);
        else Popup::keyDown(key, dt);
    }

    void onClose(CCObject* sender) override {
        if (!m_unsaved) {
            Popup::onClose(sender);
            return;
        }
        auto autosave = writeSaveFile(true);
        if (autosave) {
            toast("Autosaved editor drawing before closing", NotificationIcon::Info);
        }
        Popup::onClose(sender);
    }

    void setTool(EditorTool tool) {
        m_tool = tool;
        buildUI();
    }

    void onPencil(CCObject*) { setTool(EditorTool::Pencil); }
    void onEraser(CCObject*) { setTool(EditorTool::Eraser); }
    void onFill(CCObject*) { setTool(EditorTool::Fill); }
    void onLine(CCObject*) { setTool(EditorTool::Line); }
    void onBox(CCObject*) { setTool(EditorTool::Box); }
    void onCircle(CCObject*) { setTool(EditorTool::Circle); }

    void onToggleFilled(CCObject*) {
        m_filledShapes = !m_filledShapes;
        buildUI();
    }

    void onToggleGrid(CCObject*) {
        m_showGrid = !m_showGrid;
        redrawCanvas();
        buildUI();
    }

    void onSetHex(CCObject*) {
        auto parsed = parseHexColor(m_hexDraft);
        if (!parsed) {
            toast("Use a color like #27DBE3", NotificationIcon::Warning);
            return;
        }
        m_color = *parsed;
        m_hexDraft = hexFor(m_color);
        buildUI();
    }

    void onBrushDown(CCObject*) {
        m_brush = std::max(1, m_brush - 1);
        buildUI();
    }

    void onBrushUp(CCObject*) {
        m_brush = std::min(maxBrush(), m_brush + 1);
        buildUI();
    }

    void onSizeDown(CCObject*) {
        pushUndo();
        auto nextWidth = std::max(4, m_width / 2);
        auto nextHeight = std::max(4, m_height / 2);
        resizeCanvas(nextWidth, nextHeight, true);
        markUnsaved();
        buildUI();
    }

    void onSizeUp(CCObject*) {
        pushUndo();
        auto nextWidth = std::min(256, m_width * 2);
        auto nextHeight = std::min(256, m_height * 2);
        resizeCanvas(nextWidth, nextHeight, true);
        markUnsaved();
        buildUI();
    }

    void onMirror(CCObject*) {
        pushUndo();
        for (auto y = 0; y < m_height; ++y) {
            for (auto x = 0; x < m_width / 2; ++x) {
                std::swap(m_pixels[static_cast<size_t>(pixelIndex(x, y))], m_pixels[static_cast<size_t>(pixelIndex(m_width - 1 - x, y))]);
            }
        }
        markUnsaved();
        redrawCanvas();
    }

    void onRotate(CCObject*) {
        pushUndo();
        std::vector<Pixel> rotated(static_cast<size_t>(m_width) * m_height);
        auto oldWidth = m_width;
        auto oldHeight = m_height;
        for (auto y = 0; y < oldHeight; ++y) {
            for (auto x = 0; x < oldWidth; ++x) {
                auto nx = oldHeight - 1 - y;
                auto ny = x;
                rotated[static_cast<size_t>(ny) * oldHeight + nx] = m_pixels[static_cast<size_t>(y) * oldWidth + x];
            }
        }
        m_width = oldHeight;
        m_height = oldWidth;
        m_pixels = std::move(rotated);
        enforceShapeMask();
        markUnsaved();
        buildUI();
    }

    void onCopy(CCObject*) {
        m_clipboard = m_pixels;
        toast("Copied drawing", NotificationIcon::Success);
    }

    void onPaste(CCObject*) {
        if (m_clipboard.empty()) {
            toast("Nothing copied", NotificationIcon::Warning);
            return;
        }
        if (m_clipboard.size() != m_pixels.size()) {
            toast("Copied drawing has a different size", NotificationIcon::Warning);
            return;
        }
        pushUndo();
        m_pixels = m_clipboard;
        enforceShapeMask();
        markUnsaved();
        redrawCanvas();
    }

    void onUndo(CCObject*) {
        if (m_undo.empty()) {
            toast("Nothing to undo", NotificationIcon::Warning);
            return;
        }
        m_redo.push_back(CanvasSnapshot { m_width, m_height, m_pixels });
        auto snapshot = m_undo.back();
        m_undo.pop_back();
        restoreSnapshot(snapshot);
    }

    void onRedo(CCObject*) {
        if (m_redo.empty()) {
            toast("Nothing to redo", NotificationIcon::Warning);
            return;
        }
        m_undo.push_back(CanvasSnapshot { m_width, m_height, m_pixels });
        auto snapshot = m_redo.back();
        m_redo.pop_back();
        restoreSnapshot(snapshot);
    }

    void onSave(CCObject*) {
        saveThenUse(false);
    }
};

void openIconEditor(PackSummary pack, TargetPreset target, EditorSaveCallback onSave) {
    if (auto* popup = IconEditorPopup::create(std::move(pack), std::move(target), std::move(onSave))) {
        popup->show();
    }
}

} // namespace textureforge
