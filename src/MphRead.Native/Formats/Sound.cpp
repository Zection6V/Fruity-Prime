#include "Sound.hpp"
#define SoundRead FhSoundReadContribution
#include "FhSound.hpp"
#undef SoundRead
#include "../Metadata/Metadata.hpp"
#include "../Read.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cmath>
#include <csignal>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string_view>
namespace MphRead::Formats::Sound
{
namespace
{
template<typename T>
[[nodiscard]]T ReadLittle(std::span<const std::uint8_t> bytes,std::size_t offset)
{
static_assert(std::is_integral_v<T>);
using U=std::make_unsigned_t<T>;
if(offset> bytes.size()||sizeof(T)> bytes.size()-offset)
{
MphRead::ReadDetail::ThrowRange();
}
U value=0;
for(std::size_t i=0;i<sizeof(T);++i)
{
value|=static_cast<U>(bytes[offset+i])<<(i*8);
}
if constexpr(std::is_signed_v<T>)
{
return std::bit_cast<T>(value);
}
else
{
return value;
}
}
template<std::size_t N>
[[nodiscard]]SoundNativeRuntime::ByValArray<char16_t,N,std::uint8_t> ReadCharArray(
std::span<const std::uint8_t> bytes,std::size_t offset)
{
if(offset> bytes.size()||N> bytes.size()-offset)
{
MphRead::ReadDetail::ThrowRange();
}
std::array<std::uint8_t,N> values{};
std::copy_n(bytes.begin()+static_cast<std::ptrdiff_t>(offset),N,values.begin());
return SoundNativeRuntime::ByValArray<char16_t,N,std::uint8_t>(values);
}
template<std::size_t N>
[[nodiscard]]SoundNativeRuntime::ByValArray<std::uint8_t,N> ReadByteArray(
std::span<const std::uint8_t> bytes,std::size_t offset)
{
if(offset> bytes.size()||N> bytes.size()-offset)
{
MphRead::ReadDetail::ThrowRange();
}
std::array<std::uint8_t,N> values{};
std::copy_n(bytes.begin()+static_cast<std::ptrdiff_t>(offset),N,values.begin());
return SoundNativeRuntime::ByValArray<std::uint8_t,N>(values);
}
template<std::size_t N>
[[nodiscard]]SoundNativeRuntime::ByValArray<std::uint16_t,N> ReadUshortArray(
std::span<const std::uint8_t> bytes,std::size_t offset)
{
std::array<std::uint16_t,N> values{};
for(std::size_t i=0;i<N;++i)
{
values[i]=ReadLittle<std::uint16_t>(bytes,offset+i*2);
}
return SoundNativeRuntime::ByValArray<std::uint16_t,N>(values);
}
[[nodiscard]]std::span<const std::uint8_t> CheckedSlice(
std::span<const std::uint8_t> bytes,std::uint64_t start,std::uint64_t length)
{
if(start> bytes.size()||length> bytes.size()-static_cast<std::size_t>(start))
{
MphRead::ReadDetail::ThrowRange();
}
return bytes.subspan(static_cast<std::size_t>(start),static_cast<std::size_t>(length));
}
[[nodiscard]]std::span<const std::uint8_t> CheckedSlice(
std::span<const std::uint8_t> bytes,std::uint64_t start)
{
if(start> bytes.size())
{
MphRead::ReadDetail::ThrowRange();
}
return bytes.subspan(static_cast<std::size_t>(start));
}
[[nodiscard]]std::uint8_t CheckedByte(std::span<const std::uint8_t> bytes,std::size_t index)
{
if(index>=bytes.size())
{
MphRead::ReadDetail::ThrowRange();
}
return bytes[index];
}
[[nodiscard]]std::vector<std::uint8_t> FileReadAllBytes(const std::string&path)
{
std::ifstream stream(path,std::ios::binary|std::ios::ate);
if(!stream)
{
throw std::ios_base::failure("Could not open file: "+path);
}
const std::streampos end=stream.tellg();
if(end<0)
{
throw std::ios_base::failure("Could not determine file length: "+path);
}
std::vector<std::uint8_t> bytes(static_cast<std::size_t>(end));
stream.seekg(0,std::ios::beg);
if(!bytes.empty())
{
stream.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
if(!stream)
{
throw std::ios_base::failure("Could not read file: "+path);
}
}
return bytes;
}
[[nodiscard]]std::string PadId(std::uint32_t id)
{
std::string value=std::to_string(id);
if(value.size()<3)
{
value.insert(value.begin(),3-value.size(),'0');
}
return value;
}
[[nodiscard]]std::string PadId(std::int32_t id)
{
std::string value=std::to_string(id);
if(value.size()<3)
{
value.insert(value.begin(),3-value.size(),'0');
}
return value;
}
[[nodiscard]]std::string WaveFormatString(WaveFormat format)
{
switch(format)
{
case WaveFormat::None:return"None";
case WaveFormat::PCM8:return"PCM8";
case WaveFormat::PCM16:return"PCM16";
case WaveFormat::ADPCM:return"ADPCM";
default:return std::to_string(static_cast<std::int32_t>(format));
}
}
[[nodiscard]]std::uint32_t GetSampleCountValue(
WaveFormat format,std::uint32_t sampleStart,std::uint32_t sampleLength)noexcept
{
std::uint32_t loopStart;
std::uint32_t loopLength;
if(format==WaveFormat::ADPCM)
{
loopStart=(sampleStart*4U-4U)*2U;
loopLength=sampleLength*8U;
}
else
{
loopStart=sampleStart;
loopLength=sampleLength;
}
return loopStart+loopLength;
}
void DecodeWaveData(std::span<const std::uint8_t> data,WaveFormat format,
std::uint32_t sampleCount,bool adpcmRoundingError,BinaryWriter&writer)
{
if(format==WaveFormat::ADPCM)
{
std::int32_t transferred=0;
bool low=true;
std::int32_t sampleValue=ReadLittle<std::int16_t>(data,0);
std::int32_t stepIndex=ReadLittle<std::int16_t>(data,2);
transferred+=4;
for(std::uint32_t i=0;i<sampleCount;++i)
{
std::uint8_t value=CheckedByte(data,static_cast<std::size_t>(transferred));
if(!low)
{
value>>=4;
++transferred;
}
value&=0x0F;
if(stepIndex<0||stepIndex>=static_cast<std::int32_t>(MphRead::Metadata::AdpcmTable.size()))
{
throw std::out_of_range("Index was outside the bounds of the array.");
}
const std::int32_t step=MphRead::Metadata::AdpcmTable[static_cast<std::size_t>(stepIndex)];
std::int32_t diff=step>> 3;
if((value&1U)!=0)
{
diff+=step>> 2;
}
if((value&2U)!=0)
{
diff+=step>> 1;
}
if((value&4U)!=0)
{
diff+=step;
}
if(adpcmRoundingError)
{
if((value&8U)!=0)
{
sampleValue-=diff;
if(sampleValue<-32767)
{
sampleValue=-32767;
}
}
else
{
sampleValue+=diff;
if(sampleValue> 32767)
{
sampleValue=32767;
}
}
}
else
{
if((value&8U)!=0)
{
sampleValue-=diff;
}
else
{
sampleValue+=diff;
}
if(sampleValue<-32768)
{
sampleValue=-32768;
}
if(sampleValue> 32767)
{
sampleValue=32767;
}
}
stepIndex+=MphRead::Metadata::ImaIndexTable[value];
if(stepIndex<0)
{
stepIndex=0;
}
else if(stepIndex> 88)
{
stepIndex=88;
}
BinaryWriterExtensions::Write2(writer,sampleValue);
low=!low;
}
}
else
{
#ifndef NDEBUG
assert(format==WaveFormat::PCM8
||(format==WaveFormat::None&&sampleCount==0&&data.empty()));
#endif
for(std::uint32_t i=0;i<sampleCount;++i)
{
writer.Write(static_cast<std::uint8_t>(CheckedByte(data,i)^0x80U));
}
}
}
[[nodiscard]]std::shared_ptr<std::vector<std::uint8_t>> DecodeWaveDataVector(
std::span<const std::uint8_t> data,WaveFormat format,
std::uint32_t sampleCount,bool adpcmRoundingError)
{
std::ostringstream stream(std::ios::out|std::ios::binary);
BinaryWriter writer(stream);
DecodeWaveData(data,format,sampleCount,adpcmRoundingError,writer);
const std::string value=stream.str();
return std::make_shared<std::vector<std::uint8_t>>(value.begin(),value.end());
}
[[nodiscard]]std::int32_t ManagedAdd(std::int32_t left,std::int32_t right)noexcept;
[[nodiscard]]std::int32_t ManagedMultiply(std::int32_t left,std::int32_t right)noexcept;
[[nodiscard]]std::shared_ptr<std::vector<std::uint8_t>> InterleaveStreamChannels(
const std::shared_ptr<const std::vector<std::shared_ptr<std::vector<std::uint8_t>>>>&channels,
WaveFormat format)
{
const std::int32_t bytesPerSample=format==WaveFormat::ADPCM?2:1;
const auto&first=channels->at(0);
if(first->size()> static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())
||channels->size()> static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
{
throw std::overflow_error("Array dimensions exceeded supported range.");
}
const std::int32_t firstLength=static_cast<std::int32_t>(first->size());
const std::int32_t channelCount=static_cast<std::int32_t>(channels->size());
const std::int32_t length=ManagedMultiply(firstLength,channelCount);
if(length<0)
{
throw std::overflow_error("Array dimensions exceeded supported range.");
}
auto data=std::make_shared<std::vector<std::uint8_t>>(static_cast<std::size_t>(length));
for(std::int32_t i=0;i<channelCount;++i)
{
const auto&channel=channels->at(static_cast<std::size_t>(i));
if(channel->size()> static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
{
throw std::overflow_error("Array dimensions exceeded supported range.");
}
const std::int32_t channelLength=static_cast<std::int32_t>(channel->size());
std::int32_t destIndex=ManagedMultiply(i,bytesPerSample);
std::int32_t srcIndex=0;
for(std::int32_t j=0;j<channelLength;j=ManagedAdd(j,bytesPerSample))
{
for(std::int32_t k=0;k<bytesPerSample;++k)
{
if(srcIndex<0||srcIndex>=channelLength
||destIndex<0||destIndex>=length)
{
MphRead::ReadDetail::ThrowRange();
}
(*data)[static_cast<std::size_t>(destIndex)]
=(*channel)[static_cast<std::size_t>(srcIndex)];
destIndex=ManagedAdd(destIndex,1);
srcIndex=ManagedAdd(srcIndex,1);
}
destIndex=ManagedAdd(destIndex,
ManagedMultiply(bytesPerSample,ManagedAdd(channelCount,-1)));
}
}
return data;
}
[[nodiscard]]float CalculatePitchDiv(float pitchFac)
{
if(pitchFac==0.0F)
{
pitchFac=1.0F;
}
std::int32_t pitchInt=static_cast<std::int32_t>(pitchFac);
if(pitchFac<=0xFFF)
{
pitchInt=-((0x600000/pitchInt)>> 1);
}
else if(pitchFac<=0x1FFF)
{
pitchInt=(768*(pitchInt-0x2000))>> 12;
}
else
{
pitchInt=(768*(pitchInt-0x2000))>> 13;
}
const float semitones=pitchInt/64.0F;
const float octaves=std::fabs(semitones/12.0F);
if(semitones>=0.0F)
{
return std::pow(2.0F,octaves);
}
return std::pow(0.5F,octaves);
}
[[nodiscard]]WaveFormat ValidateMphFormat(std::uint8_t format)
{
if(format> 2)
{
throw MphRead::ProgramException("Invalid wave format "+std::to_string(format)+".");
}
return static_cast<WaveFormat>(format);
}
[[nodiscard]]WaveFormat ValidateFhFormat(std::uint8_t format)
{
if(format==4)
{
return WaveFormat::ADPCM;
}
if(format==0)
{
return WaveFormat::PCM8;
}
throw MphRead::ProgramException("Unexpected FH sound header value: "+std::to_string(format));
}
[[nodiscard]]std::uint32_t FhSampleLength(
FhSoundSampleHeader header,std::size_t dataLength)
{
if(header.Format==4)
{
if(header.LoopEnd*4U> dataLength)
{
return header.LoopEnd-header.LoopStart-1U;
}
return header.LoopEnd-header.LoopStart;
}
if(header.Format==0)
{
return header.LoopEnd-header.LoopStart;
}
return 0;
}
[[nodiscard]]std::int32_t ManagedInt32(std::uint32_t value)noexcept
{
return std::bit_cast<std::int32_t>(value);
}
[[nodiscard]]std::int32_t ManagedAdd(std::int32_t left,std::int32_t right)noexcept
{
return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(left)
+static_cast<std::uint32_t>(right));
}
[[nodiscard]]std::int32_t ManagedMultiply(std::int32_t left,std::int32_t right)noexcept
{
return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(left)
*static_cast<std::uint32_t>(right));
}
[[nodiscard]]std::span<const std::uint8_t> ManagedSliceFromUint(
std::span<const std::uint8_t> bytes,std::uint32_t start)
{
return MphRead::ReadDetail::Slice(bytes,ManagedInt32(start));
}
[[nodiscard]]std::span<const std::uint8_t> ManagedRangeSlice(
std::span<const std::uint8_t> bytes,std::uint32_t start,std::uint32_t end)
{
const std::int32_t managedStart=ManagedInt32(start);
const std::int32_t managedEnd=ManagedInt32(end);
if(managedStart<0||managedEnd<0||managedEnd<managedStart)
{
MphRead::ReadDetail::ThrowRange();
}
return MphRead::ReadDetail::Slice(bytes,managedStart,managedEnd-managedStart);
}
[[nodiscard]]std::int32_t CheckedStringLengthSum(const std::vector<std::string>&values)
{
std::int32_t sum=0;
for(const std::string&value:values)
{
if(value.size()> static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
{
throw std::overflow_error("Arithmetic operation resulted in an overflow.");
}
const std::int32_t length=static_cast<std::int32_t>(value.size());
if(length> 0&&sum> std::numeric_limits<std::int32_t>::max()-length)
{
throw std::overflow_error("Arithmetic operation resulted in an overflow.");
}
sum+=length;
}
return sum;
}
void DebugBreak()
{
#if defined(_MSC_VER)
__debugbreak();
#elif defined(SIGTRAP)
std::raise(SIGTRAP);
#else
std::abort();
#endif
}
class FixedVectorStreamBuf final:public std::streambuf
{
public:
explicit FixedVectorStreamBuf(std::vector<std::uint8_t>&buffer)
{
char*first=reinterpret_cast<char*>(buffer.data());
setp(first,first+static_cast<std::ptrdiff_t>(buffer.size()));
}
protected:
int_type overflow(int_type)override
{
return traits_type::eof();
}
};
}
SoundNativeRuntime::LazyByteArray::LazyByteArray(Factory factory)
:_factory(std::move(factory))
{
}
SoundNativeRuntime::LazyByteArray::ValueType SoundNativeRuntime::LazyByteArray::Value()const
{
std::lock_guard<std::mutex> lock(_mutex);
if(!_evaluated)
{
try
{
_value=_factory();
}
catch(...)
{
_exception=std::current_exception();
}
_factory={};
_evaluated=true;
}
if(_exception)
{
std::rethrow_exception(_exception);
}
return _value;
}
bool SoundNativeRuntime::LazyByteArray::IsValueCreated()const noexcept
{
try
{
std::lock_guard<std::mutex> lock(_mutex);
return _evaluated&&!_exception;
}
catch(...)
{
return false;
}
}
BinaryWriter::BinaryWriter(std::ostream&stream)noexcept
:_stream(&stream)
{
}
void BinaryWriter::WriteRaw(const void*bytes,std::size_t count)
{
_stream->write(static_cast<const char*>(bytes),static_cast<std::streamsize>(count));
if(!*_stream)
{
throw std::ios_base::failure("Write fault on path.");
}
}
void BinaryWriter::Write(std::uint8_t value)
{
WriteRaw(&value,1);
}
void BinaryWriter::Write(std::int16_t value)
{
Write(static_cast<std::uint16_t>(value));
}
void BinaryWriter::Write(std::uint16_t value)
{
std::array<std::uint8_t,2> bytes{
static_cast<std::uint8_t>(value),
static_cast<std::uint8_t>(value>> 8)};
WriteRaw(bytes.data(),bytes.size());
}
void BinaryWriter::Write(std::int32_t value)
{
Write(std::bit_cast<std::uint32_t>(value));
}
void BinaryWriter::Write(std::uint32_t value)
{
std::array<std::uint8_t,4> bytes{
static_cast<std::uint8_t>(value),
static_cast<std::uint8_t>(value>> 8),
static_cast<std::uint8_t>(value>> 16),
static_cast<std::uint8_t>(value>> 24)};
WriteRaw(bytes.data(),bytes.size());
}
void BinaryWriter::WriteUtf8Scalar(std::uint32_t scalar)
{
if(scalar<=0x7F)
{
Write(static_cast<std::uint8_t>(scalar));
}
else if(scalar<=0x7FF)
{
Write(static_cast<std::uint8_t>(0xC0|(scalar>> 6)));
Write(static_cast<std::uint8_t>(0x80|(scalar&0x3F)));
}
else if(scalar<=0xFFFF)
{
Write(static_cast<std::uint8_t>(0xE0|(scalar>> 12)));
Write(static_cast<std::uint8_t>(0x80|((scalar>> 6)&0x3F)));
Write(static_cast<std::uint8_t>(0x80|(scalar&0x3F)));
}
else
{
Write(static_cast<std::uint8_t>(0xF0|(scalar>> 18)));
Write(static_cast<std::uint8_t>(0x80|((scalar>> 12)&0x3F)));
Write(static_cast<std::uint8_t>(0x80|((scalar>> 6)&0x3F)));
Write(static_cast<std::uint8_t>(0x80|(scalar&0x3F)));
}
}
void BinaryWriter::Write(char16_t value)
{
if(_pendingHighSurrogate)
{
if(value>=0xDC00&&value<=0xDFFF)
{
const std::uint32_t high=static_cast<std::uint32_t>(*_pendingHighSurrogate-0xD800);
const std::uint32_t low=static_cast<std::uint32_t>(value-0xDC00);
_pendingHighSurrogate.reset();
WriteUtf8Scalar(0x10000U+(high<<10)+low);
return;
}
_pendingHighSurrogate.reset();
throw std::runtime_error("Unable to translate Unicode character to specified code page.");
}
if(value>=0xD800&&value<=0xDBFF)
{
_pendingHighSurrogate=value;
return;
}
if(value>=0xDC00&&value<=0xDFFF)
{
throw std::runtime_error("Unable to translate Unicode character to specified code page.");
}
WriteUtf8Scalar(value);
}
void BinaryWriter::Write(std::span<const std::uint8_t> bytes)
{
if(!bytes.empty())
{
WriteRaw(bytes.data(),bytes.size());
}
}
void BinaryWriterExtensions::WriteC(BinaryWriter&writer,const std::string&chars)
{
for(const unsigned char character:chars)
{
writer.Write(static_cast<std::uint8_t>(character));
}
}
void BinaryWriterExtensions::Write2(BinaryWriter&writer,std::int32_t value)
{
writer.Write(static_cast<std::uint16_t>(static_cast<std::uint32_t>(value)));
}
void BinaryWriterExtensions::Write2(BinaryWriter&writer,std::uint32_t value)
{
writer.Write(static_cast<std::uint16_t>(value));
}
void BinaryWriterExtensions::Write2(BinaryWriter&writer,std::int16_t value)
{
writer.Write(value);
}
void BinaryWriterExtensions::Write2(BinaryWriter&writer,std::uint16_t value)
{
writer.Write(value);
}
void BinaryWriterExtensions::Write4(BinaryWriter&writer,std::int32_t value)
{
writer.Write(value);
}
void BinaryWriterExtensions::Write4(BinaryWriter&writer,std::uint32_t value)
{
writer.Write(value);
}
void BinaryWriterExtensions::Write4(BinaryWriter&writer,std::int16_t value)
{
writer.Write(static_cast<std::uint32_t>(static_cast<std::int32_t>(value)));
}
void BinaryWriterExtensions::Write4(BinaryWriter&writer,std::uint16_t value)
{
writer.Write(static_cast<std::uint32_t>(value));
}
SdatHeader::SdatHeader(SoundNativeRuntime::Char4 type,std::uint32_t magic,
std::uint32_t fileSize,std::uint16_t headerSize,std::uint16_t blockCount,
std::uint32_t symbolBlockOffset,std::uint32_t symbolBlockSize,
std::uint32_t infoBlockOffset,std::uint32_t infoBlockSize,
std::uint32_t fatOffset,std::uint32_t fatSize,std::uint32_t fileBlockOffset,
std::uint32_t fileBlockSize,SoundNativeRuntime::Bytes16 reserved)
:Type(std::move(type)),Magic(magic),FileSize(fileSize),HeaderSize(headerSize),
BlockCount(blockCount),SymbolBlockOffset(symbolBlockOffset),
SymbolBlockSize(symbolBlockSize),InfoBlockOffset(infoBlockOffset),
InfoBlockSize(infoBlockSize),FatOffset(fatOffset),FatSize(fatSize),
FileBlockOffset(fileBlockOffset),FileBlockSize(fileBlockSize),Reserved(std::move(reserved))
{
}
SdatHeader SdatHeader::FromMarshaledBytes(const std::array<std::uint8_t,64>&bytes)
{
const std::span<const std::uint8_t> data(bytes);
return SdatHeader(ReadCharArray<4>(data,0),ReadLittle<std::uint32_t>(data,4),
ReadLittle<std::uint32_t>(data,8),ReadLittle<std::uint16_t>(data,12),
ReadLittle<std::uint16_t>(data,14),ReadLittle<std::uint32_t>(data,16),
ReadLittle<std::uint32_t>(data,20),ReadLittle<std::uint32_t>(data,24),
ReadLittle<std::uint32_t>(data,28),ReadLittle<std::uint32_t>(data,32),
ReadLittle<std::uint32_t>(data,36),ReadLittle<std::uint32_t>(data,40),
ReadLittle<std::uint32_t>(data,44),ReadByteArray<16>(data,48));
}
BlockHeader::BlockHeader(SoundNativeRuntime::Char4 type,std::uint32_t headerSize,
std::uint32_t seqOffset,std::uint32_t seqarcOffset,std::uint32_t bankOffset,
std::uint32_t wavearcOffset,std::uint32_t playerOffset,std::uint32_t groupOffset,
std::uint32_t strmPlayerOffset,std::uint32_t strmOffset,
SoundNativeRuntime::Bytes24 reserved)
:Type(std::move(type)),HeaderSize(headerSize),SeqOffset(seqOffset),
SeqarcOffset(seqarcOffset),BankOffset(bankOffset),WavearcOffset(wavearcOffset),
PlayerOffset(playerOffset),GroupOffset(groupOffset),StrmPlayerOffset(strmPlayerOffset),
StrmOffset(strmOffset),Reserved(std::move(reserved))
{
}
BlockHeader BlockHeader::FromMarshaledBytes(const std::array<std::uint8_t,64>&bytes)
{
const std::span<const std::uint8_t> data(bytes);
return BlockHeader(ReadCharArray<4>(data,0),ReadLittle<std::uint32_t>(data,4),
ReadLittle<std::uint32_t>(data,8),ReadLittle<std::uint32_t>(data,12),
ReadLittle<std::uint32_t>(data,16),ReadLittle<std::uint32_t>(data,20),
ReadLittle<std::uint32_t>(data,24),ReadLittle<std::uint32_t>(data,28),
ReadLittle<std::uint32_t>(data,32),ReadLittle<std::uint32_t>(data,36),
ReadByteArray<24>(data,40));
}
SdatFatHeader::SdatFatHeader(
SoundNativeRuntime::Char4 type,std::uint32_t size,std::uint32_t count)
:Type(std::move(type)),Size(size),Count(count)
{
}
SdatFatHeader SdatFatHeader::FromMarshaledBytes(const std::array<std::uint8_t,12>&bytes)
{
const std::span<const std::uint8_t> data(bytes);
return SdatFatHeader(ReadCharArray<4>(data,0),ReadLittle<std::uint32_t>(data,4),
ReadLittle<std::uint32_t>(data,8));
}
SdatFileHeader::SdatFileHeader(SoundNativeRuntime::Char4 type,std::uint32_t size,
std::uint32_t count,std::uint32_t reserved)
:Type(std::move(type)),Size(size),Count(count),Reserved(reserved)
{
}
SdatFileHeader SdatFileHeader::FromMarshaledBytes(const std::array<std::uint8_t,16>&bytes)
{
const std::span<const std::uint8_t> data(bytes);
return SdatFileHeader(ReadCharArray<4>(data,0),ReadLittle<std::uint32_t>(data,4),
ReadLittle<std::uint32_t>(data,8),ReadLittle<std::uint32_t>(data,12));
}
BankInfo::BankInfo(std::uint32_t fileId,SoundNativeRuntime::Ushort4 waveArcNo)
:FileId(fileId),WaveArcNo(std::move(waveArcNo))
{
}
BankInfo BankInfo::FromMarshaledBytes(const std::array<std::uint8_t,12>&bytes)
{
const std::span<const std::uint8_t> data(bytes);
return BankInfo(ReadLittle<std::uint32_t>(data,0),ReadUshortArray<4>(data,4));
}
StrmPlayerInfo::StrmPlayerInfo(std::uint8_t channelCount,
SoundNativeRuntime::Bytes16 channelNumbers,SoundNativeRuntime::Bytes7 reserved)
:ChannelCount(channelCount),ChannelNumbers(std::move(channelNumbers)),Reserved(std::move(reserved))
{
}
StrmPlayerInfo StrmPlayerInfo::FromMarshaledBytes(const std::array<std::uint8_t,24>&bytes)
{
const std::span<const std::uint8_t> data(bytes);
return StrmPlayerInfo(data[0],ReadByteArray<16>(data,1),ReadByteArray<7>(data,17));
}
SoundStreamHeader::SoundStreamHeader(SoundNativeRuntime::Char4 type,std::uint32_t magic,
std::uint32_t dataSize,std::uint16_t size,std::uint16_t dataBlocks,
SoundNativeRuntime::Char4 headType,std::uint32_t headerSize,std::uint8_t format,
std::uint8_t loopFlag,std::uint8_t channels,std::uint8_t padding25,
std::uint16_t sampleRate,std::uint16_t timer,std::uint32_t loopStart,
std::uint32_t loopEnd,std::uint32_t dataOffset,std::uint32_t blockCount,
std::uint32_t blockSize,std::uint32_t blockSamples,std::uint32_t lastBlockSize,
std::uint32_t lastBlockSamples)
:Type(std::move(type)),Magic(magic),DataSize(dataSize),Size(size),DataBlocks(dataBlocks),
HeadType(std::move(headType)),HeaderSize(headerSize),Format(format),LoopFlag(loopFlag),
Channels(channels),Padding25(padding25),SampleRate(sampleRate),Timer(timer),
LoopStart(loopStart),LoopEnd(loopEnd),DataOffset(dataOffset),BlockCount(blockCount),
BlockSize(blockSize),BlockSamples(blockSamples),LastBlockSize(lastBlockSize),
LastBlockSamples(lastBlockSamples)
{
}
SoundStreamHeader SoundStreamHeader::FromMarshaledBytes(const std::array<std::uint8_t,64>&bytes)
{
const std::span<const std::uint8_t> data(bytes);
return SoundStreamHeader(ReadCharArray<4>(data,0),ReadLittle<std::uint32_t>(data,4),
ReadLittle<std::uint32_t>(data,8),ReadLittle<std::uint16_t>(data,12),
ReadLittle<std::uint16_t>(data,14),ReadCharArray<4>(data,16),
ReadLittle<std::uint32_t>(data,20),data[24],data[25],data[26],data[27],
ReadLittle<std::uint16_t>(data,28),ReadLittle<std::uint16_t>(data,30),
ReadLittle<std::uint32_t>(data,32),ReadLittle<std::uint32_t>(data,36),
ReadLittle<std::uint32_t>(data,40),ReadLittle<std::uint32_t>(data,44),
ReadLittle<std::uint32_t>(data,48),ReadLittle<std::uint32_t>(data,52),
ReadLittle<std::uint32_t>(data,56),ReadLittle<std::uint32_t>(data,60));
}
RoomMusic::RoomMusic(std::uint16_t roomId,SoundNativeRuntime::Ushort3 trackIds)
:RoomId(roomId),TrackIds(std::move(trackIds))
{
}
RoomMusic RoomMusic::FromMarshaledBytes(const std::array<std::uint8_t,8>&bytes)
{
const std::span<const std::uint8_t> data(bytes);
return RoomMusic(ReadLittle<std::uint16_t>(data,0),ReadUshortArray<3>(data,2));
}
SoundData::SoundData(std::shared_ptr<const std::vector<std::shared_ptr<SoundStream>>> streams)
:Streams(std::move(streams))
{
}
SoundStream::SoundStream(std::int32_t id,std::string name,SoundStreamHeader header,
std::shared_ptr<const std::vector<std::shared_ptr<std::vector<std::uint8_t>>>> channels,
float volume)
:Id(id),Name(std::move(name)),Format(static_cast<WaveFormat>(header.Format)),
Loop(header.LoopFlag!=0),SampleRate(header.SampleRate),LoopStart(header.LoopStart),
LoopEnd(header.LoopEnd),_channels(std::move(channels)),Channels(_channels),Volume(volume),
BufferData(std::make_shared<SoundNativeRuntime::LazyByteArray>(
[channelsRef=_channels,format=Format]()
{
return InterleaveStreamChannels(channelsRef,format);
}))
{
}
SoundSample::SoundSample(std::uint32_t id,std::uint32_t offset,SoundSampleHeader header,
std::span<const std::uint8_t> data)
:Id(id),Offset(offset),Format(ValidateMphFormat(header.Format)),
Loop(header.LoopFlag!=0),SampleRate(header.SampleRate),SampleStart(header.LoopStart),
SampleLength(header.LoopLength),
LoopStart(Format==WaveFormat::ADPCM
?ManagedInt32((SampleStart*4U-4U)*2U):0),
LoopLength(Format==WaveFormat::ADPCM?ManagedInt32(SampleLength*8U):0),
_data(std::make_shared<std::vector<std::uint8_t>>(data.begin(),data.end())),Data(_data),
WaveData(std::make_shared<SoundNativeRuntime::LazyByteArray>(
[dataRef=_data,format=Format,sampleStart=SampleStart,sampleLength=SampleLength]()
{
return DecodeWaveDataVector(*dataRef,format,
GetSampleCountValue(format,sampleStart,sampleLength),false);
}))
{
}
SoundSample::SoundSample(std::uint32_t id,std::uint32_t offset,FhSoundSampleHeader header,
std::span<const std::uint8_t> data)
:Id(id),Offset(offset),Format(ValidateFhFormat(header.Format)),
Loop(header.Format==4?header.LoopStart> 1U:header.LoopStart> 0U),
SampleRate(static_cast<std::uint16_t>(header.SampleRate)),SampleStart(header.LoopStart),
SampleLength(FhSampleLength(header,data.size())),
LoopStart(Format==WaveFormat::ADPCM
?ManagedInt32((SampleStart*4U-4U)*2U):ManagedInt32(SampleStart)),
LoopLength(Format==WaveFormat::ADPCM
?ManagedInt32(SampleLength*8U):ManagedInt32(SampleLength)),
_data(std::make_shared<std::vector<std::uint8_t>>(data.begin(),data.end())),Data(_data),
WaveData(std::make_shared<SoundNativeRuntime::LazyByteArray>(
[dataRef=_data,format=Format,sampleStart=SampleStart,sampleLength=SampleLength]()
{
return DecodeWaveDataVector(*dataRef,format,
GetSampleCountValue(format,sampleStart,sampleLength),false);
}))
{
#ifndef NDEBUG
if(Format!=WaveFormat::ADPCM)
{
assert(Format==WaveFormat::PCM8);
}
#endif
}
SoundSample::SoundSample(std::uint32_t id,NullTag)
:Id(id),Offset(0),Format(WaveFormat::None),Loop(false),SampleRate(0),
SampleStart(0),SampleLength(0),LoopStart(0),LoopLength(0),
_data(std::make_shared<std::vector<std::uint8_t>>()),Data(_data),
WaveData(std::make_shared<SoundNativeRuntime::LazyByteArray>(
[dataRef=_data](){return dataRef;}))
{
}
std::shared_ptr<SoundSample> SoundSample::CreateNull(std::uint32_t id)
{
return std::shared_ptr<SoundSample>(new SoundSample(id,NullTag{}));
}
std::span<const std::uint8_t> SoundSample::CreateSpan()const noexcept
{
return std::span<const std::uint8_t>(*_data);
}
std::span<const std::uint8_t> SoundSample::GetIntro()const
{
if(LoopStart==0)
{
return{};
}
const auto data=WaveData->Value();
const std::int32_t factor=Format==WaveFormat::ADPCM?2:1;
const std::int32_t length=ManagedMultiply(LoopStart,factor);
if(length<0)
{
MphRead::ReadDetail::ThrowRange();
}
return CheckedSlice(*data,0,static_cast<std::uint64_t>(length));
}
std::span<const std::uint8_t> SoundSample::GetLoop()const
{
const auto data=WaveData->Value();
const std::int32_t factor=Format==WaveFormat::ADPCM?2:1;
const std::int32_t start=ManagedMultiply(LoopStart,factor);
const std::int32_t length=ManagedMultiply(LoopLength,factor);
if(start<0||length<0)
{
MphRead::ReadDetail::ThrowRange();
}
return CheckedSlice(*data,static_cast<std::uint64_t>(start),static_cast<std::uint64_t>(length));
}
std::span<const std::uint8_t> SoundSample::GetOutro()const
{
const auto data=WaveData->Value();
const std::int32_t factor=Format==WaveFormat::ADPCM?2:1;
const std::int32_t start=ManagedMultiply(ManagedAdd(LoopStart,LoopLength),factor);
if(static_cast<std::int64_t>(start)>=static_cast<std::int64_t>(data->size()))
{
return{};
}
if(start<0)
{
MphRead::ReadDetail::ThrowRange();
}
return CheckedSlice(*data,static_cast<std::uint64_t>(start));
}
SoundSelectEntry::SoundSelectEntry(std::string name,RawSoundSelectEntry raw)
:Name(std::move(name)),Id(raw.Id),Type(raw.Type),Field4(raw.Field4),
Field8(raw.Field8),FieldC(raw.FieldC)
{
}
SoundTable::SoundTable(
std::shared_ptr<const std::vector<std::shared_ptr<SoundTableEntry>>> entries,
std::shared_ptr<const std::vector<std::string>> categories)
:Entries(std::move(entries)),Categories(std::move(categories))
{
}
SoundTableEntry::SoundTableEntry(std::string name,std::string category,RawSoundTableEntry raw)
:Name(std::move(name)),Category(std::move(category)),CategoryId(raw.CategoryId),
SlotCount(raw.SlotCount),InitialVolume(raw.InitialVolume),Priority(raw.Priority),
Size(raw.Size),Data(raw.Data)
{
}
MusicTrack::MusicTrack(std::uint32_t id,RawMusicTrack raw)
:MusicId(static_cast<MphRead::MusicId>(id)),
SeqId(raw.SeqId==std::numeric_limits<std::uint16_t>::max()
?MphRead::SeqId::None:static_cast<MphRead::SeqId>(raw.SeqId)),
Tracks(raw.Tracks),FadeOutFrames(raw.FadeOutFrames),FadeInFrames(raw.FadeInFrames)
{
}
SfxScriptEntry::SfxScriptEntry(RawSfxScriptEntry raw,SfxScriptHeader header)
:SfxData(raw.SfxId),Delay(raw.Delay/30.0F),
Volume(raw.Volume/127.0F*header.InitialVolume/127.0F),
Pan([raw]()
{
if(raw.Pan==255)
{
return-1.0F;
}
std::int32_t rawPan=raw.Pan;
if(rawPan==127)
{
rawPan=128;
}
return(rawPan-64)/64.0F/2.0F;
}()),Pitch(CalculatePitchDiv(raw.Pitch))
{
}
SfxScriptFile::SfxScriptFile(std::string name,SfxScriptHeader header,
std::shared_ptr<const std::vector<RawSfxScriptEntry>> entries)
:Name(std::move(name)),Header(header),
Entries([entries,header]()
{
auto result=std::make_shared<std::vector<std::shared_ptr<SfxScriptEntry>>>();
result->reserve(entries->size());
for(const RawSfxScriptEntry entry:*entries)
{
result->push_back(std::make_shared<SfxScriptEntry>(entry,header));
}
return std::shared_ptr<const std::vector<std::shared_ptr<SfxScriptEntry>>>(result);
}())
{
}
DgnFile::DgnFile(std::string name,DgnHeader header,
std::shared_ptr<const std::vector<std::shared_ptr<DgnFileEntry>>> entries)
:Name(std::move(name)),Header(header),Entries(std::move(entries))
{
}
DgnFileEntry::DgnFileEntry(std::uint16_t sfxId,
std::shared_ptr<const std::vector<DgnData>> data1,
std::shared_ptr<const std::vector<DgnData>> data2,
std::shared_ptr<const std::vector<DgnData>> data3,
std::shared_ptr<const std::vector<DgnData>> data4)
:SfxId(static_cast<MphRead::SfxId>(sfxId)),Data1(std::move(data1)),Data2(std::move(data2)),
Data3(std::move(data3)),Data4(std::move(data4))
{
}
WaveExportException::WaveExportException(const std::string&message)
:MphRead::ProgramException(message)
{
}
void SoundRead::ExportSamples(bool adpcmRoundingError)
{
ExportSamples(ReadSoundSamples(),adpcmRoundingError,std::string("mph_"));
}
void SoundRead::ExportWfsSamples(bool adpcmRoundingError)
{
ExportSamples(ReadWfsSoundSamples(),adpcmRoundingError,std::string("mph_wfs_"));
}
void SoundRead::ExportSamples(
const std::shared_ptr<const std::vector<std::shared_ptr<SoundSample>>>&samples,
bool adpcmRoundingError,const std::optional<std::string>&prefix)
{
for(const std::shared_ptr<SoundSample>&sample:*samples)
{
try
{
ExportSample(sample,adpcmRoundingError,prefix);
}
catch(const WaveExportException&ex)
{
std::cout<<"["<<sample->Id<<"] WaveExportException: "<<ex.what()<<std::endl;
}
}
}
void SoundRead::ExportSample(std::int32_t id,bool adpcmRoundingError)
{
const auto samples=ReadSoundSamples();
if(id<0||static_cast<std::size_t>(id)>=samples->size())
{
throw std::out_of_range("Index was out of range. Must be non-negative and less than the size of the collection. (Parameter 'index')");
}
ExportSample(samples->at(static_cast<std::size_t>(id)),adpcmRoundingError);
}
void SoundRead::ExportWfsSample(std::int32_t id,bool adpcmRoundingError)
{
const auto samples=ReadWfsSoundSamples();
if(id<0||static_cast<std::size_t>(id)>=samples->size())
{
throw std::out_of_range("Index was out of range. Must be non-negative and less than the size of the collection. (Parameter 'index')");
}
ExportSample(samples->at(static_cast<std::size_t>(id)),adpcmRoundingError);
}
std::shared_ptr<std::vector<std::uint8_t>> SoundRead::GetWaveData(
const std::shared_ptr<SoundSample>&sample,bool adpcmRoundingError)
{
if(!sample)
{
throw std::runtime_error("Object reference not set to an instance of an object.");
}
return DecodeWaveDataVector(sample->CreateSpan(),sample->Format,
GetSampleCount(*sample),adpcmRoundingError);
}
void SoundRead::GetWaveData(std::span<const std::uint8_t> data,WaveFormat format,
std::uint32_t sampleCount,bool adpcmRoundingError,BinaryWriter&writer)
{
DecodeWaveData(data,format,sampleCount,adpcmRoundingError,writer);
}
std::uint32_t SoundRead::GetSampleCount(const SoundSample&sample)noexcept
{
return GetSampleCountValue(sample.Format,sample.SampleStart,sample.SampleLength);
}
void SoundRead::ExportSample(const std::shared_ptr<SoundSample>&sample,
bool adpcmRoundingError,const std::optional<std::string>&prefix)
{
if(!sample)
{
throw std::runtime_error("Object reference not set to an instance of an object.");
}
const std::string id=PadId(sample->Id);
const auto waveData=GetWaveData(sample,adpcmRoundingError);
ExportAudio(*waveData,GetSampleCount(*sample),sample->SampleRate,sample->Format,id,prefix);
}
void SoundRead::WriteWavHeader(BinaryWriter&writer,std::uint32_t sampleCount,
std::uint16_t sampleRate,WaveFormat format)
{
const std::uint32_t bitsPerSample=format==WaveFormat::PCM8?8U:16U;
const std::uint32_t headerSize=0x2CU;
const std::uint32_t waveSize=sampleCount*bitsPerSample/8U+headerSize;
const std::uint32_t decodedSize=sampleCount*(bitsPerSample/8U);
BinaryWriterExtensions::WriteC(writer,"RIFF");
BinaryWriterExtensions::Write4(writer,waveSize-8U);
BinaryWriterExtensions::WriteC(writer,"WAVE");
BinaryWriterExtensions::WriteC(writer,"fmt ");
BinaryWriterExtensions::Write4(writer,16);
BinaryWriterExtensions::Write2(writer,1);
BinaryWriterExtensions::Write2(writer,1);
BinaryWriterExtensions::Write4(writer,static_cast<std::uint32_t>(sampleRate));
BinaryWriterExtensions::Write4(writer,
static_cast<std::uint32_t>(sampleRate)*(bitsPerSample/8U));
BinaryWriterExtensions::Write2(writer,bitsPerSample/8U);
BinaryWriterExtensions::Write2(writer,bitsPerSample);
BinaryWriterExtensions::WriteC(writer,"data");
BinaryWriterExtensions::Write4(writer,decodedSize);
}
void SoundRead::ExportAudio(std::span<const std::uint8_t> waveData,
std::uint32_t sampleCount,std::uint16_t sampleRate,WaveFormat format,
const std::string&name,const std::optional<std::string>&prefix)
{
if(waveData.empty())
{
throw WaveExportException("Sample "+name+" contains no data.");
}
if(format!=WaveFormat::ADPCM&&format!=WaveFormat::PCM8&&format!=WaveFormat::PCM16)
{
throw WaveExportException("Format "+WaveFormatString(format)+" is unsupported.");
}
std::string path=MphRead::Paths::Combine(MphRead::Paths::Export(),"_SFX");
std::filesystem::create_directories(path);
path=MphRead::Paths::Combine(path,prefix.value_or("")+name+".wav");
std::ofstream stream(path,std::ios::binary|std::ios::trunc);
if(!stream)
{
throw std::ios_base::failure("Could not create file: "+path);
}
BinaryWriter writer(stream);
WriteWavHeader(writer,sampleCount,sampleRate,format);
for(const std::uint8_t value:waveData)
{
writer.Write(value);
}
Nop();
}
std::shared_ptr<const std::vector<std::shared_ptr<SoundSample>>> SoundRead::ReadSoundSamples()
{
return ReadSoundSamples("SNDSAMPLES.DAT");
}
std::shared_ptr<const std::vector<std::shared_ptr<SoundSample>>> SoundRead::ReadWfsSoundSamples()
{
return ReadSoundSamples("WFSSNDSAMPLES.DAT");
}
std::shared_ptr<const std::vector<std::shared_ptr<SoundSample>>> SoundRead::ReadSoundSamples(
const std::string&filename)
{
const std::string path=MphRead::Paths::Combine(
MphRead::Paths::FileSystem(),"data","sound",filename);
const std::vector<std::uint8_t> storage=FileReadAllBytes(path);
const std::span<const std::uint8_t> bytes(storage);
const std::uint32_t count=MphRead::Read::SpanReadUint(bytes,0);
#ifndef NDEBUG
assert(count> 0);
#endif
const auto offsets=MphRead::Read::DoOffsets<std::uint32_t>(bytes,4,count);
auto samples=std::make_shared<std::vector<std::shared_ptr<SoundSample>>>();
std::uint32_t id=0;
for(const std::uint32_t offset:*offsets)
{
if(offset==0)
{
samples->push_back(SoundSample::CreateNull(id));
}
else
{
const SoundSampleHeader header=MphRead::Read::DoOffset<SoundSampleHeader>(bytes,offset);
const std::uint64_t start=static_cast<std::uint64_t>(offset)+sizeof(SoundSampleHeader);
const std::uint32_t size=(static_cast<std::uint32_t>(header.LoopStart)
+header.LoopLength)*4U;
samples->push_back(std::make_shared<SoundSample>(
id,offset,header,CheckedSlice(bytes,start,size)));
}
++id;
}
return samples;
}
std::shared_ptr<const std::vector<std::shared_ptr<SoundSelectEntry>>> SoundRead::ReadBgmSelectList()
{
return ReadSelectList("BGMSELECTLIST.DAT");
}
std::shared_ptr<const std::vector<std::shared_ptr<SoundSelectEntry>>> SoundRead::ReadSfxSelectList()
{
return ReadSelectList("SFXSELECTLIST.DAT");
}
std::shared_ptr<const std::vector<std::shared_ptr<SoundSelectEntry>>> SoundRead::ReadSelectList(
const std::string&filename)
{
const std::string path=MphRead::Paths::Combine(
MphRead::Paths::FileSystem(),"data","sound",filename);
const std::vector<std::uint8_t> storage=FileReadAllBytes(path);
const std::span<const std::uint8_t> bytes(storage);
const std::uint32_t count=MphRead::Read::SpanReadUint(bytes,0);
#ifndef NDEBUG
assert(count> 0);
#endif
const auto rawEntries=MphRead::Read::DoOffsets<RawSoundSelectEntry>(bytes,4,count);
const std::int64_t offset=static_cast<std::int64_t>(count)*sizeof(RawSoundSelectEntry)+4;
const auto names=MphRead::Read::ReadStrings(bytes,offset,count);
auto entries=std::make_shared<std::vector<std::shared_ptr<SoundSelectEntry>>>();
for(std::size_t i=0;i<rawEntries->size();++i)
{
entries->push_back(std::make_shared<SoundSelectEntry>(names->at(i),rawEntries->at(i)));
}
return entries;
}
std::shared_ptr<const std::vector<Sound3dEntry>> SoundRead::ReadSound3dList()
{
const std::string path=MphRead::Paths::Combine(
MphRead::Paths::FileSystem(),"data","sound","SND3DLIST.DAT");
const std::vector<std::uint8_t> storage=FileReadAllBytes(path);
const std::span<const std::uint8_t> bytes(storage);
const std::uint32_t count=MphRead::Read::SpanReadUint(bytes,0);
#ifndef NDEBUG
assert(count> 0);
#endif
return MphRead::Read::DoOffsets<Sound3dEntry>(bytes,4,count);
}
std::shared_ptr<SoundTable> SoundRead::ReadSoundTables()
{
const std::string path=MphRead::Paths::Combine(
MphRead::Paths::FileSystem(),"data","sound","SNDTBLS.DAT");
const std::vector<std::uint8_t> storage=FileReadAllBytes(path);
const std::span<const std::uint8_t> bytes(storage);
const std::uint32_t count=MphRead::Read::SpanReadUint(bytes,0);
#ifndef NDEBUG
assert(count> 0);
#endif
const auto rawEntries=MphRead::Read::DoOffsets<RawSoundTableEntry>(bytes,4,count);
std::int64_t offset=static_cast<std::int64_t>(count)*sizeof(RawSoundTableEntry)+4;
const auto names=MphRead::Read::ReadStrings(bytes,offset,count);
const std::int32_t nameLength=CheckedStringLengthSum(*names);
const std::int32_t namesCount=static_cast<std::int32_t>(names->size());
if(nameLength> std::numeric_limits<std::int32_t>::max()-namesCount)
{
throw std::overflow_error("Arithmetic operation resulted in an overflow.");
}
offset+=static_cast<std::int64_t>(nameLength+namesCount);
if(rawEntries->empty())
{
throw std::runtime_error("Sequence contains no elements");
}
std::uint8_t maxCategory=rawEntries->front().CategoryId;
for(const RawSoundTableEntry raw:*rawEntries)
{
maxCategory=std::max(maxCategory,raw.CategoryId);
}
const auto categories=MphRead::Read::ReadStrings(
bytes,offset,static_cast<std::int32_t>(maxCategory)+1);
auto entries=std::make_shared<std::vector<std::shared_ptr<SoundTableEntry>>>();
for(std::size_t i=0;i<rawEntries->size();++i)
{
const RawSoundTableEntry raw=rawEntries->at(i);
entries->push_back(std::make_shared<SoundTableEntry>(
names->at(i),categories->at(raw.CategoryId),raw));
}
return std::make_shared<SoundTable>(entries,categories);
}
std::shared_ptr<const std::vector<RoomMusic>> SoundRead::ReadAssignMusic()
{
const std::string path=MphRead::Paths::Combine(
MphRead::Paths::FileSystem(),"data","sound","ASSIGNMUSIC.DAT");
const std::vector<std::uint8_t> storage=FileReadAllBytes(path);
const std::span<const std::uint8_t> bytes(storage);
const std::uint32_t count=MphRead::Read::SpanReadUint(bytes,0);
#ifndef NDEBUG
assert(count> 0);
#endif
return MphRead::Read::DoOffsets<RoomMusic>(bytes,4,count);
}
std::shared_ptr<const std::vector<std::shared_ptr<MusicTrack>>> SoundRead::ReadInterMusicInfo()
{
const std::string path=MphRead::Paths::Combine(
MphRead::Paths::FileSystem(),"data","sound","INTERMUSICINFO.DAT");
const std::vector<std::uint8_t> storage=FileReadAllBytes(path);
const std::span<const std::uint8_t> bytes(storage);
const std::uint32_t count=MphRead::Read::SpanReadUint(bytes,0);
#ifndef NDEBUG
assert(count> 0);
#endif
auto tracks=std::make_shared<std::vector<std::shared_ptr<MusicTrack>>>();
std::uint32_t id=0;
const auto rawTracks=MphRead::Read::DoOffsets<RawMusicTrack>(bytes,4,count);
for(const RawMusicTrack track:*rawTracks)
{
tracks->push_back(std::make_shared<MusicTrack>(id++,track));
}
return tracks;
}
std::shared_ptr<const std::vector<std::shared_ptr<SfxScriptFile>>> SoundRead::ReadSfxScriptFiles()
{
const std::string path=MphRead::Paths::Combine(
MphRead::Paths::FileSystem(),"data","sound","SFXSCRIPTFILES.DAT");
const std::vector<std::uint8_t> storage=FileReadAllBytes(path);
const std::span<const std::uint8_t> bytes(storage);
const std::uint32_t fileCount=MphRead::Read::SpanReadUint(bytes,0);
#ifndef NDEBUG
assert(fileCount> 0);
#endif
const auto headers=MphRead::Read::DoOffsets<SfxScriptHeader>(bytes,4,fileCount);
if(headers->empty())
{
throw std::out_of_range("Index was outside the bounds of the array.");
}
const SfxScriptHeader last=headers->back();
const std::uint32_t namesOffset=last.Offset+static_cast<std::uint32_t>(last.Size);
const auto names=MphRead::Read::ReadStrings(bytes,namesOffset,fileCount);
auto files=std::make_shared<std::vector<std::shared_ptr<SfxScriptFile>>>();
std::int32_t max=0;
for(std::size_t i=0;i<headers->size();++i)
{
const SfxScriptHeader header=headers->at(i);
const std::uint32_t entryCount=MphRead::Read::SpanReadUint(bytes,header.Offset);
const auto entries=MphRead::Read::DoOffsets<RawSfxScriptEntry>(
bytes,header.Offset+4U,entryCount);
files->push_back(std::make_shared<SfxScriptFile>(names->at(i),header,entries));
max=std::max(max,static_cast<std::int32_t>(entries->size()));
}
(void)max;
return files;
}
std::shared_ptr<const std::vector<std::shared_ptr<DgnFile>>> SoundRead::ReadDgnFiles()
{
const std::string path=MphRead::Paths::Combine(
MphRead::Paths::FileSystem(),"data","sound","DGNFILES.DAT");
const std::vector<std::uint8_t> storage=FileReadAllBytes(path);
const std::span<const std::uint8_t> bytes(storage);
const std::uint32_t fileCount=MphRead::Read::SpanReadUint(bytes,0);
#ifndef NDEBUG
assert(fileCount> 0);
#endif
const auto headers=MphRead::Read::DoOffsets<DgnHeader>(bytes,4,fileCount);
if(headers->empty())
{
throw std::out_of_range("Index was outside the bounds of the array.");
}
const DgnHeader last=headers->back();
const std::uint32_t namesOffset=last.Offset+static_cast<std::uint32_t>(last.Size);
const auto names=MphRead::Read::ReadStrings(bytes,namesOffset,fileCount);
auto files=std::make_shared<std::vector<std::shared_ptr<DgnFile>>>();
for(std::size_t i=0;i<headers->size();++i)
{
const DgnHeader header=headers->at(i);
const std::uint32_t entryCount=MphRead::Read::SpanReadUint(bytes,header.Offset);
const auto rawEntries=MphRead::Read::DoOffsets<DgnEntry>(
bytes,header.Offset+4U,entryCount);
auto entries=std::make_shared<std::vector<std::shared_ptr<DgnFileEntry>>>();
for(const DgnEntry entry:*rawEntries)
{
const auto data1=MphRead::Read::DoOffsets<DgnData>(
bytes,header.Offset+entry.Offset1,entry.Count1);
const auto data2=MphRead::Read::DoOffsets<DgnData>(
bytes,header.Offset+entry.Offset2,entry.Count2);
const auto data3=MphRead::Read::DoOffsets<DgnData>(
bytes,header.Offset+entry.Offset3,entry.Count3);
const auto data4=MphRead::Read::DoOffsets<DgnData>(
bytes,header.Offset+entry.Offset4,entry.Count4);
entries->push_back(std::make_shared<DgnFileEntry>(
entry.SfxId,data1,data2,data3,data4));
const auto checkStuff=[](const std::shared_ptr<const std::vector<DgnData>>&data)
{
for(const DgnData item:*data)
{
const std::uint16_t flags=static_cast<std::uint16_t>(item.Value&0xC000U);
if(flags!=0&&flags!=0x4000)
{
DebugBreak();
}
}
};
checkStuff(data1);
checkStuff(data2);
checkStuff(data3);
checkStuff(data4);
}
files->push_back(std::make_shared<DgnFile>(names->at(i),header,entries));
}
return files;
}
void SoundRead::Nop()noexcept
{
}
std::shared_ptr<SoundData> SoundRead::ReadSdat()
{
const std::string path=MphRead::Paths::Combine(
MphRead::Paths::FileSystem(),"data","sound","sound_data.sdat");
const std::vector<std::uint8_t> storage=FileReadAllBytes(path);
const std::span<const std::uint8_t> bytes(storage);
const SdatHeader sdatHeader=MphRead::Read::ReadStruct<SdatHeader>(bytes);
#ifndef NDEBUG
assert(sdatHeader.Type.MarshalString()=="SDAT");
assert(sdatHeader.Magic==0x100FEFFU);
assert(sdatHeader.SymbolBlockOffset!=0);
assert(sdatHeader.InfoBlockOffset!=0);
assert(sdatHeader.FatOffset!=0);
assert(sdatHeader.FileBlockOffset!=0);
#endif
const BlockHeader symbHeader=MphRead::Read::DoOffset<BlockHeader>(
bytes,sdatHeader.SymbolBlockOffset);
#ifndef NDEBUG
assert(symbHeader.Type.MarshalString()=="SYMB");
assert(symbHeader.SeqOffset!=0);
assert(symbHeader.SeqarcOffset!=0);
assert(symbHeader.BankOffset!=0);
assert(symbHeader.WavearcOffset!=0);
assert(symbHeader.PlayerOffset!=0);
assert(symbHeader.GroupOffset!=0);
assert(symbHeader.StrmPlayerOffset!=0);
assert(symbHeader.StrmOffset!=0);
#endif
const auto getNames=[](std::span<const std::uint8_t> localBytes,std::uint32_t offset)
{
auto results=std::make_shared<std::vector<std::string>>();
const std::uint32_t count=MphRead::Read::SpanReadUint(localBytes,offset);
const auto entryOffsets=MphRead::Read::DoOffsets<std::uint32_t>(
localBytes,offset+4U,count);
for(const std::uint32_t entryOffset:*entryOffsets)
{
if(entryOffset!=0)
{
results->push_back(MphRead::Read::ReadString(localBytes,entryOffset));
}
}
return std::shared_ptr<const std::vector<std::string>>(results);
};
const std::span<const std::uint8_t> symbBytes=ManagedSliceFromUint(bytes,sdatHeader.SymbolBlockOffset);
const auto seqNames=getNames(symbBytes,symbHeader.SeqOffset);
const auto bankNames=getNames(symbBytes,symbHeader.BankOffset);
const auto wavearcNames=getNames(symbBytes,symbHeader.WavearcOffset);
const auto playerNames=getNames(symbBytes,symbHeader.PlayerOffset);
const auto groupNames=getNames(symbBytes,symbHeader.GroupOffset);
const auto strmPlayerNames=getNames(symbBytes,symbHeader.StrmPlayerOffset);
const auto strmNames=getNames(symbBytes,symbHeader.StrmOffset);
(void)seqNames;
(void)bankNames;
(void)wavearcNames;
(void)playerNames;
(void)groupNames;
(void)strmPlayerNames;
std::vector<std::pair<std::string,std::shared_ptr<const std::vector<std::string>>>> seqarcNames;
const std::uint32_t arcOffset=symbHeader.SeqarcOffset;
const std::uint32_t seqarcCount=MphRead::Read::SpanReadUint(symbBytes,arcOffset);
const auto seqarcs=MphRead::Read::DoOffsets<SeqArcEntries>(
symbBytes,arcOffset+4U,seqarcCount);
for(const SeqArcEntries seqarc:*seqarcs)
{
const std::string arcName=MphRead::Read::ReadString(symbBytes,seqarc.EntryOffset);
const auto fileNames=getNames(symbBytes,seqarc.FilesOffset);
seqarcNames.emplace_back(arcName,fileNames);
}
const BlockHeader infoHeader=MphRead::Read::DoOffset<BlockHeader>(
bytes,sdatHeader.InfoBlockOffset);
#ifndef NDEBUG
assert(infoHeader.Type.MarshalString()=="INFO");
assert(infoHeader.SeqOffset!=0);
assert(infoHeader.SeqarcOffset!=0);
assert(infoHeader.BankOffset!=0);
assert(infoHeader.WavearcOffset!=0);
assert(infoHeader.PlayerOffset!=0);
assert(infoHeader.GroupOffset!=0);
assert(infoHeader.StrmPlayerOffset!=0);
assert(infoHeader.StrmOffset!=0);
#endif
const auto getStructs=[]<typename T>(std::span<const std::uint8_t> localBytes,std::uint32_t offset)
{
auto results=std::make_shared<std::vector<T>>();
const std::uint32_t count=MphRead::Read::SpanReadUint(localBytes,offset);
const auto structOffsets=MphRead::Read::DoOffsets<std::uint32_t>(
localBytes,offset+4U,count);
for(const std::uint32_t strOffset:*structOffsets)
{
if(strOffset!=0)
{
results->push_back(MphRead::Read::DoOffset<T>(localBytes,strOffset));
}
}
return std::shared_ptr<const std::vector<T>>(results);
};
const std::span<const std::uint8_t> infoBytes=ManagedSliceFromUint(bytes,sdatHeader.InfoBlockOffset);
const auto seqInfo=getStructs.template operator()<SeqInfo>(infoBytes,infoHeader.SeqOffset);
const auto seqarcInfo=getStructs.template operator()<std::uint32_t>(infoBytes,infoHeader.SeqarcOffset);
const auto bankInfo=getStructs.template operator()<BankInfo>(infoBytes,infoHeader.BankOffset);
const auto wavearcInfo=getStructs.template operator()<std::uint32_t>(infoBytes,infoHeader.WavearcOffset);
const auto playerInfo=getStructs.template operator()<PlayerInfo>(infoBytes,infoHeader.PlayerOffset);
const auto strmPlayerInfo=getStructs.template operator()<StrmPlayerInfo>(infoBytes,infoHeader.StrmPlayerOffset);
const auto strmInfo=getStructs.template operator()<StrmInfo>(infoBytes,infoHeader.StrmOffset);
(void)playerInfo;
(void)strmPlayerInfo;
std::vector<std::shared_ptr<const std::vector<GroupItemInfo>>> groupInfo;
std::uint32_t groupOffset=infoHeader.GroupOffset;
const std::uint32_t groupCount=MphRead::Read::SpanReadUint(infoBytes,groupOffset);
groupOffset+=4U;
groupInfo.reserve(groupCount);
for(std::uint32_t i=0;i<groupCount;++i)
{
const std::uint32_t itemCount=MphRead::Read::SpanReadUint(infoBytes,groupOffset);
groupOffset+=4U;
groupInfo.push_back(MphRead::Read::DoOffsets<GroupItemInfo>(infoBytes,groupOffset,itemCount));
groupOffset+=itemCount*8U;
}
std::uint32_t fatOffset=sdatHeader.FatOffset;
const SdatFatHeader fatHeader=MphRead::Read::DoOffset<SdatFatHeader>(bytes,fatOffset);
#ifndef NDEBUG
assert(fatHeader.Type.MarshalString()=="FAT ");
#endif
fatOffset+=12U;
const auto fatEntries=MphRead::Read::DoOffsets<SdatFatEntry>(
bytes,fatOffset,fatHeader.Count);
const SdatFileHeader fileHeader=MphRead::Read::DoOffset<SdatFileHeader>(
bytes,sdatHeader.FileBlockOffset);
#ifndef NDEBUG
assert(fileHeader.Type.MarshalString()=="FILE");
assert(fileHeader.Count==fatHeader.Count);
#endif
std::int32_t filesRead=0;
const auto incrementFilesRead=[&filesRead]()
{
filesRead=std::bit_cast<std::int32_t>(
static_cast<std::uint32_t>(filesRead)+1U);
};
for([[maybe_unused]]const SeqInfo info:*seqInfo)
{
incrementFilesRead();
}
for([[maybe_unused]]const std::uint32_t info:*seqarcInfo)
{
incrementFilesRead();
}
for([[maybe_unused]]const BankInfo&info:*bankInfo)
{
incrementFilesRead();
}
for([[maybe_unused]]const std::uint32_t info:*wavearcInfo)
{
incrementFilesRead();
}
auto streams=std::make_shared<std::vector<std::shared_ptr<SoundStream>>>();
for(std::size_t i=0;i<strmInfo->size();++i)
{
const StrmInfo info=strmInfo->at(i);
const std::string&name=strmNames->at(i);
const std::int32_t fileIndex=ManagedInt32(info.FileId);
if(fileIndex<0)
{
throw std::out_of_range("Index was out of range. Must be non-negative and less than the size of the collection. (Parameter 'index')");
}
const SdatFatEntry entry=fatEntries->at(static_cast<std::size_t>(fileIndex));
const SoundStreamHeader header=MphRead::Read::DoOffset<SoundStreamHeader>(bytes,entry.Offset);
#ifndef NDEBUG
assert(header.Type.MarshalString()=="STRM");
assert(header.HeadType.MarshalString()=="HEAD");
assert(header.Channels==1||header.Channels==2);
assert(header.BlockCount> 1||header.BlockSize==header.LastBlockSize);
assert(header.BlockCount> 1||header.BlockSamples==header.LastBlockSamples);
#endif
const WaveFormat format=static_cast<WaveFormat>(header.Format);
#ifndef NDEBUG
assert(format==WaveFormat::PCM8||format==WaveFormat::ADPCM);
assert(format==WaveFormat::ADPCM||header.BlockCount==1);
#endif
auto channels=std::make_shared<std::vector<std::shared_ptr<std::vector<std::uint8_t>>>>();
std::uint32_t channelSize=header.BlockSamples*(header.BlockCount-1U)
+header.LastBlockSamples;
if(format==WaveFormat::ADPCM)
{
channelSize*=2U;
}
for(std::uint32_t j=0;j<header.Channels;++j)
{
auto channel=std::make_shared<std::vector<std::uint8_t>>(channelSize);
FixedVectorStreamBuf streamBuffer(*channel);
std::ostream stream(&streamBuffer);
BinaryWriter writer(stream);
std::uint32_t start=entry.Offset+header.DataOffset+header.BlockSize*j;
for(std::uint32_t k=0;k<header.BlockCount;++k)
{
std::uint32_t size;
std::uint32_t samples;
std::uint32_t increment;
if(k==header.BlockCount-1U)
{
size=header.LastBlockSize;
samples=header.LastBlockSamples;
increment=0;
}
else
{
size=header.BlockSize;
samples=header.BlockSamples;
if(j> 0&&k==header.BlockCount-2U)
{
increment=size+header.LastBlockSize;
}
else
{
increment=size*header.Channels;
}
}
const std::uint32_t end=start+size;
const auto data=ManagedRangeSlice(bytes,start,end);
GetWaveData(data,format,samples,false,writer);
start+=increment;
}
channels->push_back(channel);
}
streams->push_back(std::make_shared<SoundStream>(
static_cast<std::int32_t>(i),name,header,channels,info.Volume/127.0F));
incrementFilesRead();
}
#ifndef NDEBUG
assert(static_cast<std::int64_t>(filesRead)==static_cast<std::int64_t>(fileHeader.Count));
#endif
return std::make_shared<SoundData>(streams);
}
void SoundRead::ExportStreams()
{
const std::shared_ptr<SoundData> soundData=ReadSdat();
for(const std::shared_ptr<SoundStream>&stream:*soundData->Streams)
{
ExportStream(stream);
}
}
void SoundRead::ExportStream(const std::shared_ptr<SoundStream>&stream)
{
if(!stream)
{
throw std::runtime_error("Object reference not set to an instance of an object.");
}
for(std::size_t i=0;i<stream->Channels->size();++i)
{
const auto&channel=stream->Channels->at(i);
const std::string id=PadId(stream->Id);
std::string suffix;
if(stream->Channels->size()==2)
{
suffix=i==0?"_L":"_R";
}
const std::string filename=id+"_"+stream->Name+suffix;
std::size_t length=channel->size();
if(stream->Format==WaveFormat::ADPCM)
{
length/=2;
}
ExportAudio(*channel,static_cast<std::uint32_t>(length),stream->SampleRate,
stream->Format,filename);
}
}
std::shared_ptr<std::vector<std::uint8_t>> SoundRead::GetStreamBufferData(
const SoundStream&stream)
{
return InterleaveStreamChannels(stream.Channels,stream.Format);
}
namespace FhSoundDependency
{
std::shared_ptr<SoundSample> CreateNullSoundSample(std::uint32_t id)
{
return SoundSample::CreateNull(id);
}
std::shared_ptr<SoundSample> CreateFhSoundSample(std::uint32_t id,
std::uint32_t offset,FhSoundSampleHeader header,std::span<const std::uint8_t> data)
{
return std::make_shared<SoundSample>(id,offset,header,data);
}
void ExportSamples(
const std::shared_ptr<const std::vector<std::shared_ptr<SoundSample>>>&samples,
bool adpcmRoundingError,const std::string&prefix)
{
SoundRead::ExportSamples(samples,adpcmRoundingError,prefix);
}
}
}
