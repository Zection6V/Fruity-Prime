#include "Images.hpp"
#include "NativeRuntime/System/AtomicSharedPtr.hpp"

#include "../NativeRuntime/OpenTK/GL.hpp"
#include "../NativeRuntime/Stb/Image.hpp"

#include "../Formats/Model.hpp"
#include "../HUD/HudInfo.hpp"
#include "../Read.hpp"
#include "../NativeRuntime/System/IO.hpp"
#include "../NativeRuntime/System/Managed.hpp"

#include <algorithm>
#include <atomic>
#include <bit>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <deque>
#include <exception>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <mutex>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

using ::MphRead::NativeRuntime::PathFromUtf8;
using ::MphRead::NativeRuntime::RequireReference;
using ::MphRead::NativeRuntime::UncheckedAdd;
using ::MphRead::NativeRuntime::UncheckedMultiply;

namespace MphRead::Export::ImagesInterop
{
    // GL.ReadPixels and ReFuel.Stb's PNG writer, as Images.cs calls them.
    void ReadPixelsRgbUnsignedByte(
        std::int32_t x,
        std::int32_t y,
        std::int32_t width,
        std::int32_t height,
        std::span<std::uint8_t> buffer)
    {
        ::OpenTK::Graphics::OpenGL::GL::ReadPixels(
            x, y, width, height,
            ::OpenTK::Graphics::OpenGL::GL::PixelFormat::Rgb,
            ::OpenTK::Graphics::OpenGL::GL::PixelType::UnsignedByte,
            buffer.data());
    }

    void SetFlipVerticallyOnSave(bool value)
    {
        ::stbi_flip_vertically_on_write(value ? 1 : 0);
    }

    namespace
    {
        void WriteToStream(void* context, void* data, int size)
        {
            auto* const stream = static_cast<std::ostream*>(context);
            stream->write(static_cast<const char*>(data), size);
        }
    }

    void WritePngRgb(
        std::span<const std::uint8_t> buffer,
        std::int32_t width,
        std::int32_t height,
        std::ostream& stream)
    {
        (void)::stbi_write_png_to_func(
            &WriteToStream, &stream, width, height, 3, buffer.data(), width * 3);
    }

    void WritePngRgba(
        std::span<const ColorRgba> buffer,
        std::uint16_t width,
        std::uint16_t height,
        std::ostream& stream)
    {
        // ColorRgba is four bytes in R, G, B, A order, which is the layout the
        // writer takes.
        (void)::stbi_write_png_to_func(
            &WriteToStream, &stream, width, height, 4, buffer.data(), width * 4);
    }
}

namespace
{
    using MphRead::ColorRgba;

    [[nodiscard]] std::size_t ArrayLength(std::int32_t length)
    {
        if (length < 0)
        {
            throw std::overflow_error("Arithmetic operation resulted in an overflow.");
        }
        return static_cast<std::size_t>(length);
    }

    [[nodiscard]] std::int32_t RgbBufferLengthValue(
        std::int32_t width,
        std::int32_t height) noexcept
    {
        return UncheckedMultiply(UncheckedMultiply(width, height), 3);
    }

    [[nodiscard]] std::size_t NewArrayLength(
        std::int32_t width,
        std::int32_t height)
    {
        return ArrayLength(RgbBufferLengthValue(width, height));
    }

    [[nodiscard]] std::size_t PoolRentLength(
        std::int32_t width,
        std::int32_t height)
    {
        const std::int32_t length = RgbBufferLengthValue(width, height);
        if (length < 0)
        {
            throw std::out_of_range(
                "Specified argument was out of the range of valid values. "
                "(Parameter 'minimumLength')");
        }
        return static_cast<std::size_t>(length);
    }

    [[nodiscard]] std::ofstream CreateFile(const std::string& path)
    {
        std::ofstream stream(PathFromUtf8(path), std::ios::binary | std::ios::trunc);
        stream.exceptions(std::ios::failbit | std::ios::badbit);
        return stream;
    }

    [[nodiscard]] std::string PadLeft3(std::int32_t value)
    {
        std::string text = std::to_string(value);
        if (text.size() < 3)
        {
            text.insert(text.begin(), 3 - text.size(), '0');
        }
        return text;
    }

    class BytePool final
    {
    public:
        [[nodiscard]] std::vector<std::uint8_t> Rent(std::size_t minimumLength)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            const auto found = std::find_if(
                _buffers.begin(),
                _buffers.end(),
                [minimumLength](const std::vector<std::uint8_t>& buffer)
                {
                    return buffer.size() >= minimumLength;
                });
            if (found == _buffers.end())
            {
                return std::vector<std::uint8_t>(minimumLength);
            }
            std::vector<std::uint8_t> result = std::move(*found);
            _buffers.erase(found);
            return result;
        }

        void Return(std::vector<std::uint8_t> buffer)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _buffers.push_back(std::move(buffer));
        }

    private:
        std::mutex _mutex;
        std::vector<std::vector<std::uint8_t>> _buffers;
    };

    [[nodiscard]] BytePool& SharedBytePool()
    {
        static BytePool pool;
        return pool;
    }

    struct IntPair final
    {
        std::int32_t First;
        std::int32_t Second;

        [[nodiscard]] friend bool operator==(const IntPair&, const IntPair&) noexcept = default;
    };

    struct IntPairHash final
    {
        [[nodiscard]] std::size_t operator()(const IntPair& value) const noexcept
        {
            const std::size_t first = std::hash<std::int32_t>{}(value.First);
            const std::size_t second = std::hash<std::int32_t>{}(value.Second);
            return first ^ (second + static_cast<std::size_t>(0x9E3779B9U)
                + (first << 6U) + (first >> 2U));
        }
    };
}

namespace MphRead::Export
{
    class Images::QueueState final
    {
    public:
        struct Item final
        {
            std::vector<std::uint8_t> Buffer;
            std::string Name;
            std::int32_t Width = 0;
            std::int32_t Height = 0;
        };

        void Enqueue(Item item)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _items.push_back(std::move(item));
        }

        [[nodiscard]] bool TryDequeue(Item& item)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (_items.empty())
            {
                return false;
            }
            item = std::move(_items.front());
            _items.pop_front();
            return true;
        }

        [[nodiscard]] std::size_t Count() const
        {
            std::lock_guard<std::mutex> lock(_mutex);
            return _items.size();
        }

    private:
        mutable std::mutex _mutex;
        std::deque<Item> _items;
    };

    class Images::TaskState final
    {
    public:
        TaskState()
        {
            std::thread([this]
            {
                try
                {
                    Images::ProcessQueue();
                }
                catch (...)
                {
                    _exception = std::current_exception();
                }
            }).detach();
        }

        TaskState(const TaskState&) = delete;
        TaskState& operator=(const TaskState&) = delete;

    private:
        std::exception_ptr _exception{};
    };

    ::MphRead::NativeRuntime::AtomicSharedPtr<Images::TaskState> Images::_task{};
    std::atomic<bool> Images::_recording{false};
    Images::QueueState Images::_queue{};

    void Images::Screenshot(
        std::int32_t width,
        std::int32_t height,
        std::optional<std::string> name)
    {
        std::vector<std::uint8_t> buffer(NewArrayLength(width, height));
        ImagesInterop::ReadPixelsRgbUnsignedByte(0, 0, width, height, buffer);
        const std::string path = Paths::Combine(Paths::Export(), "_screenshots");
        std::filesystem::create_directories(PathFromUtf8(path));
        if (!name.has_value())
        {
            const auto now = std::chrono::system_clock::now().time_since_epoch();
            const auto milliseconds
                = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
            name = std::to_string(milliseconds);
        }
        std::ofstream fileStream = CreateFile(
            Paths::Combine(path, *name + ".png"));
        ImagesInterop::SetFlipVerticallyOnSave(true);
        ImagesInterop::WritePngRgb(buffer, width, height, fileStream);
    }

    void Images::Record(
        std::int32_t width,
        std::int32_t height,
        const std::string& name)
    {
        _recording.store(true, std::memory_order_relaxed);
        if (!_task.load(std::memory_order_relaxed))
        {
            std::shared_ptr<TaskState> task = std::make_shared<TaskState>();
            _task.store(std::move(task), std::memory_order_relaxed);
        }
        std::vector<std::uint8_t> buffer
            = SharedBytePool().Rent(PoolRentLength(width, height));
        ImagesInterop::ReadPixelsRgbUnsignedByte(0, 0, width, height, buffer);
        _queue.Enqueue(QueueState::Item{
            std::move(buffer),
            name,
            width,
            height});
    }

    void Images::StopRecording()
    {
        _recording.store(false, std::memory_order_relaxed);
    }

    void Images::ProcessQueue()
    {
        while (_recording.load(std::memory_order_relaxed)
            || _queue.Count() > 0)
        {
            QueueState::Item result;
            while (_queue.TryDequeue(result))
            {
                const std::string path = Paths::Combine(Paths::Export(), "_screenshots");
                std::filesystem::create_directories(PathFromUtf8(path));
                std::ofstream fileStream = CreateFile(
                    Paths::Combine(path, result.Name + ".png"));
                ImagesInterop::SetFlipVerticallyOnSave(true);
                ImagesInterop::WritePngRgb(
                    result.Buffer,
                    result.Width,
                    result.Height,
                    fileStream);
                SharedBytePool().Return(std::move(result.Buffer));
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    void Images::ExportImages(const Model& model)
    {
        const std::string exportPath = Paths::Combine(Paths::Export(), model.Name);
        const auto& recolors = RequireReference(model.Recolors);
        for (const std::shared_ptr<Recolor>& recolorPointer : recolors)
        {
            const Recolor& recolor = RequireReference(recolorPointer);
            const std::string colorPath = Paths::Combine(exportPath, recolor.Name);
            std::filesystem::create_directories(PathFromUtf8(colorPath));
            std::unordered_set<std::int32_t> usedTextures;
            std::int32_t id = 0;
            std::unordered_set<IntPair, IntPairHash> usedCombos;

            const auto doTexture = [&](std::int32_t textureId, std::int32_t paletteId)
            {
                const IntPair combo{textureId, paletteId};
                if (textureId == -1 || usedCombos.contains(combo))
                {
                    return;
                }
                const auto& textures = RequireReference(recolor.Textures);
                const Texture texture = textures.at(static_cast<std::size_t>(textureId));
                const std::vector<ColorRgba> pixels = recolor.GetPixels(textureId, paletteId);
                if (texture.Width == 0 || texture.Height == 0 || pixels.empty())
                {
                    return;
                }
                assert(UncheckedMultiply(
                    static_cast<std::int32_t>(texture.Width),
                    static_cast<std::int32_t>(texture.Height))
                    == static_cast<std::int32_t>(pixels.size()));
                usedTextures.insert(textureId);
                usedCombos.insert(combo);
                std::string filename
                    = std::to_string(textureId) + "-" + std::to_string(paletteId);
                if (id > 0)
                {
                    filename = "anim__" + PadLeft3(id);
                }
                SaveTexture(
                    colorPath,
                    filename,
                    texture.Width,
                    texture.Height,
                    pixels);
            };

            if (!model.Materials)
            {
                throw System::ArgumentNullException("source");
            }
            const auto& materials = *model.Materials;
            std::vector<std::shared_ptr<Material>> orderedMaterials(
                materials.begin(), materials.end());
            std::stable_sort(
                orderedMaterials.begin(),
                orderedMaterials.end(),
                [](const std::shared_ptr<Material>& leftPointer,
                    const std::shared_ptr<Material>& rightPointer)
                {
                    const Material& left = RequireReference(leftPointer);
                    const Material& right = RequireReference(rightPointer);
                    if (left.TextureId != right.TextureId)
                    {
                        return left.TextureId < right.TextureId;
                    }
                    return left.PaletteId < right.PaletteId;
                });
            for (const std::shared_ptr<Material>& materialPointer : orderedMaterials)
            {
                const Material& material = RequireReference(materialPointer);
                doTexture(material.TextureId, material.PaletteId);
            }

            id = 1;
            usedCombos.clear();
            const AnimationGroups& animationGroups = RequireReference(model.AnimationGroups);
            const auto& textureGroups = RequireReference(animationGroups.Texture);
            for (const std::shared_ptr<TextureAnimationGroup>& groupPointer : textureGroups)
            {
                const TextureAnimationGroup& group = RequireReference(groupPointer);
                const auto& animations = RequireReference(group.Animations);
                for (const auto& [name, animation] : animations)
                {
                    (void)name;
                    const std::int32_t end = static_cast<std::int32_t>(animation.StartIndex)
                        + static_cast<std::int32_t>(animation.Count);
                    for (std::int32_t i = animation.StartIndex; i < end; ++i)
                    {
                        const std::uint16_t textureId
                            = RequireReference(group.TextureIds).at(static_cast<std::size_t>(i));
                        const std::uint16_t paletteId
                            = RequireReference(group.PaletteIds).at(static_cast<std::size_t>(i));
                        doTexture(textureId, paletteId);
                        id = UncheckedAdd(id, 1);
                    }
                }
            }

            const auto& textures = RequireReference(recolor.Textures);
            if (usedTextures.size() != textures.size())
            {
                const std::string unusedPath = Paths::Combine(colorPath, "unused");
                std::filesystem::create_directories(PathFromUtf8(unusedPath));
                const auto& palettes = RequireReference(recolor.Palettes);
                for (std::int32_t t = 0;
                    t < static_cast<std::int32_t>(textures.size());
                    ++t)
                {
                    if (usedTextures.contains(t))
                    {
                        continue;
                    }
                    const Texture texture = textures.at(static_cast<std::size_t>(t));
                    for (std::int32_t p = 0;
                        p < static_cast<std::int32_t>(palettes.size());
                        ++p)
                    {
                        const auto& allTextureData = RequireReference(recolor.TextureData);
                        const auto textureDataPointer
                            = allTextureData.at(static_cast<std::size_t>(t));
                        const auto& allPaletteData = RequireReference(recolor.PaletteData);
                        const auto palettePointer
                            = allPaletteData.at(static_cast<std::size_t>(p));
                        if (!textureDataPointer)
                        {
                            throw System::ArgumentNullException("source");
                        }
                        if (std::any_of(
                            textureDataPointer->begin(),
                            textureDataPointer->end(),
                            [&palettePointer](const MphRead::TextureData& value)
                            {
                                if (!palettePointer)
                                {
                                    throw System::NullReferenceException();
                                }
                                return static_cast<std::uint64_t>(value.Data)
                                    >= static_cast<std::uint64_t>(palettePointer->size());
                            }))
                        {
                            continue;
                        }
                        const std::vector<ColorRgba> pixels = recolor.GetPixels(t, p);
                        const std::string filename
                            = std::to_string(t) + "-" + std::to_string(p);
                        SaveTexture(
                            unusedPath,
                            filename,
                            texture.Width,
                            texture.Height,
                            pixels);
                    }
                }
            }
        }
    }

    void Images::ExportPalettes(const Model& model)
    {
        const std::string exportPath = Paths::Combine(Paths::Export(), model.Name);
        const auto& recolors = RequireReference(model.Recolors);
        for (const std::shared_ptr<Recolor>& recolorPointer : recolors)
        {
            const Recolor& recolor = RequireReference(recolorPointer);
            const std::string palettePath
                = Paths::Combine(exportPath, recolor.Name, "palettes");
            std::filesystem::create_directories(PathFromUtf8(palettePath));
            const auto& palettes = RequireReference(recolor.Palettes);
            for (std::int32_t p = 0;
                p < static_cast<std::int32_t>(palettes.size());
                ++p)
            {
                const std::vector<ColorRgba> pixels = recolor.GetPalettePixels(p);
                const std::string filename = "p" + std::to_string(p);
                SaveTexture(palettePath, filename, 16, 16, pixels);
            }
        }
    }

    void Images::SaveTexture(
        const std::string& directory,
        const std::string& filename,
        std::uint16_t width,
        std::uint16_t height,
        const std::vector<ColorRgba>& pixels)
    {
        const std::string imagePath
            = Paths::Combine(directory, filename + ".png");
        const std::vector<ColorRgba> pixelBuffer(pixels.begin(), pixels.end());
        std::ofstream fileStream = CreateFile(imagePath);
        ImagesInterop::SetFlipVerticallyOnSave(false);
        ImagesInterop::WritePngRgba(pixelBuffer, width, height, fileStream);
    }

    void Images::ExportHudLayers()
    {
        Hud::HudInfo::TestLayers(true);
    }

    void Images::ExportHudObjects()
    {
        Hud::HudInfo::TestObjects(std::nullopt, 0, 0, 0, 0, true);
    }
}
