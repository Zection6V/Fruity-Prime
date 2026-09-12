#pragma once
#include "Enums.hpp"
#include "Types.hpp"
#include "../Metadata/SoundMeta.hpp"
#include "../Program.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <ostream>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
namespace MphRead::Formats::Sound
{
struct FhSoundSampleHeader;
class SoundSample;
class SoundStream;
class SoundData;
class SoundSelectEntry;
class SoundTable;
class SoundTableEntry;
class MusicTrack;
class SfxScriptFile;
class DgnFile;
struct Sound3dEntry;
struct RoomMusic;
namespace FhSoundDependency
{
[[nodiscard]]std::shared_ptr<SoundSample> CreateNullSoundSample(std::uint32_t id);
[[nodiscard]]std::shared_ptr<SoundSample> CreateFhSoundSample(
std::uint32_t id,std::uint32_t offset,FhSoundSampleHeader header,
std::span<const std::uint8_t> data);
void ExportSamples(
const std::shared_ptr<const std::vector<std::shared_ptr<SoundSample>>>&samples,
bool adpcmRoundingError,const std::string&prefix);
}
namespace SoundNativeRuntime
{
template<typename T,std::size_t N,typename TWire=T>
class ByValArray final
{
public:
using value_type=T;
using wire_type=TWire;
using ManagedStorage=std::vector<T>;
ByValArray()noexcept=default;
explicit ByValArray(const std::array<TWire,N>&wire)
:_wire(wire)
{
Materialize();
}
ByValArray(const ByValArray&other)
:_wire(other.WireValues())
{
if(auto storage=other.TryGetStorage())
{
SetStorage(std::move(storage));
}
}
ByValArray&operator=(const ByValArray&other)
{
if(this!=std::addressof(other))
{
_wire=other.WireValues();
if(auto storage=other.TryGetStorage())
{
SetStorage(std::move(storage));
}
else
{
ClearStorage();
}
}
return*this;
}
~ByValArray()noexcept
{
ClearStorageNoThrow();
}
[[nodiscard]]bool IsNull()const
{
return!TryGetStorage();
}
[[nodiscard]]constexpr std::size_t size()const noexcept
{
return N;
}
[[nodiscard]]std::shared_ptr<const ManagedStorage> Managed()const
{
return TryGetStorage();
}
[[nodiscard]]T operator[](std::size_t index)const
{
auto storage=TryGetStorage();
if(!storage)
{
throw System::NullReferenceException();
}
return storage->at(index);
}
[[nodiscard]]std::string MarshalString()const
requires std::is_same_v<T,char16_t>
{
auto storage=TryGetStorage();
if(!storage)
{
throw System::ArgumentNullException("array");
}
std::string result;
result.reserve(N);
for(const char16_t value:*storage)
{
if(value==u'\0')
{
break;
}
if(value<=0x7F)
{
result.push_back(static_cast<char>(value));
}
else if(value<=0x7FF)
{
result.push_back(static_cast<char>(0xC0|(value>> 6)));
result.push_back(static_cast<char>(0x80|(value&0x3F)));
}
else
{
result.push_back(static_cast<char>(0xE0|(value>> 12)));
result.push_back(static_cast<char>(0x80|((value>> 6)&0x3F)));
result.push_back(static_cast<char>(0x80|(value&0x3F)));
}
}
return result;
}
[[nodiscard]]const std::array<TWire,N>&WireValues()const
{
if(auto storage=TryGetStorage())
{
for(std::size_t i=0;i<N;++i)
{
_wire[i]=static_cast<TWire>((*storage)[i]);
}
}
return _wire;
}
private:
struct Registry final
{
std::mutex Mutex;
std::unordered_map<const ByValArray*,std::shared_ptr<ManagedStorage>> Storage;
};
[[nodiscard]]static Registry&GetRegistry()
{
static Registry*registry=new Registry();
return*registry;
}
[[nodiscard]]std::shared_ptr<ManagedStorage> TryGetStorage()const
{
Registry&registry=GetRegistry();
std::lock_guard<std::mutex> lock(registry.Mutex);
auto it=registry.Storage.find(this);
return it==registry.Storage.end()?nullptr:it->second;
}
void SetStorage(std::shared_ptr<ManagedStorage> storage)const
{
Registry&registry=GetRegistry();
std::lock_guard<std::mutex> lock(registry.Mutex);
registry.Storage[this]=std::move(storage);
}
void ClearStorage()const
{
Registry&registry=GetRegistry();
std::lock_guard<std::mutex> lock(registry.Mutex);
registry.Storage.erase(this);
}
void ClearStorageNoThrow()const noexcept
{
try
{
ClearStorage();
}
catch(...)
{
}
}
void Materialize()
{
auto storage=std::make_shared<ManagedStorage>();
storage->reserve(N);
for(const TWire value:_wire)
{
storage->push_back(static_cast<T>(value));
}
SetStorage(std::move(storage));
}
mutable std::array<TWire,N> _wire{};
};
using Char4=ByValArray<char16_t,4,std::uint8_t>;
using Bytes7=ByValArray<std::uint8_t,7>;
using Bytes16=ByValArray<std::uint8_t,16>;
using Bytes24=ByValArray<std::uint8_t,24>;
using Ushort3=ByValArray<std::uint16_t,3>;
using Ushort4=ByValArray<std::uint16_t,4>;
class LazyByteArray final
{
public:
using ValueType=std::shared_ptr<std::vector<std::uint8_t>>;
using Factory=std::function<ValueType()>;
explicit LazyByteArray(Factory factory);
[[nodiscard]]ValueType Value()const;
[[nodiscard]]bool IsValueCreated()const noexcept;
LazyByteArray(const LazyByteArray&)=delete;
LazyByteArray&operator=(const LazyByteArray&)=delete;
LazyByteArray(LazyByteArray&&)=delete;
LazyByteArray&operator=(LazyByteArray&&)=delete;
private:
mutable std::mutex _mutex;
mutable bool _evaluated=false;
mutable ValueType _value{};
mutable std::exception_ptr _exception{};
mutable Factory _factory;
};
}
class BinaryWriter final
{
public:
explicit BinaryWriter(std::ostream&stream)noexcept;
void Write(std::uint8_t value);
void Write(std::int16_t value);
void Write(std::uint16_t value);
void Write(std::int32_t value);
void Write(std::uint32_t value);
void Write(char16_t value);
void Write(std::span<const std::uint8_t> bytes);
private:
std::ostream*_stream;
std::optional<char16_t> _pendingHighSurrogate{};
void WriteRaw(const void*bytes,std::size_t count);
void WriteUtf8Scalar(std::uint32_t scalar);
};
class SoundRead final
{
public:
SoundRead()=delete;
static void ExportSamples(bool adpcmRoundingError=false);
static void ExportWfsSamples(bool adpcmRoundingError=false);
private:
static void ExportSamples(
const std::shared_ptr<const std::vector<std::shared_ptr<SoundSample>>>&samples,
bool adpcmRoundingError,const std::optional<std::string>&prefix=std::nullopt);
public:
static void ExportSample(std::int32_t id,bool adpcmRoundingError=false);
static void ExportWfsSample(std::int32_t id,bool adpcmRoundingError=false);
[[nodiscard]]static std::shared_ptr<std::vector<std::uint8_t>> GetWaveData(
const std::shared_ptr<SoundSample>&sample,bool adpcmRoundingError=false);
private:
static void GetWaveData(std::span<const std::uint8_t> data,WaveFormat format,
std::uint32_t sampleCount,bool adpcmRoundingError,BinaryWriter&writer);
[[nodiscard]]static std::uint32_t GetSampleCount(const SoundSample&sample)noexcept;
static void ExportSample(const std::shared_ptr<SoundSample>&sample,
bool adpcmRoundingError=false,const std::optional<std::string>&prefix=std::nullopt);
public:
static void WriteWavHeader(BinaryWriter&writer,std::uint32_t sampleCount,
std::uint16_t sampleRate,WaveFormat format);
private:
static void ExportAudio(std::span<const std::uint8_t> waveData,std::uint32_t sampleCount,
std::uint16_t sampleRate,WaveFormat format,const std::string&name,
const std::optional<std::string>&prefix=std::nullopt);
public:
[[nodiscard]]static std::shared_ptr<const std::vector<std::shared_ptr<SoundSample>>> ReadSoundSamples();
[[nodiscard]]static std::shared_ptr<const std::vector<std::shared_ptr<SoundSample>>> ReadWfsSoundSamples();
private:
[[nodiscard]]static std::shared_ptr<const std::vector<std::shared_ptr<SoundSample>>> ReadSoundSamples(
const std::string&filename);
public:
[[nodiscard]]static std::shared_ptr<const std::vector<std::shared_ptr<SoundSelectEntry>>> ReadBgmSelectList();
[[nodiscard]]static std::shared_ptr<const std::vector<std::shared_ptr<SoundSelectEntry>>> ReadSfxSelectList();
private:
[[nodiscard]]static std::shared_ptr<const std::vector<std::shared_ptr<SoundSelectEntry>>> ReadSelectList(
const std::string&filename);
public:
[[nodiscard]]static std::shared_ptr<const std::vector<Sound3dEntry>> ReadSound3dList();
[[nodiscard]]static std::shared_ptr<SoundTable> ReadSoundTables();
[[nodiscard]]static std::shared_ptr<const std::vector<RoomMusic>> ReadAssignMusic();
[[nodiscard]]static std::shared_ptr<const std::vector<std::shared_ptr<MusicTrack>>> ReadInterMusicInfo();
[[nodiscard]]static std::shared_ptr<const std::vector<std::shared_ptr<SfxScriptFile>>> ReadSfxScriptFiles();
[[nodiscard]]static std::shared_ptr<const std::vector<std::shared_ptr<DgnFile>>> ReadDgnFiles();
static void Nop()noexcept;
[[nodiscard]]static std::shared_ptr<SoundData> ReadSdat();
static void ExportStreams();
static void ExportStream(const std::shared_ptr<SoundStream>&stream);
[[nodiscard]]static std::shared_ptr<std::vector<std::uint8_t>> GetStreamBufferData(
const SoundStream&stream);
private:
struct SeqArcEntries
{
const std::uint32_t EntryOffset;
const std::uint32_t FilesOffset;
SeqArcEntries(std::uint32_t entryOffset,std::uint32_t filesOffset)noexcept
:EntryOffset(entryOffset),FilesOffset(filesOffset)
{
}
[[nodiscard]]static SeqArcEntries FromMarshaledBytes(
const std::array<std::uint8_t,8>&bytes)noexcept
{
const std::uint32_t entry=static_cast<std::uint32_t>(bytes[0])
|static_cast<std::uint32_t>(bytes[1])<<8
|static_cast<std::uint32_t>(bytes[2])<<16
|static_cast<std::uint32_t>(bytes[3])<<24;
const std::uint32_t files=static_cast<std::uint32_t>(bytes[4])
|static_cast<std::uint32_t>(bytes[5])<<8
|static_cast<std::uint32_t>(bytes[6])<<16
|static_cast<std::uint32_t>(bytes[7])<<24;
return SeqArcEntries(entry,files);
}
};
static_assert(sizeof(SeqArcEntries)==8);
friend void FhSoundDependency::ExportSamples(
const std::shared_ptr<const std::vector<std::shared_ptr<SoundSample>>>&samples,
bool adpcmRoundingError,const std::string&prefix);
public:
static void ExportAllFh(bool adpcmRoundingError=false);
static void ExportFhSfx(bool adpcmRoundingError=false);
static void ExportFhBgm(bool adpcmRoundingError=false);
static void ExportFhMenuSfx(bool adpcmRoundingError=false);
static void ExportFhGlobalSfx(bool adpcmRoundingError=false);
[[nodiscard]]static std::shared_ptr<const std::vector<std::shared_ptr<SoundSample>>> ReadFhSfx();
[[nodiscard]]static std::shared_ptr<const std::vector<std::shared_ptr<SoundSample>>> ReadFhBgm();
[[nodiscard]]static std::shared_ptr<const std::vector<std::shared_ptr<SoundSample>>> ReadFhMenuSfx();
[[nodiscard]]static std::shared_ptr<const std::vector<std::shared_ptr<SoundSample>>> ReadFhGlobalSfx();
[[nodiscard]]static std::shared_ptr<const std::vector<std::shared_ptr<SoundSample>>> ReadFhSoundFile(
const std::string&filename);
};
struct SdatHeader
{
const SoundNativeRuntime::Char4 Type{};
const std::uint32_t Magic=0;
const std::uint32_t FileSize=0;
const std::uint16_t HeaderSize=0;
const std::uint16_t BlockCount=0;
const std::uint32_t SymbolBlockOffset=0;
const std::uint32_t SymbolBlockSize=0;
const std::uint32_t InfoBlockOffset=0;
const std::uint32_t InfoBlockSize=0;
const std::uint32_t FatOffset=0;
const std::uint32_t FatSize=0;
const std::uint32_t FileBlockOffset=0;
const std::uint32_t FileBlockSize=0;
const SoundNativeRuntime::Bytes16 Reserved{};
SdatHeader()noexcept=default;
[[nodiscard]]static SdatHeader FromMarshaledBytes(const std::array<std::uint8_t,64>&bytes);
private:
SdatHeader(SoundNativeRuntime::Char4 type,std::uint32_t magic,std::uint32_t fileSize,
std::uint16_t headerSize,std::uint16_t blockCount,std::uint32_t symbolBlockOffset,
std::uint32_t symbolBlockSize,std::uint32_t infoBlockOffset,std::uint32_t infoBlockSize,
std::uint32_t fatOffset,std::uint32_t fatSize,std::uint32_t fileBlockOffset,
std::uint32_t fileBlockSize,SoundNativeRuntime::Bytes16 reserved);
};
struct BlockHeader
{
const SoundNativeRuntime::Char4 Type{};
const std::uint32_t HeaderSize=0;
const std::uint32_t SeqOffset=0;
const std::uint32_t SeqarcOffset=0;
const std::uint32_t BankOffset=0;
const std::uint32_t WavearcOffset=0;
const std::uint32_t PlayerOffset=0;
const std::uint32_t GroupOffset=0;
const std::uint32_t StrmPlayerOffset=0;
const std::uint32_t StrmOffset=0;
const SoundNativeRuntime::Bytes24 Reserved{};
BlockHeader()noexcept=default;
[[nodiscard]]static BlockHeader FromMarshaledBytes(const std::array<std::uint8_t,64>&bytes);
private:
BlockHeader(SoundNativeRuntime::Char4 type,std::uint32_t headerSize,
std::uint32_t seqOffset,std::uint32_t seqarcOffset,std::uint32_t bankOffset,
std::uint32_t wavearcOffset,std::uint32_t playerOffset,std::uint32_t groupOffset,
std::uint32_t strmPlayerOffset,std::uint32_t strmOffset,
SoundNativeRuntime::Bytes24 reserved);
};
struct SdatFatHeader
{
const SoundNativeRuntime::Char4 Type{};
const std::uint32_t Size=0;
const std::uint32_t Count=0;
SdatFatHeader()noexcept=default;
[[nodiscard]]static SdatFatHeader FromMarshaledBytes(const std::array<std::uint8_t,12>&bytes);
private:
SdatFatHeader(SoundNativeRuntime::Char4 type,std::uint32_t size,std::uint32_t count);
};
struct SdatFatEntry
{
const std::uint32_t Offset=0;
const std::uint32_t Size=0;
const std::uint32_t Pointer=0;
const std::uint32_t Reserved=0;
};
struct SdatFileHeader
{
const SoundNativeRuntime::Char4 Type{};
const std::uint32_t Size=0;
const std::uint32_t Count=0;
const std::uint32_t Reserved=0;
SdatFileHeader()noexcept=default;
[[nodiscard]]static SdatFileHeader FromMarshaledBytes(const std::array<std::uint8_t,16>&bytes);
private:
SdatFileHeader(SoundNativeRuntime::Char4 type,std::uint32_t size,
std::uint32_t count,std::uint32_t reserved);
};
struct SeqInfo
{
const std::uint32_t FileId=0;
const std::uint16_t BankNo=0;
const std::uint8_t Volume=0;
const std::uint8_t ChannelPriority=0;
const std::uint8_t PlayerPriority=0;
const std::uint8_t PlayerNo=0;
const std::uint16_t Reserved=0;
};
struct BankInfo
{
const std::uint32_t FileId=0;
const SoundNativeRuntime::Ushort4 WaveArcNo{};
BankInfo()noexcept=default;
[[nodiscard]]static BankInfo FromMarshaledBytes(const std::array<std::uint8_t,12>&bytes);
private:
BankInfo(std::uint32_t fileId,SoundNativeRuntime::Ushort4 waveArcNo);
};
struct PlayerInfo
{
const std::uint8_t MaxSequences=0;
const std::uint8_t Padding1=0;
const std::uint16_t AllocateChannelBits=0;
const std::uint32_t HeapSize=0;
};
struct GroupItemInfo
{
const std::uint8_t Type=0;
const std::uint8_t LoadTypes=0;
const std::uint16_t Padding2=0;
const std::uint32_t LoadIndex=0;
};
struct StrmPlayerInfo
{
const std::uint8_t ChannelCount=0;
const SoundNativeRuntime::Bytes16 ChannelNumbers{};
const SoundNativeRuntime::Bytes7 Reserved{};
StrmPlayerInfo()noexcept=default;
[[nodiscard]]static StrmPlayerInfo FromMarshaledBytes(const std::array<std::uint8_t,24>&bytes);
private:
StrmPlayerInfo(std::uint8_t channelCount,SoundNativeRuntime::Bytes16 channelNumbers,
SoundNativeRuntime::Bytes7 reserved);
};
struct StrmInfo
{
const std::uint32_t FileId=0;
const std::uint8_t Volume=0;
const std::uint8_t PlayerPriority=0;
const std::uint8_t PlayerNo=0;
const std::uint8_t Flags=0;
};
struct SoundSampleHeader
{
const std::uint8_t Format=0;
const std::uint8_t LoopFlag=0;
const std::uint16_t SampleRate=0;
const std::uint16_t Timer=0;
const std::uint16_t LoopStart=0;
const std::uint32_t LoopLength=0;
};
struct SoundStreamHeader
{
const SoundNativeRuntime::Char4 Type{};
const std::uint32_t Magic=0;
const std::uint32_t DataSize=0;
const std::uint16_t Size=0;
const std::uint16_t DataBlocks=0;
const SoundNativeRuntime::Char4 HeadType{};
const std::uint32_t HeaderSize=0;
const std::uint8_t Format=0;
const std::uint8_t LoopFlag=0;
const std::uint8_t Channels=0;
const std::uint8_t Padding25=0;
const std::uint16_t SampleRate=0;
const std::uint16_t Timer=0;
const std::uint32_t LoopStart=0;
const std::uint32_t LoopEnd=0;
const std::uint32_t DataOffset=0;
const std::uint32_t BlockCount=0;
const std::uint32_t BlockSize=0;
const std::uint32_t BlockSamples=0;
const std::uint32_t LastBlockSize=0;
const std::uint32_t LastBlockSamples=0;
SoundStreamHeader()noexcept=default;
[[nodiscard]]static SoundStreamHeader FromMarshaledBytes(const std::array<std::uint8_t,64>&bytes);
private:
SoundStreamHeader(SoundNativeRuntime::Char4 type,std::uint32_t magic,
std::uint32_t dataSize,std::uint16_t size,std::uint16_t dataBlocks,
SoundNativeRuntime::Char4 headType,std::uint32_t headerSize,std::uint8_t format,
std::uint8_t loopFlag,std::uint8_t channels,std::uint8_t padding25,
std::uint16_t sampleRate,std::uint16_t timer,std::uint32_t loopStart,
std::uint32_t loopEnd,std::uint32_t dataOffset,std::uint32_t blockCount,
std::uint32_t blockSize,std::uint32_t blockSamples,std::uint32_t lastBlockSize,
std::uint32_t lastBlockSamples);
};
class SoundData
{
public:
const std::shared_ptr<const std::vector<std::shared_ptr<SoundStream>>> Streams;
explicit SoundData(std::shared_ptr<const std::vector<std::shared_ptr<SoundStream>>> streams);
SoundData(const SoundData&)=delete;
SoundData&operator=(const SoundData&)=delete;
};
class SoundStream
{
public:
const std::int32_t Id;
const std::string Name;
const WaveFormat Format;
const bool Loop;
const std::uint16_t SampleRate;
const std::uint32_t LoopStart;
const std::uint32_t LoopEnd;
private:
const std::shared_ptr<const std::vector<std::shared_ptr<std::vector<std::uint8_t>>>> _channels;
public:
const std::shared_ptr<const std::vector<std::shared_ptr<std::vector<std::uint8_t>>>> Channels;
float Volume=1.0F;
const std::shared_ptr<SoundNativeRuntime::LazyByteArray> BufferData;
SoundStream(std::int32_t id,std::string name,SoundStreamHeader header,
std::shared_ptr<const std::vector<std::shared_ptr<std::vector<std::uint8_t>>>> channels,
float volume);
SoundStream(const SoundStream&)=delete;
SoundStream&operator=(const SoundStream&)=delete;
};
class SoundSample
{
public:
const std::uint32_t Id;
const std::uint32_t Offset;
const WaveFormat Format;
const bool Loop;
const std::uint16_t SampleRate;
const std::uint32_t SampleStart;
const std::uint32_t SampleLength;
const std::int32_t LoopStart;
const std::int32_t LoopLength;
private:
const std::shared_ptr<std::vector<std::uint8_t>> _data;
public:
const std::shared_ptr<const std::vector<std::uint8_t>> Data;
float Volume=1.0F;
const std::shared_ptr<SoundNativeRuntime::LazyByteArray> WaveData;
std::int32_t BufferId=0;
std::int32_t MaxBuffers=0;
std::int32_t BufferCount=0;
std::int32_t References=0;
std::optional<std::string> Name{};
SoundSample(std::uint32_t id,std::uint32_t offset,SoundSampleHeader header,
std::span<const std::uint8_t> data);
SoundSample(std::uint32_t id,std::uint32_t offset,FhSoundSampleHeader header,
std::span<const std::uint8_t> data);
[[nodiscard]]static std::shared_ptr<SoundSample> CreateNull(std::uint32_t id);
[[nodiscard]]std::span<const std::uint8_t> CreateSpan()const noexcept;
[[nodiscard]]std::span<const std::uint8_t> GetIntro()const;
[[nodiscard]]std::span<const std::uint8_t> GetLoop()const;
[[nodiscard]]std::span<const std::uint8_t> GetOutro()const;
SoundSample(const SoundSample&)=delete;
SoundSample&operator=(const SoundSample&)=delete;
private:
struct NullTag final{};
explicit SoundSample(std::uint32_t id,NullTag);
};
struct RawSoundSelectEntry
{
const std::uint16_t Id=0;
const std::uint16_t Type=0;
const std::uint32_t Field4=0;
const std::uint32_t Field8=0;
const std::uint32_t FieldC=0;
};
class SoundSelectEntry
{
public:
const std::string Name;
const std::uint16_t Id;
const std::uint16_t Type;
const std::uint32_t Field4;
const std::uint32_t Field8;
const std::uint32_t FieldC;
SoundSelectEntry(std::string name,RawSoundSelectEntry raw);
};
struct Sound3dEntry
{
const std::uint32_t FalloffDistance=0;
const std::uint32_t MaxDistance=0;
};
struct RawSoundTableEntry
{
const std::uint16_t Exists=0;
const std::uint8_t CategoryId=0;
const std::uint8_t SlotCount=0;
const std::uint8_t InitialVolume=0;
const std::uint8_t Priority=0;
const std::uint16_t Size=0;
const std::uint32_t Data=0;
};
class SoundTable
{
public:
const std::shared_ptr<const std::vector<std::shared_ptr<SoundTableEntry>>> Entries;
const std::shared_ptr<const std::vector<std::string>> Categories;
SoundTable(std::shared_ptr<const std::vector<std::shared_ptr<SoundTableEntry>>> entries,
std::shared_ptr<const std::vector<std::string>> categories);
};
class SoundTableEntry
{
public:
const std::string Name;
const std::string Category;
std::uint8_t CategoryId=0;
std::uint8_t SlotCount=0;
std::uint8_t InitialVolume=0;
std::uint8_t Priority=0;
std::uint16_t Size=0;
std::uint32_t Data=0;
SoundTableEntry(std::string name,std::string category,RawSoundTableEntry raw);
};
struct RoomMusic
{
const std::uint16_t RoomId=0;
const SoundNativeRuntime::Ushort3 TrackIds{};
RoomMusic()noexcept=default;
[[nodiscard]]static RoomMusic FromMarshaledBytes(const std::array<std::uint8_t,8>&bytes);
private:
RoomMusic(std::uint16_t roomId,SoundNativeRuntime::Ushort3 trackIds);
};
struct RawMusicTrack
{
const std::uint16_t SeqId=0;
const std::uint16_t FadeOutFrames=0;
const std::uint16_t Tracks=0;
const std::uint16_t FadeInFrames=0;
};
class MusicTrack
{
public:
const MphRead::MusicId MusicId;
const MphRead::SeqId SeqId;
const std::uint16_t Tracks;
const std::uint16_t FadeOutFrames;
const std::uint16_t FadeInFrames;
MusicTrack(std::uint32_t id,RawMusicTrack raw);
};
struct SfxScriptHeader
{
const std::uint32_t Offset=0;
const std::uint16_t Size=0;
const std::uint8_t InitialVolume=0;
const std::uint8_t SlotCount=0;
};
struct RawSfxScriptEntry
{
const std::uint16_t SfxId=0;
const std::uint16_t Delay=0;
const std::uint8_t Volume=0;
const std::uint8_t Pan=0;
const std::uint16_t Pitch=0;
const std::uint32_t Handle=0;
};
class SfxScriptEntry
{
public:
const std::int32_t SfxData;
const float Delay;
const float Volume;
const float Pan;
const float Pitch;
std::int32_t Handle=-1;
SfxScriptEntry(RawSfxScriptEntry raw,SfxScriptHeader header);
};
class SfxScriptFile
{
public:
const std::string Name;
const SfxScriptHeader Header;
const std::shared_ptr<const std::vector<std::shared_ptr<SfxScriptEntry>>> Entries;
SfxScriptFile(std::string name,SfxScriptHeader header,
std::shared_ptr<const std::vector<RawSfxScriptEntry>> entries);
};
struct DgnHeader
{
const std::uint32_t Offset=0;
const std::uint16_t Size=0;
const std::uint8_t InitialVolume=0;
const std::uint8_t SlotCount=0;
};
struct DgnEntry
{
const std::uint16_t Unused0=0;
const std::uint16_t SfxId=0;
const std::uint32_t Count1=0;
const std::uint32_t Offset1=0;
const std::uint32_t Count2=0;
const std::uint32_t Offset2=0;
const std::uint32_t Count3=0;
const std::uint32_t Offset3=0;
const std::uint32_t Count4=0;
const std::uint32_t Offset4=0;
};
struct DgnData
{
const std::uint16_t Amount=0;
const std::uint16_t Value=0;
};
class DgnFile
{
public:
const std::string Name;
const DgnHeader Header;
const std::shared_ptr<const std::vector<std::shared_ptr<class DgnFileEntry>>> Entries;
DgnFile(std::string name,DgnHeader header,
std::shared_ptr<const std::vector<std::shared_ptr<class DgnFileEntry>>> entries);
};
class DgnFileEntry
{
public:
const MphRead::SfxId SfxId;
const std::shared_ptr<const std::vector<DgnData>> Data1;
const std::shared_ptr<const std::vector<DgnData>> Data2;
const std::shared_ptr<const std::vector<DgnData>> Data3;
const std::shared_ptr<const std::vector<DgnData>> Data4;
DgnFileEntry(std::uint16_t sfxId,std::shared_ptr<const std::vector<DgnData>> data1,
std::shared_ptr<const std::vector<DgnData>> data2,
std::shared_ptr<const std::vector<DgnData>> data3,
std::shared_ptr<const std::vector<DgnData>> data4);
};
class WaveExportException:public MphRead::ProgramException
{
public:
explicit WaveExportException(const std::string&message);
};
class BinaryWriterExtensions final
{
public:
BinaryWriterExtensions()=delete;
static void WriteC(BinaryWriter&writer,const std::string&chars);
static void Write2(BinaryWriter&writer,std::int32_t value);
static void Write2(BinaryWriter&writer,std::uint32_t value);
static void Write2(BinaryWriter&writer,std::int16_t value);
static void Write2(BinaryWriter&writer,std::uint16_t value);
static void Write4(BinaryWriter&writer,std::int32_t value);
static void Write4(BinaryWriter&writer,std::uint32_t value);
static void Write4(BinaryWriter&writer,std::int16_t value);
static void Write4(BinaryWriter&writer,std::uint16_t value);
};
static_assert(sizeof(SoundNativeRuntime::Char4)==4);
static_assert(sizeof(SoundNativeRuntime::Bytes7)==7);
static_assert(sizeof(SoundNativeRuntime::Bytes16)==16);
static_assert(sizeof(SoundNativeRuntime::Bytes24)==24);
static_assert(sizeof(SoundNativeRuntime::Ushort3)==6);
static_assert(sizeof(SoundNativeRuntime::Ushort4)==8);
static_assert(std::is_standard_layout_v<SdatHeader>&&sizeof(SdatHeader)==64);
static_assert(offsetof(SdatHeader,Type)==0);
static_assert(offsetof(SdatHeader,Magic)==4);
static_assert(offsetof(SdatHeader,FileSize)==8);
static_assert(offsetof(SdatHeader,HeaderSize)==12);
static_assert(offsetof(SdatHeader,BlockCount)==14);
static_assert(offsetof(SdatHeader,SymbolBlockOffset)==16);
static_assert(offsetof(SdatHeader,SymbolBlockSize)==20);
static_assert(offsetof(SdatHeader,InfoBlockOffset)==24);
static_assert(offsetof(SdatHeader,InfoBlockSize)==28);
static_assert(offsetof(SdatHeader,FatOffset)==32);
static_assert(offsetof(SdatHeader,FatSize)==36);
static_assert(offsetof(SdatHeader,FileBlockOffset)==40);
static_assert(offsetof(SdatHeader,FileBlockSize)==44);
static_assert(offsetof(SdatHeader,Reserved)==48);
static_assert(std::is_standard_layout_v<BlockHeader>&&sizeof(BlockHeader)==64);
static_assert(offsetof(BlockHeader,Type)==0);
static_assert(offsetof(BlockHeader,HeaderSize)==4);
static_assert(offsetof(BlockHeader,SeqOffset)==8);
static_assert(offsetof(BlockHeader,SeqarcOffset)==12);
static_assert(offsetof(BlockHeader,BankOffset)==16);
static_assert(offsetof(BlockHeader,WavearcOffset)==20);
static_assert(offsetof(BlockHeader,PlayerOffset)==24);
static_assert(offsetof(BlockHeader,GroupOffset)==28);
static_assert(offsetof(BlockHeader,StrmPlayerOffset)==32);
static_assert(offsetof(BlockHeader,StrmOffset)==36);
static_assert(offsetof(BlockHeader,Reserved)==40);
static_assert(std::is_standard_layout_v<SdatFatHeader>&&sizeof(SdatFatHeader)==12);
static_assert(offsetof(SdatFatHeader,Type)==0);
static_assert(offsetof(SdatFatHeader,Size)==4);
static_assert(offsetof(SdatFatHeader,Count)==8);
static_assert(std::is_standard_layout_v<SdatFatEntry>&&sizeof(SdatFatEntry)==16);
static_assert(offsetof(SdatFatEntry,Offset)==0);
static_assert(offsetof(SdatFatEntry,Size)==4);
static_assert(offsetof(SdatFatEntry,Pointer)==8);
static_assert(offsetof(SdatFatEntry,Reserved)==12);
static_assert(std::is_standard_layout_v<SdatFileHeader>&&sizeof(SdatFileHeader)==16);
static_assert(offsetof(SdatFileHeader,Type)==0);
static_assert(offsetof(SdatFileHeader,Size)==4);
static_assert(offsetof(SdatFileHeader,Count)==8);
static_assert(offsetof(SdatFileHeader,Reserved)==12);
static_assert(std::is_standard_layout_v<SeqInfo>&&sizeof(SeqInfo)==12);
static_assert(offsetof(SeqInfo,FileId)==0);
static_assert(offsetof(SeqInfo,BankNo)==4);
static_assert(offsetof(SeqInfo,Volume)==6);
static_assert(offsetof(SeqInfo,ChannelPriority)==7);
static_assert(offsetof(SeqInfo,PlayerPriority)==8);
static_assert(offsetof(SeqInfo,PlayerNo)==9);
static_assert(offsetof(SeqInfo,Reserved)==10);
static_assert(std::is_standard_layout_v<BankInfo>&&sizeof(BankInfo)==12);
static_assert(offsetof(BankInfo,FileId)==0);
static_assert(offsetof(BankInfo,WaveArcNo)==4);
static_assert(std::is_standard_layout_v<PlayerInfo>&&sizeof(PlayerInfo)==8);
static_assert(offsetof(PlayerInfo,MaxSequences)==0);
static_assert(offsetof(PlayerInfo,Padding1)==1);
static_assert(offsetof(PlayerInfo,AllocateChannelBits)==2);
static_assert(offsetof(PlayerInfo,HeapSize)==4);
static_assert(std::is_standard_layout_v<GroupItemInfo>&&sizeof(GroupItemInfo)==8);
static_assert(offsetof(GroupItemInfo,Type)==0);
static_assert(offsetof(GroupItemInfo,LoadTypes)==1);
static_assert(offsetof(GroupItemInfo,Padding2)==2);
static_assert(offsetof(GroupItemInfo,LoadIndex)==4);
static_assert(std::is_standard_layout_v<StrmPlayerInfo>&&sizeof(StrmPlayerInfo)==24);
static_assert(offsetof(StrmPlayerInfo,ChannelCount)==0);
static_assert(offsetof(StrmPlayerInfo,ChannelNumbers)==1);
static_assert(offsetof(StrmPlayerInfo,Reserved)==17);
static_assert(std::is_standard_layout_v<StrmInfo>&&sizeof(StrmInfo)==8);
static_assert(offsetof(StrmInfo,FileId)==0);
static_assert(offsetof(StrmInfo,Volume)==4);
static_assert(offsetof(StrmInfo,PlayerPriority)==5);
static_assert(offsetof(StrmInfo,PlayerNo)==6);
static_assert(offsetof(StrmInfo,Flags)==7);
static_assert(std::is_standard_layout_v<SoundSampleHeader>&&sizeof(SoundSampleHeader)==12);
static_assert(offsetof(SoundSampleHeader,Format)==0);
static_assert(offsetof(SoundSampleHeader,LoopFlag)==1);
static_assert(offsetof(SoundSampleHeader,SampleRate)==2);
static_assert(offsetof(SoundSampleHeader,Timer)==4);
static_assert(offsetof(SoundSampleHeader,LoopStart)==6);
static_assert(offsetof(SoundSampleHeader,LoopLength)==8);
static_assert(std::is_standard_layout_v<SoundStreamHeader>&&sizeof(SoundStreamHeader)==64);
static_assert(sizeof(SoundStreamHeader)+32+8==104);
static_assert(offsetof(SoundStreamHeader,Type)==0);
static_assert(offsetof(SoundStreamHeader,Magic)==4);
static_assert(offsetof(SoundStreamHeader,DataSize)==8);
static_assert(offsetof(SoundStreamHeader,Size)==12);
static_assert(offsetof(SoundStreamHeader,DataBlocks)==14);
static_assert(offsetof(SoundStreamHeader,HeadType)==16);
static_assert(offsetof(SoundStreamHeader,HeaderSize)==20);
static_assert(offsetof(SoundStreamHeader,Format)==24);
static_assert(offsetof(SoundStreamHeader,LoopFlag)==25);
static_assert(offsetof(SoundStreamHeader,Channels)==26);
static_assert(offsetof(SoundStreamHeader,Padding25)==27);
static_assert(offsetof(SoundStreamHeader,SampleRate)==28);
static_assert(offsetof(SoundStreamHeader,Timer)==30);
static_assert(offsetof(SoundStreamHeader,LoopStart)==32);
static_assert(offsetof(SoundStreamHeader,LoopEnd)==36);
static_assert(offsetof(SoundStreamHeader,DataOffset)==40);
static_assert(offsetof(SoundStreamHeader,BlockCount)==44);
static_assert(offsetof(SoundStreamHeader,BlockSize)==48);
static_assert(offsetof(SoundStreamHeader,BlockSamples)==52);
static_assert(offsetof(SoundStreamHeader,LastBlockSize)==56);
static_assert(offsetof(SoundStreamHeader,LastBlockSamples)==60);
static_assert(std::is_standard_layout_v<RawSoundSelectEntry>&&sizeof(RawSoundSelectEntry)==16);
static_assert(offsetof(RawSoundSelectEntry,Id)==0);
static_assert(offsetof(RawSoundSelectEntry,Type)==2);
static_assert(offsetof(RawSoundSelectEntry,Field4)==4);
static_assert(offsetof(RawSoundSelectEntry,Field8)==8);
static_assert(offsetof(RawSoundSelectEntry,FieldC)==12);
static_assert(std::is_standard_layout_v<Sound3dEntry>&&sizeof(Sound3dEntry)==8);
static_assert(offsetof(Sound3dEntry,FalloffDistance)==0);
static_assert(offsetof(Sound3dEntry,MaxDistance)==4);
static_assert(std::is_standard_layout_v<RawSoundTableEntry>&&sizeof(RawSoundTableEntry)==12);
static_assert(offsetof(RawSoundTableEntry,Exists)==0);
static_assert(offsetof(RawSoundTableEntry,CategoryId)==2);
static_assert(offsetof(RawSoundTableEntry,SlotCount)==3);
static_assert(offsetof(RawSoundTableEntry,InitialVolume)==4);
static_assert(offsetof(RawSoundTableEntry,Priority)==5);
static_assert(offsetof(RawSoundTableEntry,Size)==6);
static_assert(offsetof(RawSoundTableEntry,Data)==8);
static_assert(std::is_standard_layout_v<RoomMusic>&&sizeof(RoomMusic)==8);
static_assert(offsetof(RoomMusic,RoomId)==0);
static_assert(offsetof(RoomMusic,TrackIds)==2);
static_assert(std::is_standard_layout_v<RawMusicTrack>&&sizeof(RawMusicTrack)==8);
static_assert(offsetof(RawMusicTrack,SeqId)==0);
static_assert(offsetof(RawMusicTrack,FadeOutFrames)==2);
static_assert(offsetof(RawMusicTrack,Tracks)==4);
static_assert(offsetof(RawMusicTrack,FadeInFrames)==6);
static_assert(std::is_standard_layout_v<SfxScriptHeader>&&sizeof(SfxScriptHeader)==8);
static_assert(offsetof(SfxScriptHeader,Offset)==0);
static_assert(offsetof(SfxScriptHeader,Size)==4);
static_assert(offsetof(SfxScriptHeader,InitialVolume)==6);
static_assert(offsetof(SfxScriptHeader,SlotCount)==7);
static_assert(std::is_standard_layout_v<RawSfxScriptEntry>&&sizeof(RawSfxScriptEntry)==12);
static_assert(offsetof(RawSfxScriptEntry,SfxId)==0);
static_assert(offsetof(RawSfxScriptEntry,Delay)==2);
static_assert(offsetof(RawSfxScriptEntry,Volume)==4);
static_assert(offsetof(RawSfxScriptEntry,Pan)==5);
static_assert(offsetof(RawSfxScriptEntry,Pitch)==6);
static_assert(offsetof(RawSfxScriptEntry,Handle)==8);
static_assert(std::is_standard_layout_v<DgnHeader>&&sizeof(DgnHeader)==8);
static_assert(offsetof(DgnHeader,Offset)==0);
static_assert(offsetof(DgnHeader,Size)==4);
static_assert(offsetof(DgnHeader,InitialVolume)==6);
static_assert(offsetof(DgnHeader,SlotCount)==7);
static_assert(std::is_standard_layout_v<DgnEntry>&&sizeof(DgnEntry)==36);
static_assert(offsetof(DgnEntry,Unused0)==0);
static_assert(offsetof(DgnEntry,SfxId)==2);
static_assert(offsetof(DgnEntry,Count1)==4);
static_assert(offsetof(DgnEntry,Offset1)==8);
static_assert(offsetof(DgnEntry,Count2)==12);
static_assert(offsetof(DgnEntry,Offset2)==16);
static_assert(offsetof(DgnEntry,Count3)==20);
static_assert(offsetof(DgnEntry,Offset3)==24);
static_assert(offsetof(DgnEntry,Count4)==28);
static_assert(offsetof(DgnEntry,Offset4)==32);
static_assert(std::is_standard_layout_v<DgnData>&&sizeof(DgnData)==4);
static_assert(offsetof(DgnData,Amount)==0);
static_assert(offsetof(DgnData,Value)==2);
}
