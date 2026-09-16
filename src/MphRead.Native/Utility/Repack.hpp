#pragma once

#include "../Formats/Formats.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace MphRead
{
    class Model;
}

namespace MphRead::Utility
{
    enum class RepackFilter : std::int32_t
    {
        All = 0,
        SinglePlayer = 1,
        Multiplayer = 2
    };

    class BinaryWriter final
    {
    public:
        [[nodiscard]] std::size_t Position() const noexcept;
        void Position(std::size_t value);
        [[nodiscard]] std::vector<std::uint8_t> ToArray() const;

        void Write(std::uint8_t value);
        void Write(std::int8_t value);
        void Write(std::uint16_t value);
        void Write(std::int16_t value);
        void Write(std::uint32_t value);
        void Write(std::int32_t value);

        void WriteString(const std::string& value, std::size_t length);
        void WriteFloat(float value);
        void WriteVector3(OpenTK::Mathematics::Vector3 value);
        void WriteVector4(OpenTK::Mathematics::Vector4 value);
        void WriteColorRgb(ColorRgb value);
        void WriteByte(bool value);
        void WriteInt(bool value);

    private:
        [[nodiscard]] const std::vector<std::uint8_t>& Bytes() const noexcept;

        std::vector<std::uint8_t> _bytes{};
        std::size_t _position = 0;

        void WriteRaw(std::uint32_t value, std::size_t count);
    };

    class Repack final
    {
    public:
        [[nodiscard]] static std::vector<std::uint8_t> RepackMphEntities(const std::string& room);
        [[nodiscard]] static std::vector<std::uint8_t> RepackFhEntities(
            const std::string& room, RepackFilter filter = RepackFilter::All);
        [[nodiscard]] static std::vector<std::uint8_t> RepackHook(
            const std::string& path, bool firstHunt);
        [[nodiscard]] static std::vector<std::uint8_t> TestEntityEdit();
        static void TestEntities();

        static void CompareRooms(
            const std::string& room1,
            const std::string& room2,
            const std::string& game1 = "amhe1",
            const std::string& game2 = "amhe1");
        static void PrintLayers(std::uint16_t mask);

        static void WriteVolume(BinaryWriter& writer, const CollisionVolume& volume);
        static void WriteFhVolume(BinaryWriter& writer, const CollisionVolume& volume);

        enum class RepackTexture : std::int32_t
        {
            Inline = 0,
            Separate = 1,
            Shared = 2
        };

        enum class ComputeBounds : std::int32_t
        {
            None = 0,
            Capped = 1,
            Uncapped = 2
        };

        class RepackOptions
        {
        public:
            RepackTexture Texture = RepackTexture::Inline;
            bool IsRoom = false;
            Repack::ComputeBounds ComputeBounds = Repack::ComputeBounds::None;
            bool WriteFile = false;
            bool Compare = false;
        };

        class TextureInfo
        {
        public:
            const TextureFormat Format;
            const bool Opaque;
            const std::uint16_t Height;
            const std::uint16_t Width;
            const std::shared_ptr<const std::vector<std::uint8_t>> Data;

            TextureInfo(TextureFormat format, bool opaque, std::uint16_t height,
                std::uint16_t width, std::shared_ptr<const std::vector<std::uint8_t>> data);
        };

        class PaletteInfo
        {
        public:
            std::shared_ptr<const std::vector<std::uint16_t>> Data;

            explicit PaletteInfo(std::shared_ptr<const std::vector<std::uint16_t>> data);
        };

        class ImageInfo
        {
        public:
            const TextureFormat Format;
            const std::uint16_t Height;
            const std::uint16_t Width;
            const std::shared_ptr<const std::vector<ColorRgba>> Pixels;
            const bool PaletteOpaque;

            ImageInfo(TextureFormat format, std::uint16_t height, std::uint16_t width,
                std::shared_ptr<const std::vector<ColorRgba>> pixels, bool paletteOpaque);
        };

        [[nodiscard]] static std::pair<std::vector<std::uint8_t>, std::vector<std::uint8_t>>
            RepackRoomModel(const std::string& room, bool separateTextures,
                RepackFilter filter = RepackFilter::All);

        static void TestRepack();
        static void TestRepack(const std::string& name, std::int32_t recolor = 0,
            bool firstHunt = false);

        static void CompareModels(const std::string& model1, const std::string& model2,
            const std::string& game1 = "amhe1", const std::string& game2 = "amhe1");
        static void CompareAnims(const std::string& model1, const std::string& model2,
            const std::string& game1 = "amhe1", const std::string& game2 = "amhe1");

        [[nodiscard]] static std::vector<std::uint8_t> PackAnim(
            const std::shared_ptr<const std::vector<std::shared_ptr<NodeAnimationGroup>>>& nodeGroups,
            const std::shared_ptr<const std::vector<std::shared_ptr<MaterialAnimationGroup>>>& matGroups,
            const std::shared_ptr<const std::vector<std::shared_ptr<TexcoordAnimationGroup>>>& uvGroups,
            const std::shared_ptr<const std::vector<std::shared_ptr<TextureAnimationGroup>>>& texGroups,
            bool fhPad);

        [[nodiscard]] static std::pair<std::vector<std::uint8_t>, std::vector<std::uint8_t>>
            PackModel(const std::shared_ptr<Model>& model);

        [[nodiscard]] static std::pair<std::vector<std::uint8_t>, std::vector<std::uint8_t>>
            PackModel(
                std::int32_t scale,
                const std::shared_ptr<const std::vector<std::int32_t>>& nodeMtxIds,
                const std::shared_ptr<const std::vector<std::int32_t>>& nodePosScaleCounts,
                const std::shared_ptr<const std::vector<std::shared_ptr<Material>>>& materials,
                const std::shared_ptr<const std::vector<std::shared_ptr<TextureInfo>>>& textures,
                const std::shared_ptr<const std::vector<std::shared_ptr<PaletteInfo>>>& palettes,
                const std::shared_ptr<const std::vector<std::shared_ptr<Node>>>& nodes,
                const std::shared_ptr<const std::vector<std::shared_ptr<Mesh>>>& meshes,
                const std::shared_ptr<const std::vector<
                    std::shared_ptr<const std::vector<std::shared_ptr<RenderInstruction>>>>>& renders,
                const std::shared_ptr<const std::vector<DisplayList>>& dlists,
                const std::shared_ptr<RepackOptions>& options);

        [[nodiscard]] static std::shared_ptr<TextureInfo> ConvertData(
            const Texture& texture,
            const std::shared_ptr<const std::vector<TextureData>>& data);

        [[nodiscard]] static std::pair<std::shared_ptr<TextureInfo>, std::shared_ptr<PaletteInfo>>
            ConvertImage(const std::shared_ptr<ImageInfo>& image);

        static void WriteString(BinaryWriter& writer, const std::string& value, std::int32_t length);
        static void WriteFloat(BinaryWriter& writer, float value);
        static void WriteVector3(BinaryWriter& writer, OpenTK::Mathematics::Vector3 vector);
        static void WriteVector4(BinaryWriter& writer, OpenTK::Mathematics::Vector4 vector);
        static void WriteColorRgb(BinaryWriter& writer, ColorRgb color);
        static void WriteAngle(BinaryWriter& writer, float angle);
        static void WriteAngles(BinaryWriter& writer, OpenTK::Mathematics::Vector3 angles);
        static void WriteByte(BinaryWriter& writer, bool value);
        static void WriteInt(BinaryWriter& writer, bool value);

        Repack() = delete;
        Repack(const Repack&) = delete;
        Repack& operator=(const Repack&) = delete;

    private:
        class TexMtxMap;

        static void TestAnimRepack(
            const std::shared_ptr<Model>& model, const std::string& animPath,
            bool firstHunt, bool writeFile);
        static void TestModelRepack(
            const std::shared_ptr<Model>& model, std::int32_t recolor,
            const std::string& modelPath, const std::string* texPath,
            bool firstHunt, const std::shared_ptr<RepackOptions>& options);

        static void CompareModels(const std::string& name,
            const std::vector<std::uint8_t>& bytes,
            const std::vector<std::uint8_t>& otherBytes,
            const std::shared_ptr<RepackOptions>& options);
        static void CompareAnims(const std::string& name,
            const std::vector<std::uint8_t>& bytes,
            const std::vector<std::uint8_t>& otherBytes);

        [[nodiscard]] static std::int32_t WriteNodeGroup(
            const std::shared_ptr<NodeAnimationGroup>& group, bool fhPad, BinaryWriter& writer);
        [[nodiscard]] static std::int32_t WriteMatGroup(
            const std::shared_ptr<MaterialAnimationGroup>& group, bool fhPad, BinaryWriter& writer);
        [[nodiscard]] static std::int32_t WriteUvGroup(
            const std::shared_ptr<TexcoordAnimationGroup>& group, bool fhPad, BinaryWriter& writer);
        [[nodiscard]] static std::int32_t WriteTexGroup(
            const std::shared_ptr<TextureAnimationGroup>& group, bool fhPad, BinaryWriter& writer);

        [[nodiscard]] static std::pair<OpenTK::Mathematics::Vector3i,
            OpenTK::Mathematics::Vector3i> CalculateBounds(
                const std::shared_ptr<const std::vector<std::shared_ptr<RenderInstruction>>>& insts);

        [[nodiscard]] static std::pair<std::int32_t, std::int32_t> GetDlistCounts(
            const std::shared_ptr<const std::vector<std::shared_ptr<RenderInstruction>>>& dlist);

        static void WriteTextureMeta(
            const std::shared_ptr<TextureInfo>& texture, std::int32_t offset, BinaryWriter& writer);
        [[nodiscard]] static std::int32_t WriteRenderInstructions(
            const std::shared_ptr<const std::vector<std::shared_ptr<RenderInstruction>>>& list,
            BinaryWriter& writer);
        [[nodiscard]] static std::int32_t GetTextureMatrixId(
            const std::shared_ptr<Material>& material, std::int32_t& indexCount);
        static void WriteMaterial(
            const std::shared_ptr<Material>& material, std::int32_t matrixId, BinaryWriter& writer);
        static void WriteNode(
            const std::shared_ptr<Node>& node,
            OpenTK::Mathematics::Vector3i minBounds,
            OpenTK::Mathematics::Vector3i maxBounds,
            BinaryWriter& writer);

        static void Nop();
    };
}
