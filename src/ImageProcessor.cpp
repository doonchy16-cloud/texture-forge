#include "TextureForge/ImageProcessor.hpp"

#include "vendor/stb_image.h"
#include "vendor/stb_image_write.h"

namespace textureforge {

namespace {

constexpr auto kMaxImageDimension = 8192;
constexpr auto kMaxImagePixels = static_cast<size_t>(kMaxImageDimension) * static_cast<size_t>(kMaxImageDimension);

Result<> validateImageSizeBeforeLoad(fs::path const& path) {
    int width = 0;
    int height = 0;
    int channels = 0;
    if (!stbi_info(pathString(path).c_str(), &width, &height, &channels) || width <= 0 || height <= 0) {
        return Err("Unable to read image size: {}", pathString(path.filename()));
    }

    auto pixels = static_cast<size_t>(width) * static_cast<size_t>(height);
    if (width > kMaxImageDimension || height > kMaxImageDimension || pixels > kMaxImagePixels) {
        return Err(
            "Image is too large: {}x{}. Max is {}x{}.",
            width,
            height,
            kMaxImageDimension,
            kMaxImageDimension
        );
    }
    return Ok();
}

} // namespace

Result<ImageData> loadImage(fs::path const& path) {
    GEODE_UNWRAP(validateImageSizeBeforeLoad(path));
    int width = 0;
    int height = 0;
    int channels = 0;
    std::unique_ptr<unsigned char, decltype(&stbi_image_free)> pixels(
        stbi_load(pathString(path).c_str(), &width, &height, &channels, 4),
        stbi_image_free
    );
    if (!pixels || width <= 0 || height <= 0) {
        return Err("Unable to read image: {}", pathString(path.filename()));
    }

    auto size = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
    ImageData image { width, height, std::vector<unsigned char>(size) };
    std::copy(pixels.get(), pixels.get() + size, image.rgba.begin());
    return Ok(std::move(image));
}

bool backgroundLikePixel(unsigned char const* px, std::vector<std::array<int, 3>> const& borderSamples) {
    if (px[3] < 24) return true;
    auto r = static_cast<int>(px[0]);
    auto g = static_cast<int>(px[1]);
    auto b = static_cast<int>(px[2]);
    auto mx = std::max({ r, g, b });
    auto mn = std::min({ r, g, b });
    auto saturation = mx - mn;
    auto average = (r + g + b) / 3;

    if ((average >= 174 && saturation <= 54) ||
        (average >= 154 && saturation <= 32) ||
        (mx >= 205 && saturation <= 80)) {
        return true;
    }

    for (auto const& sample : borderSamples) {
        auto dr = r - sample[0];
        auto dg = g - sample[1];
        auto db = b - sample[2];
        if (dr * dr + dg * dg + db * db <= 58 * 58) return true;
    }
    return false;
}

std::vector<std::array<int, 3>> collectBorderSamples(ImageData const& image) {
    std::vector<std::array<int, 3>> samples;
    auto stride = std::max(1, std::min(image.width, image.height) / 18);
    auto addSample = [&](int x, int y) {
        auto const* px = image.rgba.data() + (static_cast<size_t>(y) * image.width + x) * 4;
        if (px[3] >= 24) samples.push_back({ static_cast<int>(px[0]), static_cast<int>(px[1]), static_cast<int>(px[2]) });
    };

    for (auto x = 0; x < image.width; x += stride) {
        addSample(x, 0);
        addSample(x, image.height - 1);
    }
    for (auto y = 0; y < image.height; y += stride) {
        addSample(0, y);
        addSample(image.width - 1, y);
    }
    return samples;
}

std::vector<std::uint8_t> dilateMask(std::vector<std::uint8_t> const& mask, int width, int height) {
    std::vector<std::uint8_t> out(mask.size(), 0);
    for (auto y = 0; y < height; ++y) {
        for (auto x = 0; x < width; ++x) {
            auto value = 0;
            for (auto yy = std::max(0, y - 1); yy <= std::min(height - 1, y + 1); ++yy) {
                for (auto xx = std::max(0, x - 1); xx <= std::min(width - 1, x + 1); ++xx) {
                    value = std::max(value, static_cast<int>(mask[static_cast<size_t>(yy) * width + xx]));
                }
            }
            out[static_cast<size_t>(y) * width + x] = static_cast<std::uint8_t>(value);
        }
    }
    return out;
}

std::vector<std::uint8_t> softenMask(std::vector<std::uint8_t> const& mask, int width, int height) {
    std::vector<std::uint8_t> out(mask.size(), 0);
    for (auto y = 0; y < height; ++y) {
        for (auto x = 0; x < width; ++x) {
            auto total = 0;
            auto count = 0;
            for (auto yy = std::max(0, y - 1); yy <= std::min(height - 1, y + 1); ++yy) {
                for (auto xx = std::max(0, x - 1); xx <= std::min(width - 1, x + 1); ++xx) {
                    total += mask[static_cast<size_t>(yy) * width + xx];
                    ++count;
                }
            }
            out[static_cast<size_t>(y) * width + x] = static_cast<std::uint8_t>(total / std::max(1, count));
        }
    }
    return out;
}

void removeSmallMaskIslands(std::vector<std::uint8_t>& subject, int width, int height) {
    auto totalPixels = static_cast<size_t>(width) * static_cast<size_t>(height);
    std::vector<std::uint8_t> seen(totalPixels, 0);
    std::vector<int> stack;
    std::vector<int> component;
    auto smallArea = std::max(16, (width * height) / 1800);

    for (auto y = 0; y < height; ++y) {
        for (auto x = 0; x < width; ++x) {
            auto index = y * width + x;
            if (seen[index] || subject[index] == 0) continue;
            stack.clear();
            component.clear();
            stack.push_back(index);
            seen[index] = 1;
            auto minX = x;
            auto maxX = x;
            auto minY = y;
            auto maxY = y;

            while (!stack.empty()) {
                auto current = stack.back();
                stack.pop_back();
                component.push_back(current);
                auto cx = current % width;
                auto cy = current / width;
                minX = std::min(minX, cx);
                maxX = std::max(maxX, cx);
                minY = std::min(minY, cy);
                maxY = std::max(maxY, cy);
                auto add = [&](int nx, int ny) {
                    if (nx < 0 || nx >= width || ny < 0 || ny >= height) return;
                    auto next = ny * width + nx;
                    if (seen[next] || subject[next] == 0) return;
                    seen[next] = 1;
                    stack.push_back(next);
                };
                add(cx + 1, cy);
                add(cx - 1, cy);
                add(cx, cy + 1);
                add(cx, cy - 1);
            }

            auto outsideCenter = maxX < width * 0.18f || minX > width * 0.82f || maxY < height * 0.05f || minY > height * 0.90f;
            if (static_cast<int>(component.size()) < smallArea || (static_cast<int>(component.size()) < smallArea * 3 && outsideCenter)) {
                for (auto pixel : component) subject[pixel] = 0;
            }
        }
    }
}

void removeBackground(ImageData& image) {
    auto width = image.width;
    auto height = image.height;
    auto totalPixels = static_cast<size_t>(width) * static_cast<size_t>(height);
    if (width <= 0 || height <= 0 || totalPixels == 0) return;

    auto borderSamples = collectBorderSamples(image);
    std::vector<std::uint8_t> visited(totalPixels, 0);
    std::vector<std::uint8_t> background(totalPixels, 0);
    std::vector<int> queue;
    queue.reserve(totalPixels);
    auto cursor = size_t { 0 };

    auto add = [&](int x, int y) {
        if (x < 0 || x >= width || y < 0 || y >= height) return;
        auto index = y * width + x;
        if (visited[index]) return;
        visited[index] = 1;
        auto const* px = image.rgba.data() + static_cast<size_t>(index) * 4;
        if (backgroundLikePixel(px, borderSamples)) {
            background[index] = 1;
            queue.push_back(index);
        }
    };

    for (auto x = 0; x < width; ++x) {
        add(x, 0);
        add(x, height - 1);
    }
    for (auto y = 0; y < height; ++y) {
        add(0, y);
        add(width - 1, y);
    }

    while (cursor < queue.size()) {
        auto current = queue[cursor++];
        auto x = current % width;
        auto y = current / width;
        add(x + 1, y);
        add(x - 1, y);
        add(x, y + 1);
        add(x, y - 1);
    }

    std::vector<std::uint8_t> subject(totalPixels, 0);
    for (auto index = size_t { 0 }; index < totalPixels; ++index) {
        subject[index] = background[index] ? 0 : 255;
    }
    removeSmallMaskIslands(subject, width, height);
    auto mask = softenMask(dilateMask(subject, width, height), width, height);
    for (auto index = size_t { 0 }; index < totalPixels; ++index) {
        auto* px = image.rgba.data() + index * 4;
        px[3] = static_cast<unsigned char>((static_cast<int>(px[3]) * static_cast<int>(mask[index])) / 255);
    }
}

Result<ImageData> loadPreparedImage(fs::path const& path, bool removeImageBackground) {
    GEODE_UNWRAP_INTO(auto image, loadImage(path));
    if (removeImageBackground) removeBackground(image);
    return Ok(std::move(image));
}

std::optional<IntRect> parseTextureRect(std::string const& value) {
    IntRect rect;
    if (std::sscanf(value.c_str(), "{{%d,%d},{%d,%d}}", &rect.x, &rect.y, &rect.width, &rect.height) != 4) {
        return std::nullopt;
    }
    if (rect.width <= 0 || rect.height <= 0) return std::nullopt;
    return rect;
}

std::string stripScaleSuffix(std::string stem) {
    for (auto suffix : { "-uhd", "-hd" }) {
        if (endsWith(stem, suffix)) {
            stem.resize(stem.size() - std::strlen(suffix));
            break;
        }
    }
    return stem;
}

bool isIconResourceOutput(fs::path const& output) {
    return lowerString(genericPathString(output.parent_path())) == "icons" && lowerString(pathString(output.extension())) == ".png";
}

std::optional<std::string> iconFrameTail(std::string const& frameKey, std::string const& baseStem) {
    auto key = lowerString(frameKey);
    auto stem = lowerString(baseStem);
    auto prefix = stem + "_";

    if (!startsWith(key, prefix) || !endsWith(key, ".png")) return std::nullopt;

    auto tail = key.substr(prefix.size(), key.size() - prefix.size() - std::strlen(".png"));
    if (tail.empty()) return std::nullopt;
    return tail;
}

bool isPrimaryIconImageFrame(std::string const& frameKey, std::string const& baseStem) {
    auto tail = iconFrameTail(frameKey, baseStem);
    if (!tail) return false;
    if (tail->find("glow") != std::string::npos || tail->find("shadow") != std::string::npos) return false;
    if (startsWith(*tail, "2_") || tail->find("_2_") != std::string::npos) return false;
    return true;
}

struct ParsedIconFrames {
    std::vector<IntRect> primaryFrames;
    std::vector<IntRect> clearFrames;
};

ParsedIconFrames parseIconFrames(std::string const& plist, std::string const& baseStem) {
    ParsedIconFrames frames;
    auto pos = plist.find("<key>frames</key>");
    if (pos == std::string::npos) pos = 0;

    while (true) {
        auto keyStart = plist.find("<key>", pos);
        if (keyStart == std::string::npos) break;
        auto keyTextStart = keyStart + std::strlen("<key>");
        auto keyEnd = plist.find("</key>", keyTextStart);
        if (keyEnd == std::string::npos) break;
        auto frameKey = plist.substr(keyTextStart, keyEnd - keyTextStart);
        pos = keyEnd + std::strlen("</key>");
        if (frameKey == "metadata") break;
        auto tail = iconFrameTail(frameKey, baseStem);
        if (!tail) continue;

        auto dictEnd = plist.find("</dict>", pos);
        auto textureKey = plist.find("<key>textureRect</key>", pos);
        if (textureKey == std::string::npos || (dictEnd != std::string::npos && textureKey > dictEnd)) continue;
        auto stringStart = plist.find("<string>", textureKey);
        if (stringStart == std::string::npos || (dictEnd != std::string::npos && stringStart > dictEnd)) continue;
        stringStart += std::strlen("<string>");
        auto stringEnd = plist.find("</string>", stringStart);
        if (stringEnd == std::string::npos || (dictEnd != std::string::npos && stringEnd > dictEnd)) continue;
        if (auto rect = parseTextureRect(plist.substr(stringStart, stringEnd - stringStart))) {
            frames.clearFrames.push_back(*rect);
            if (isPrimaryIconImageFrame(frameKey, baseStem)) frames.primaryFrames.push_back(*rect);
        }
    }
    return frames;
}

Result<IconSheetLayout> readIconSheetLayout(fs::path const& output) {
    auto originalPng = resourcePath(output);
    auto relativePlist = output;
    relativePlist.replace_extension(".plist");
    auto originalPlist = resourcePath(relativePlist);

    std::error_code ec;
    if (!fs::exists(originalPng, ec) || !fs::exists(originalPlist, ec)) {
        return Err("Missing vanilla icon resource for {}", pathString(output.filename()));
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    if (!stbi_info(pathString(originalPng).c_str(), &width, &height, &channels) || width <= 0 || height <= 0) {
        return Err("Unable to read vanilla icon size for {}", pathString(output.filename()));
    }

    GEODE_UNWRAP_INTO(auto plist, geode::utils::file::readString(originalPlist));
    auto frames = parseIconFrames(plist, stripScaleSuffix(pathString(output.stem())));
    if (frames.primaryFrames.empty()) return Err("Unable to find icon frame layout for {}", pathString(output.filename()));
    return Ok(IconSheetLayout { width, height, originalPng, originalPlist, frames.primaryFrames, frames.clearFrames });
}

std::vector<IntRect> const& replacementFrames(IconSheetLayout const& layout) {
    return layout.clearFrames.empty() ? layout.primaryFrames : layout.clearFrames;
}

void clearFrame(
    std::vector<unsigned char>& destination,
    int destinationWidth,
    int destinationHeight,
    IntRect const& frame
) {
    auto startX = std::max(0, frame.x);
    auto startY = std::max(0, frame.y);
    auto endX = std::min(destinationWidth, frame.x + frame.width);
    auto endY = std::min(destinationHeight, frame.y + frame.height);
    if (startX >= endX || startY >= endY) return;

    for (auto y = startY; y < endY; ++y) {
        for (auto x = startX; x < endX; ++x) {
            auto* dst = destination.data() + ((y * destinationWidth + x) * 4);
            dst[0] = 0;
            dst[1] = 0;
            dst[2] = 0;
            dst[3] = 0;
        }
    }
}

void copyFrame(
    std::vector<unsigned char> const& source,
    int sourceWidth,
    int sourceHeight,
    std::vector<unsigned char>& destination,
    int destinationWidth,
    int destinationHeight,
    IntRect const& frame
) {
    auto startX = std::max(0, frame.x);
    auto startY = std::max(0, frame.y);
    auto endX = std::min({ sourceWidth, destinationWidth, frame.x + frame.width });
    auto endY = std::min({ sourceHeight, destinationHeight, frame.y + frame.height });
    if (startX >= endX || startY >= endY) return;

    for (auto y = startY; y < endY; ++y) {
        for (auto x = startX; x < endX; ++x) {
            auto const* src = source.data() + ((y * sourceWidth + x) * 4);
            auto* dst = destination.data() + ((y * destinationWidth + x) * 4);
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = src[3];
        }
    }
}

void blitFittedImage(
    unsigned char const* source,
    int sourceWidth,
    int sourceHeight,
    std::vector<unsigned char>& destination,
    int destinationWidth,
    int destinationHeight,
    IntRect const& frame,
    bool cover
) {
    if (!source || sourceWidth <= 0 || sourceHeight <= 0 || frame.width <= 0 || frame.height <= 0) return;
    auto scale = cover
        ? std::max(static_cast<float>(frame.width) / sourceWidth, static_cast<float>(frame.height) / sourceHeight)
        : std::min(static_cast<float>(frame.width) / sourceWidth, static_cast<float>(frame.height) / sourceHeight);
    auto fitWidth = std::max(1, static_cast<int>(std::round(sourceWidth * scale)));
    auto fitHeight = std::max(1, static_cast<int>(std::round(sourceHeight * scale)));
    auto offsetX = frame.x + (frame.width - fitWidth) / 2;
    auto offsetY = frame.y + (frame.height - fitHeight) / 2;
    auto drawStartX = cover ? frame.x : std::max(frame.x, offsetX);
    auto drawEndX = cover ? frame.x + frame.width : std::min(frame.x + frame.width, offsetX + fitWidth);
    auto drawStartY = cover ? frame.y : std::max(frame.y, offsetY);
    auto drawEndY = cover ? frame.y + frame.height : std::min(frame.y + frame.height, offsetY + fitHeight);

    for (auto destinationY = drawStartY; destinationY < drawEndY; ++destinationY) {
        if (destinationY < 0 || destinationY >= destinationHeight) continue;
        auto sourceY = std::clamp(static_cast<int>((destinationY - offsetY + 0.5f) * sourceHeight / fitHeight), 0, sourceHeight - 1);
        for (auto destinationX = drawStartX; destinationX < drawEndX; ++destinationX) {
            if (destinationX < 0 || destinationX >= destinationWidth) continue;
            auto sourceX = std::clamp(static_cast<int>((destinationX - offsetX + 0.5f) * sourceWidth / fitWidth), 0, sourceWidth - 1);
            auto const* src = source + ((sourceY * sourceWidth + sourceX) * 4);
            auto* dst = destination.data() + ((destinationY * destinationWidth + destinationX) * 4);
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = src[3];
        }
    }
}

Result<bool> writeIconTextureIfPossible(fs::path const& source, fs::path const& destination, fs::path const& output, bool removeImageBackground) {
    if (!isIconResourceOutput(output)) return Ok(false);

    auto relativePlist = output;
    relativePlist.replace_extension(".plist");
    std::error_code ec;
    if (!fs::exists(resourcePath(relativePlist), ec)) return Ok(false);

    GEODE_UNWRAP_INTO(auto layout, readIconSheetLayout(output));
    GEODE_UNWRAP_INTO(auto sourceImage, loadPreparedImage(source, removeImageBackground));

    GEODE_UNWRAP_INTO(auto originalSheet, loadImage(layout.pngPath));
    if (originalSheet.width != layout.width || originalSheet.height != layout.height) {
        return Err("Vanilla icon sheet size changed for {}", pathString(output.filename()));
    }
    auto sheet = std::move(originalSheet.rgba);
    for (auto const& frame : layout.clearFrames) {
        clearFrame(sheet, layout.width, layout.height, frame);
    }
    auto const& imageFrames = replacementFrames(layout);
    for (auto const& frame : imageFrames) {
        blitFittedImage(sourceImage.rgba.data(), sourceImage.width, sourceImage.height, sheet, layout.width, layout.height, frame, false);
    }

    GEODE_UNWRAP(ensureDir(destination.parent_path()));
    if (!stbi_write_png(pathString(destination).c_str(), layout.width, layout.height, 4, sheet.data(), layout.width * 4)) {
        return Err("Unable to write fitted icon PNG");
    }

    auto destinationPlist = destination;
    destinationPlist.replace_extension(".plist");
    fs::copy_file(layout.plistPath, destinationPlist, fs::copy_options::overwrite_existing, ec);
    if (ec) return Err("Unable to copy icon plist: {}", ec.message());
    log::info(
        "Generated Texture Forge icon sheet {} and plist {} ({} replacement frame(s), {} primary frame(s), {} cleared layer frame(s))",
        normalizedPathString(destination),
        normalizedPathString(destinationPlist),
        imageFrames.size(),
        layout.primaryFrames.size(),
        layout.clearFrames.size()
    );
    return Ok(true);
}

Result<int> normalizeIconOutputsInRoot(fs::path const& root) {
    std::error_code ec;
    if (!fs::exists(root, ec)) return Ok(0);

    auto normalized = 0;
    for (
        auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec);
        it != fs::recursive_directory_iterator();
        it.increment(ec)
    ) {
        if (ec) break;
        if (!it->is_regular_file(ec)) continue;
        if (lowerString(pathString(it->path().extension())) != ".png") continue;

        auto relative = fs::relative(it->path(), root, ec);
        if (ec) continue;
        relative = fs::path(relative.generic_string());
        if (!isIconResourceOutput(relative)) continue;

        auto relativePlist = relative;
        relativePlist.replace_extension(".plist");
        if (!fs::exists(root / relativePlist, ec) || !fs::exists(resourcePath(relativePlist), ec)) continue;

        GEODE_UNWRAP_INTO(auto layout, readIconSheetLayout(relative));
        GEODE_UNWRAP_INTO(auto currentSheet, loadImage(it->path()));
        if (currentSheet.width != layout.width || currentSheet.height != layout.height) {
            return Err("Pack icon sheet size changed for {}", pathString(relative.filename()));
        }

        auto originalPixels = currentSheet.rgba;
        auto normalizedPixels = std::move(currentSheet.rgba);
        for (auto const& frame : layout.clearFrames) {
            clearFrame(normalizedPixels, layout.width, layout.height, frame);
        }
        auto const& imageFrames = replacementFrames(layout);
        for (auto const& frame : imageFrames) {
            copyFrame(originalPixels, layout.width, layout.height, normalizedPixels, layout.width, layout.height, frame);
        }

        if (!stbi_write_png(pathString(it->path()).c_str(), layout.width, layout.height, 4, normalizedPixels.data(), layout.width * 4)) {
            return Err("Unable to normalize icon PNG: {}", pathString(relative.filename()));
        }
        fs::copy_file(layout.plistPath, root / relativePlist, fs::copy_options::overwrite_existing, ec);
        if (ec) return Err("Unable to refresh icon plist: {}", ec.message());
        ++normalized;
    }

    if (normalized > 0) {
        log::info("Normalized {} Texture Forge icon sheet(s) in {}", normalized, normalizedPathString(root));
    }
    return Ok(normalized);
}

bool isBackgroundTarget(fs::path const& output) {
    auto name = lowerString(genericPathString(output.filename()));
    return name.find("bg") != std::string::npos || name.find("background") != std::string::npos || name.find("gradient") != std::string::npos;
}

Result<> writeRasterTexture(fs::path const& source, fs::path const& destination, fs::path const& output, bool removeImageBackground) {
    GEODE_UNWRAP(ensureDir(destination.parent_path()));
    GEODE_UNWRAP_INTO(auto image, loadPreparedImage(source, removeImageBackground));

    auto original = resourcePath(output);
    int width = 0;
    int height = 0;
    int channels = 0;
    if (stbi_info(pathString(original).c_str(), &width, &height, &channels) && width > 0 && height > 0) {
        std::vector<unsigned char> canvas(static_cast<size_t>(width) * static_cast<size_t>(height) * 4, 0);
        blitFittedImage(image.rgba.data(), image.width, image.height, canvas, width, height, { 0, 0, width, height }, isBackgroundTarget(output));
        if (!stbi_write_png(pathString(destination).c_str(), width, height, 4, canvas.data(), width * 4)) {
            return Err("Unable to write PNG: {}", pathString(destination.filename()));
        }
        return Ok();
    }

    if (!stbi_write_png(pathString(destination).c_str(), image.width, image.height, 4, image.rgba.data(), image.width * 4)) {
        return Err("Unable to write PNG: {}", pathString(destination.filename()));
    }
    return Ok();
}

Result<> writeTextureOutput(fs::path const& source, fs::path const& destination, fs::path const& output, bool removeImageBackground) {
    GEODE_UNWRAP(ensureDir(destination.parent_path()));
    GEODE_UNWRAP_INTO(auto wroteIconTexture, writeIconTextureIfPossible(source, destination, output, removeImageBackground));
    if (wroteIconTexture) return Ok();

    if (isImageImport(source) && lowerString(pathString(output.extension())) == ".png") {
        return writeRasterTexture(source, destination, output, removeImageBackground);
    }
    if (removeImageBackground) return Err("Background removal only works on image files");

    std::error_code ec;
    fs::copy_file(source, destination, fs::copy_options::overwrite_existing, ec);
    if (ec) return Err("Copy failed: {}", ec.message());
    return Ok();
}

} // namespace textureforge
