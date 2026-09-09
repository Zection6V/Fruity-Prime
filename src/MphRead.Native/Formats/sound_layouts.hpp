#pragma once

// SDAT container records and the sound-table entries read from them.
// Transliterated from the managed sources so the field names and order stay
// checkable against them.  A C# reference member becomes a pointer.

#include "Formats/Types.hpp"
#include "Formats/raw_formats.hpp"
#include "Formats/enum_tables.hpp"


#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::formats {



struct SdatHeader;
struct BlockHeader;
struct SdatFatHeader;
struct SdatFatEntry;
struct SdatFileHeader;
struct SeqInfo;
struct GroupItemInfo;
struct StrmPlayerInfo;
struct StrmInfo;
struct SoundSampleHeader;
struct SoundStreamHeader;
struct SoundStream;
struct SoundSample;
struct RawSoundSelectEntry;
struct SoundSelectEntry;
struct RawSoundTableEntry;
struct RawMusicTrack;
struct RawSfxScriptEntry;
struct DgnData;
struct DgnFileEntry;
struct FhSoundSampleHeader;

// Sound.cs
struct SdatHeader {
    std::vector<char> Type;
    std::uint32_t Magic{};
    std::uint32_t FileSize{};
    std::uint16_t HeaderSize{};
    std::uint16_t BlockCount{};
    std::uint32_t SymbolBlockOffset{};
    std::uint32_t SymbolBlockSize{};
    std::uint32_t InfoBlockOffset{};
    std::uint32_t InfoBlockSize{};
    std::uint32_t FatOffset{};
    std::uint32_t FatSize{};
    std::uint32_t FileBlockOffset{};
    std::uint32_t FileBlockSize{};
    std::vector<std::uint8_t> Reserved;
};

// Sound.cs
struct BlockHeader {
    std::vector<char> Type;
    std::uint32_t HeaderSize{};
    std::uint32_t SeqOffset{};
    std::uint32_t SeqarcOffset{};
    std::uint32_t BankOffset{};
    std::uint32_t WavearcOffset{};
    std::uint32_t PlayerOffset{};
    std::uint32_t GroupOffset{};
    std::uint32_t StrmPlayerOffset{};
    std::uint32_t StrmOffset{};
    std::vector<std::uint8_t> Reserved;
};

// Sound.cs
struct SdatFatHeader {
    std::vector<char> Type;
    std::uint32_t Size{};
    std::uint32_t Count{};
};

// Sound.cs
struct SdatFatEntry {
    std::uint32_t Offset{};
    std::uint32_t Size{};
    std::uint32_t Pointer{};
    std::uint32_t Reserved{};
};

// Sound.cs
struct SdatFileHeader {
    std::vector<char> Type;
    std::uint32_t Size{};
    std::uint32_t Count{};
    std::uint32_t Reserved{};
};

// Sound.cs
struct SeqInfo {
    std::uint32_t FileId{};
    std::uint16_t BankNo{};
    std::uint8_t Volume{};
    std::uint8_t ChannelPriority{};
    std::uint8_t PlayerPriority{};
    std::uint8_t PlayerNo{};
    std::uint16_t Reserved{};
};

// Sound.cs
struct GroupItemInfo {
    std::uint8_t Type{};
    std::uint8_t LoadTypes{};
    std::uint16_t Padding2{};
    std::uint32_t LoadIndex{};
};

// Sound.cs
struct StrmPlayerInfo {
    std::uint8_t ChannelCount{};
    std::vector<std::uint8_t> ChannelNumbers;
    std::vector<std::uint8_t> Reserved;
};

// Sound.cs
struct StrmInfo {
    std::uint32_t FileId{};
    std::uint8_t Volume{};
    std::uint8_t PlayerPriority{};
    std::uint8_t PlayerNo{};
    std::uint8_t Flags{};
};

// Sound.cs
struct SoundSampleHeader {
    std::uint8_t Format{};
    std::uint8_t LoopFlag{};
    std::uint16_t SampleRate{};
    std::uint16_t Timer{};
    std::uint16_t LoopStart{};
    std::uint32_t LoopLength{};
};

// Sound.cs
struct SoundStreamHeader {
    std::vector<char> Type;
    std::uint32_t Magic{};
    std::uint32_t DataSize{};
    std::uint16_t Size{};
    std::uint16_t DataBlocks{};
    std::vector<char> HeadType;
    std::uint32_t HeaderSize{};
    std::uint8_t Format{};
    std::uint8_t LoopFlag{};
    std::uint8_t Channels{};
    std::uint8_t Padding25{};
    std::uint16_t SampleRate{};
    std::uint16_t Timer{};
    std::uint32_t LoopStart{};
    std::uint32_t LoopEnd{};
    std::uint32_t DataOffset{};
    std::uint32_t BlockCount{};
    std::uint32_t BlockSize{};
    std::uint32_t BlockSamples{};
    std::uint32_t LastBlockSize{};
    std::uint32_t LastBlockSamples{};
};

// Sound.cs
struct SoundStream {
    std::int32_t Id{};
    std::string_view Name;
    WaveFormat Format{};
    bool Loop{};
    std::uint16_t SampleRate{};
    std::uint32_t LoopStart{};
    std::uint32_t LoopEnd{};
    float Volume{};
    std::vector<std::uint8_t> BufferData;
};

// Sound.cs
struct SoundSample {
    std::uint32_t Id{};
    std::uint32_t Offset{};
    WaveFormat Format{};
    bool Loop{};
    std::uint16_t SampleRate{};
    std::uint32_t SampleStart{};
    std::uint32_t SampleLength{};
    std::int32_t LoopStart{};
    std::int32_t LoopLength{};
    float Volume{};
    std::vector<std::uint8_t> WaveData;
    std::int32_t BufferId{};
    std::int32_t MaxBuffers{};
    std::int32_t BufferCount{};
    std::int32_t References{};
    std::string_view Name;
};

// Sound.cs
struct RawSoundSelectEntry {
    std::uint16_t Id{};
    std::uint16_t Type{};
    std::uint32_t Field4{};
    std::uint32_t Field8{};
    std::uint32_t FieldC{};
};

// Sound.cs
struct SoundSelectEntry {
    std::string_view Name;
    std::uint16_t Id{};
    std::uint16_t Type{};
    std::uint32_t Field4{};
    std::uint32_t Field8{};
    std::uint32_t FieldC{};
};

// Sound.cs
struct RawSoundTableEntry {
    std::uint16_t Exists{};
    std::uint8_t CategoryId{};
    std::uint8_t SlotCount{};
    std::uint8_t InitialVolume{};
    std::uint8_t Priority{};
    std::uint16_t Size{};
    std::uint32_t Data{};
};

// Sound.cs
struct RawMusicTrack {
    std::uint16_t SeqId{};
    std::uint16_t FadeOutFrames{};
    std::uint16_t Tracks{};
    std::uint16_t FadeInFrames{};
};

// Sound.cs
struct RawSfxScriptEntry {
    std::uint16_t SfxId{};
    std::uint16_t Delay{};
    std::uint8_t Volume{};
    std::uint8_t Pan{};
    std::uint16_t Pitch{};
    std::uint32_t Handle{};
};

// Sound.cs
struct DgnData {
    std::uint16_t Amount{};
    std::uint16_t Value{};
};

// Sound.cs
struct DgnFileEntry {
    SfxId SfxId{};
    std::vector<DgnData> Data1;
    std::vector<DgnData> Data2;
    std::vector<DgnData> Data3;
    std::vector<DgnData> Data4;
};

// FhSound.cs
struct FhSoundSampleHeader {
    std::uint32_t DataSize{};
    std::uint32_t DataPointer{};
    std::uint32_t SampleRate{};
    std::uint16_t Volume{};
    std::uint8_t FieldE{};
    std::uint8_t Format{};
    std::uint32_t LoopStart{};
    std::uint32_t LoopEnd{};
};

} // namespace fruityprime::formats
