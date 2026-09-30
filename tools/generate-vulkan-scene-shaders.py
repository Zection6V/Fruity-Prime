"""Generate Vulkan GLSL from the frozen desktop shader bodies.

The generated binding manifest is the production uniform/descriptor contract.
Compilation is deliberately separate, so CMake can own the glslc commands.
"""
import argparse
import json
import re
from pathlib import Path


def generate(source, output):
    shaders = dict(re.findall(
        r'const std::string Shaders::(\w+) = R"shader\((.*?)\)shader";',
        source.read_text(encoding="utf-8"), re.S))
    programs = {
        "main": ("VertexShader", "FragmentShader"),
        "composite": ("RttVertexShader", "RttFragmentShader"),
        "cel": ("RttVertexShader", "CelFragmentShader"),
        "shift": ("RttVertexShader", "ShiftFragmentShader"),
    }
    declaration = re.compile(r'^uniform (\w+)(?:\[(\d+)\])? (\w+);$', re.M)
    locations = {"a_position": 0, "a_normal": 1, "a_color": 2,
                 "a_texcoord": 3, "a_texcoord1": 4}
    output.mkdir(parents=True, exist_ok=True)
    manifest = {}
    for program, names in programs.items():
        bodies = [shaders[name] for name in names]
        # Legacy GL accepts unused unmatched varyings. Vulkan requires every
        # declared fragment input to have a vertex output even with -O0.
        for index, body in enumerate(bodies):
            for kind, name in re.findall(r'^varying (\w+) (\w+);$', body, re.M):
                if len(re.findall(r'\b' + re.escape(name) + r'\b', body)) == 1:
                    body = re.sub(r'^varying ' + kind + ' ' + name + r';$', '', body, flags=re.M)
            bodies[index] = body
        uniforms = {}
        for body in bodies:
            for kind, count, name in declaration.findall(body):
                value = (kind, int(count) if count else 0)
                if name in uniforms and uniforms[name] != value:
                    raise ValueError(f"Conflicting uniform {program}.{name}")
                uniforms[name] = value
        offset = 0
        members, textures, metadata = [], [], []
        binding = 1
        for name, (kind, count) in uniforms.items():
            if kind == "sampler2D":
                textures += [f"layout(set=0,binding={binding}) uniform texture2D {name}_image;",
                             f"layout(set=0,binding={binding+1}) uniform sampler {name}_sampler;",
                             f"#define {name} sampler2D({name}_image, {name}_sampler)"]
                metadata.append(dict(name=name, image_binding=binding, sampler_binding=binding+1))
                binding += 2
                continue
            alignment, size = {"bool": (4, 4), "int": (4, 4), "float": (4, 4),
                               "vec3": (16, 12), "vec4": (16, 16),
                               "mat4": (16, 64)}[kind]
            if count:
                alignment = 16
                size = ((size + 15) // 16 * 16) * count
            offset = (offset + alignment - 1) // alignment * alignment
            suffix = f"[{count}]" if count else ""
            members.append(f"layout(offset={offset}) {kind} {name}{suffix};")
            metadata.append(dict(name=name, type=kind, count=count, offset=offset, size=size))
            offset += size
        block = "layout(std140,set=0,binding=0) uniform SceneConstants {\n" + "\n".join(members) + "\n};\n"
        varyings = {}
        for body in bodies:
            for kind, name in re.findall(r'^varying (\w+) (\w+);$', body, re.M):
                if name not in varyings:
                    varyings[name] = len(varyings)
        for stage, body in zip(("vert", "frag"), bodies):
            body = re.sub(r'^#version .*$', '#version 450', body, flags=re.M)
            body = declaration.sub('', body)
            body = re.sub(r'^attribute (\w+) (\w+);$',
                          lambda m: f"layout(location={locations[m[2]]}) in {m[1]} {m[2]};",
                          body, flags=re.M)
            body = re.sub(r'^varying (\w+) (\w+);$',
                          lambda m: f"layout(location={varyings[m[2]]}) {'out' if stage == 'vert' else 'in'} {m[1]} {m[2]};",
                          body, flags=re.M)
            body = body.replace('texture2D(', 'texture(')
            body = body.replace('gl_FragColor', 'fragment_color')
            if stage == 'vert':
                # Preserve GL window-depth values with Vulkan's [0,w] clip Z.
                body = re.sub(r'(gl_Position\s*=\s*[^;]+;)',
                              r'\1\n    gl_Position.z = (gl_Position.z + gl_Position.w) * 0.5;', body)
            prefix = block + '\n'.join(textures) + '\n'
            if stage == 'frag':
                prefix += 'layout(location=0) out vec4 fragment_color;\n'
            body = body.replace('#version 450', '#version 450\n' + prefix, 1)
            (output / f"{program}.{stage}").write_text(body, encoding="utf-8", newline="\n")
        manifest[program] = dict(set=0, uniform_binding=0,
                                 uniform_size=(offset + 15) // 16 * 16, members=metadata)
    (output / 'bindings.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    generate(args.source, args.output)
