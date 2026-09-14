MPH uses pre-rendered movie files in a format called VX, which was developed by ActImagine (now Nintendo European Research & Development or NERD), while FH uses an earlier version of the format called FV. VX files are used by other Nintendo DS games, but FV is exclusive to First Hunt. Outside the scope of this project, an even earlier version of FV or FastVideo was used for the Game Boy Advance Video series. The DS video formats could also use an audio codec called FastAudio, not used by MPH or FH.

The movie files are decoded in real time. A separate file is used for the top screen and (optionally) bottom screen, and in cases where both screens are used, stereo audio is implemented by using the top screen's audio for the left channel and bottom screen's for the right channel. VX video is encoded as a variant of H.264, while both VX and FV use encoded 16-bit PCM for audio. More specific information on the codecs may be added to this page in the future.

## VX

### File Header

- Size: 0x30 (48)

| Offset | Size | Type | Name | Description |
:- | :- | :- | :- | :-
0x00 | 4 | char[] | Magic | `VXDS`
0x04 | 4 | uint | FrameCount | Number of video frames.
0x08 | 4 | uint | FrameWidth | Width of a video frame in pixels. Always a multiple of 16. Always 256 in MPH.
0x0C | 4 | uint | FrameHeight | Height of a video frame in pixels. Always a multiple of 16. Always 192 in MPH.
0x10 | 4 | fx | FrameRate | Video frame rate in unsigned, fixed-point format with 16 bits for integral digits and 16 bits for fractional digits. Always 983040 (exactly 15 fps) in MPH.
0x14 | 4 | uint | Quantizer | Video quantizer value between 12 and 161.
0x18 | 4 | uint | SampleRate | Audio sample rate in Hz. Always 22050 (22.05 kHz) in MPH.
0x1C | 4 | uint | StreamCount | Audio stream count. Always 1 in MPH.
0x20 | 4 | uint | MaxDataSize | Maximum size in bytes of the video and audio data of any one frame. Always an even number.
0x24 | 4 | uint | ExtradataOffset | Offset in the file to the audio extradata.
0x28 | 4 | uint | SeekTableOffset | Offset in the file to the seek table entries.
0x2C | 4 | uint | SeekTableCount | Number of seek table entries. Always 1 in MPH.

### Audio Extradata

- Size: 0xC34 (3124)

| Offset | Size | Type | Name | Description |
:- | :- | :- | :- | :-
0x00 | 3072 | short[] | LpcCodebooks | 3D array of size 3 x 64 x 8 = 1536 containing LPC codebook values.
0xC00 | 16 | short[] | ScaleModifiers | Array of size 8 containing scale modifier values.
0xC10 | 32 | int[] | LpcBase | Array of size 8 containing LPC base values.
0xC30 | 4 | int | ScaleInitial | Initial scale value.

### Seek Table Entry

- Size: 8

| Offset | Size | Type | Name | Description |
:- | :- | :- | :- | :-
0x0 | 4 | int | FrameIndex | Index number of the frame this seek table entry points to.
0x4 | 4 | int | FrameOffset | Offset in the file to the start of that frame's data (points to the frame's data size value).

In MPH, seeking is not used, and the lone seek table entry always has a frame index of 0 and offset of 48 (the first frame after the file header).

### Frame Data

- Size: 4 + variable

| Offset | Size | Type | Name | Description |
:- | :- | :- | :- | :-
0x0 | 2 | ushort | DataSize | Size in bytes of the frame data, not including this value, but inclusive of the following two bytes for the audio frame count.
0x2 | 2 | ushort | AudioFrameCount | Number of audio frames that are part of this video frame. Always 11 or 12 in MPH.
0x4 |  |  | Video Data | Bit stream
<span></span> |  |  | Audio Data | Bit stream

The first frame's data begins immediately after the 48-byte file header. The video data is padded to 16-bit alignment before the audio data begins. Because there is no specified size for the video data, the only way to locate the audio data is to parse the video data.

Each audio frame decodes to 128 16-bit PCM samples. The number of audio frames per video frame depends on the video frame rate and audio sample rate.

More details about the codecs may be added to this section in the future.

## FV

### File Header

- Size: 0x24 (36)

| Offset | Size | Type | Name | Description |
:- | :- | :- | :- | :-
0x00 | 4 | char[] | Magic | `FVDS`
0x04 | 4 | uint | FrameCount | Number of video frames.
0x08 | 4 | uint | FrameWidth | Width of a video frame in pixels. Always 256 in FH.
0x0C | 4 | uint | FrameHeight | Height of a video frame in pixels. Always 192 in FH.
0x10 | 4 | fx | FrameRate | Video frame rate in unsigned, fixed-point format with 16 bits for integral digits and 16 bits for fractional digits. Always 983035 (just under 15 fps) in FH.
0x14 | 4 | uint | SampleRate | Audio sample rate in Hz. Always 32768 (32.768 kHz) in FH.
0x18 | 4 | uint | TotalDataSize | Total size in bytes of all frame data. Equal to the size of the whole file minus the 36-byte file header and 16-byte empty final frame header.
0x1C | 4 | uint | MaxDataSize | Maximum size in bytes of the video and audio data of any one frame, not including the frame header.
0x20 | 4 | int | Unknown | Not used in the decoding process. Always 4608 in FH.

### Frame Header

- Size: 0x10 (16)

| Offset | Size | Type | Name | Description |
:- | :- | :- | :- | :-
0x00 | 4 | uint | DataSize | Size in bytes of the frame data, not including the frame header, but including the upcoming video and audio size values.
0x04 | 4 | int | SeekBackOffset | Negative offset from the start of this frame's header to the header of a previous frame that can be seeked back to. If the current frame is the first frame in the file, this value is always 0.
0x08 | 4 | int | SeekAheadOffset | Positive offset from the start of this frame's header to the header of a following frame that can be seeked ahead to. If the current frame is the last frame in the file, this value is always 0.
0x0C | 4 | uint | FrameIndex | Index number of the current frame.

The first frame's data begins immediately after the 36-byte file header. After the last frame, there is a final 16-byte frame header where all the bytes are 0. The decoding process checks for a data size of 0 in this dummy header to identify the end of the file.

Note: In the FH movie file `opening-15fps-up-left.avi.fv`, the last three seek ahead offsets appear to be invalid. The third to last frame has a value of 0, which should only be found on the last frame, and the second to last and last frames have negative values.

### Frame Data

- Size: 8 + variable

| Offset | Size | Type | Name | Description |
:- | :- | :- | :- | :-
0x0 | 4 | uint | VideoDataSize | Size in bytes of the video data.
0x4 |  |  | Video Data | 
<span></span> | 4 | uint | AudioDataSize | Size in bytes of the audio data. Always a multiple of 40. Always 320 or 360 in FH (8 or 9 audio frames).
<span></span> |  |  | Audio Data | 

The frame data begins immediately after the 16-byte frame header.

The video data is controlled by a bit stream which is read as little endian 4-byte dwords, with bits read from MSB to LSB. If reading individual bytes, this means the bytes would be read in the order 3-2-1-0, 7-6-5-4, 11-10-9-8, etc. The game's approach allows each bit to be read by adding the current dword value to itself, which is effectively a left shift by 1, and then using the carry flag, which will hold the value of the bit that was just shifted out. When the current dword is out of bits, the next dword is read and the process repeats. This approach is fast and uses few instructions while avoiding unnecessary memory accesses and maximizing the operations that can be done with only the registers.

Each audio frame decodes to 256 16-bit PCM samples. The number of audio frames per video frame depends on the video frame rate and audio sample rate, and can be determined by dividing the audio data size by 40. Audio frame data consists of 10 4-byte unsigned integers.

More details about the codecs may be added to this section in the future.

### Video Decoding

The output color buffers are of size `width x height x 2`, with each 2 bytes holding a 15-bit color value for one pixel (bits 0-4 for red, bits 5-9 for green, bits 10-14 for blue, and the MSB at bit 15 unused). This color data is intended to be copied directly to the DS's VRAM in "LCDC" mode for direct display, bypassing the 2D and 3D engines. The most recent previous frame's final color buffer must be retained, as the decoding process for the next frame may reference it.

The decoding process for each frame must read from up to three offsets into the video data in tandem. At the start of the video data are two 4-byte offset values, pointing to two of those data locations, while the third data location comes immediately after the 8 bytes occupied by those two values.

| Offset | Size | Type | Name | Description |
:- | :- | :- | :- | :-
0x0 | 4 | uint | Color Offset | Offset to data words used in determining pixel colors.
0x4 | 4 | uint | Control Offset | Offset to bit stream used for controlling the decoding process.
0x8 |  | byte[] | Previous Frame Index Data | Array of bytes used as indices when determining offsets into the previous frame's output buffer.
color offset + 8 |  | short[] | Color Data | Array of words used when determining pixel color output for this frame.
control offset + color offset + 8 |  | uint[] | Control Data | Bit stream used to control the decoding of this frame.

The previous frame index data is optional, and expected to be omitted for the first frame (since there is no previous frame's output). When it is not present, the color offset will be 0, and the color data will begin at `0x8`.

The first offset to be read is the control data, a bit stream whose values will determine the code path for the decoder to take. Depending on the decoding operations specified for this frame, bytes may be consumed from the previous frame index data, and words may be consumed from the color data. Values from each of these three locations are read consecutively and strictly in the advancing direction.

Decoding takes place in 8x8-pixel blocks. For a full frame of 256x192 pixels, this means 24 rows of 32 columns of blocks, or 768 blocks total. Blocks are decoded from left to right, top to bottom.

While a block is being decoded, all of its pixels can be expected to be written with color data, but this data is not necessarily final. Within a frame, adjacent blocks (to the right or below) may write outside their own bounds (to the left or above), affecting previous blocks. Only the rightmost column of 8 pixels, or bottom row of 8 pixels, of a previous block may be affected.

Each block is usually decoded in multiple passes covering smaller sub-blocks.