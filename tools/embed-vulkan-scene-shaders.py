"""Embed SPIR-V words and the generated binding contract without runtime files."""
import argparse
import json
import struct
from pathlib import Path


def embed(directory, destination):
    manifest = json.loads((directory / 'bindings.json').read_text(encoding='utf-8'))
    lines = ['#pragma once', '#include <array>', '#include <cstdint>',
             '#include <string_view>',
             'namespace MphRead::NativeRuntime::Rhi::Vulkan::Generated {',
             'struct UniformMember { std::string_view name; std::uint32_t offset, size, count; };',
             'struct TextureBinding { std::string_view name; std::uint32_t image, sampler; };']
    for program, contract in manifest.items():
        for stage in ('vert', 'frag'):
            code = (directory / f'{program}.{stage}.spv').read_bytes()
            if len(code) % 4 or len(code) < 20:
                raise ValueError('Malformed SPIR-V length')
            words = struct.unpack(f'<{len(code)//4}I', code)
            if words[0] != 0x07230203:
                raise ValueError('Malformed SPIR-V magic')
            lines.append(f'inline constexpr std::array<std::uint32_t, {len(words)}> {program}_{stage}{{{{')
            for start in range(0, len(words), 8):
                lines.append(','.join(f'0x{word:08x}U' for word in words[start:start+8]) + ',')
            lines.append('}};')
        lines.append(f'inline constexpr std::uint32_t {program}_uniform_size={contract["uniform_size"]};')
        members = [entry for entry in contract['members'] if 'offset' in entry]
        textures = [entry for entry in contract['members'] if 'image_binding' in entry]
        lines.append(f'inline constexpr std::array<UniformMember,{len(members)}> {program}_uniforms{{{{')
        for entry in members:
            lines.append('{"%s",%d,%d,%d},' % (entry['name'], entry['offset'], entry['size'], entry['count']))
        lines.append('}};')
        lines.append(f'inline constexpr std::array<TextureBinding,{len(textures)}> {program}_textures{{{{')
        for entry in textures:
            lines.append('{"%s",%d,%d},' % (entry['name'], entry['image_binding'], entry['sampler_binding']))
        lines.append('}};')
    lines.append('}')
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text('\n'.join(lines) + '\n', encoding='utf-8', newline='\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--directory', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    embed(args.directory, args.output)
