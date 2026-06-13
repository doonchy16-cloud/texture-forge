#include "TextureForge/TargetCatalog.hpp"

namespace textureforge {

TargetPreset iconPreset(std::string const& label, std::string const& prefix, int number, int displayNumber) {
    auto id = iconID(number);
    return TargetPreset {
        fmt::format("{} {}", label, displayNumber),
        {
            fs::path(fmt::format("icons/{}_{}.png", prefix, id)),
            fs::path(fmt::format("icons/{}_{}-hd.png", prefix, id)),
            fs::path(fmt::format("icons/{}_{}-uhd.png", prefix, id)),
        },
    };
}

std::string resourceCategory(fs::path const& relative) {
    auto path = lowerString(genericPathString(relative));
    auto name = lowerString(genericPathString(relative.filename()));
    if (startsWith(path, "icons/")) return "Raw Icons";
    if (name.find("font") != std::string::npos) return "Fonts";
    if (name.find("bg") != std::string::npos || name.find("background") != std::string::npos || name.find("gradient") != std::string::npos) return "Backgrounds";
    if (name.find("gj_") == 0 || name.find("button") != std::string::npos || name.find("menu") != std::string::npos || name.find("dialog") != std::string::npos) return "Menu UI";
    if (name.find("edit") != std::string::npos || name.find("editor") != std::string::npos) return "Editor";
    if (name.find("game") != std::string::npos || name.find("sheet") != std::string::npos || name.find("object") != std::string::npos) return "Level Objects";
    return "Other";
}

std::vector<int> scanIconNumbers(std::string const& prefix) {
    std::set<int> numbers;
    auto iconsDir = resourcePath("icons");
    auto prefixText = prefix + "_";
    std::error_code ec;
    if (!fs::exists(iconsDir, ec)) return {};

    for (auto const& entry : fs::directory_iterator(iconsDir, fs::directory_options::skip_permission_denied, ec)) {
        if (!entry.is_regular_file(ec)) continue;
        if (lowerString(pathString(entry.path().extension())) != ".png") continue;

        auto stem = stripScaleSuffix(lowerString(pathString(entry.path().stem())));
        if (!startsWith(stem, prefixText)) continue;
        auto numberText = stem.substr(prefixText.size());
        if (numberText.empty() || !std::all_of(numberText.begin(), numberText.end(), [](unsigned char c) {
            return std::isdigit(c);
        })) {
            continue;
        }

        try {
            numbers.insert(std::stoi(numberText));
        }
        catch (...) {
            continue;
        }
    }
    return { numbers.begin(), numbers.end() };
}

TargetCatalog const& targetCatalog() {
    static TargetCatalog catalog = [] {
        TargetCatalog out;

        auto addGroup = [&](std::string const& label, std::vector<TargetPreset> presets, int first = 0, int last = 0) {
            auto start = out.presets.size();
            for (auto& preset : presets) out.presets.push_back(std::move(preset));
            out.groups.push_back(TargetGroupRange {
                label,
                start,
                out.presets.size() - start,
                first,
                last,
                first > 0 && last >= first,
            });
        };

        auto addIconGroup = [&](std::string const& label, std::string const& prefix) {
            std::vector<TargetPreset> presets;
            auto numbers = scanIconNumbers(prefix);
            for (auto number : numbers) {
                if (number == 0) continue;
                presets.push_back(iconPreset(label, prefix, number, number));
            }
            if (!presets.empty()) {
                addGroup(label, std::move(presets), std::max(1, numbers.front()), numbers.back());
            }
        };

        addIconGroup("Cube", "player");
        addIconGroup("Ship", "ship");
        addIconGroup("Ball", "player_ball");
        addIconGroup("UFO", "bird");
        addIconGroup("Wave", "dart");
        addIconGroup("Robot", "robot");
        addIconGroup("Spider", "spider");
        addIconGroup("Swing", "swing");
        addIconGroup("Jetpack", "jetpack");

        std::vector<fs::path> rawFiles;
        std::error_code ec;
        auto resources = geode::dirs::getResourcesDir();
        for (auto const& entry : fs::recursive_directory_iterator(resources, fs::directory_options::skip_permission_denied, ec)) {
            if (!entry.is_regular_file(ec)) continue;
            if (lowerString(pathString(entry.path().extension())) != ".png") continue;
            auto rel = fs::relative(entry.path(), resources, ec);
            if (!ec) rawFiles.push_back(fs::path(rel.generic_string()));
        }
        std::sort(rawFiles.begin(), rawFiles.end());

        for (auto const& groupName : { "Backgrounds", "Menu UI", "Level Objects", "Editor", "Fonts", "Other", "Raw Icons" }) {
            std::vector<TargetPreset> presets;
            for (auto const& rel : rawFiles) {
                if (resourceCategory(rel) != groupName) continue;
                presets.push_back(TargetPreset { genericPathString(rel), { rel } });
            }
            if (!presets.empty()) addGroup(groupName, std::move(presets));
        }

        std::vector<TargetPreset> rawPresets;
        for (auto const& rel : rawFiles) rawPresets.push_back(TargetPreset { genericPathString(rel), { rel } });
        addGroup("Raw Browser", std::move(rawPresets));
        return out;
    }();
    return catalog;
}

std::vector<TargetPreset> const& targetPresets() {
    return targetCatalog().presets;
}

std::vector<TargetGroupRange> const& targetGroups() {
    return targetCatalog().groups;
}

} // namespace textureforge
