#include "Formats/model_instance.hpp"
#include <iomanip>
#include <iostream>
#include <sstream>

using namespace fruityprime;
using namespace fruityprime::model;

static File fixture() {
    std::vector<std::uint8_t> b(588, 0);
    const auto put = [&](int offset, std::uint32_t value, int size = 4) {
        for (int i = 0; i < size; ++i) b.at(offset + i) = static_cast<std::uint8_t>(value >> (i * 8));
    };
    put(4, 4096); put(24, 100); put(28, 2, 2); put(32, 580); put(74, 2, 2);
    for (int i = 0; i < 2; ++i) {
        const int n = 100 + i * 240;
        b[n] = static_cast<std::uint8_t>('a' + i);
        put(n + 64, i == 0 ? 0xffff : 0, 2);
        put(n + 66, i == 0 ? 1 : 0xffff, 2);
        put(n + 68, 0xffff, 2); put(n + 72, 1);
        put(n + 80, 4096); put(n + 84, 4096); put(n + 88, 4096);
        put(n + 140, 1); // spherical billboard
        put(580 + i * 4, i);
    }
    return File::from_bytes(std::move(b));
}

static void matrix(const formats::Matrix4& m) {
    for (int i = 0; i < 4; ++i) {
        const auto r = m.row(i);
        std::cout << r.x << ' ' << r.y << ' ' << r.z << ' ' << r.w << ' ';
    }
    std::cout << '\n';
}

int main() {
    std::cout << std::setprecision(9);
    auto file = fixture();
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream in(line);
        char op;
        in >> op;
        try {
            ModelInstance instance(file);
            if (op == 'F') {
                int flags, steps, slot;
                auto& info = instance.animation_info();
                in >> slot >> info.frame[slot] >> info.step[slot] >> info.frame_count[slot] >> flags >> steps;
                info.flags[slot] = static_cast<AnimationFlags>(flags);
                info.node.slot = info.material.slot = info.texture.slot = info.texcoord.slot = slot;
                for (int i = 0; i < steps; ++i) instance.update_anim_frames();
                std::cout << info.frame[slot] << ' ' << static_cast<unsigned>(info.flags[slot]) << '\n';
            } else if (op == 'I') {
                int start, frame, blend, length, count, rotation, size;
                in >> start >> frame >> blend >> length >> count >> rotation >> size;
                std::vector<float> values(size);
                for (auto& value : values) in >> value;
                std::cout << interpolate_animation(values,start,frame,blend,length,count,rotation != 0) << '\n';
            } else if (op == 'N') {
                NodeAnimationGroup group;
                group.frame_count = 1;
                group.scales.resize(3); group.rotations.resize(3); group.translations.resize(3);
                for (auto& x : group.scales) in >> x;
                for (auto& x : group.rotations) in >> x;
                for (auto& x : group.translations) in >> x;
                formats::Vector3 scale; in >> scale.x >> scale.y >> scale.z;
                raw::NodeAnimation anim{};
                anim.scale_lut_length_x = anim.scale_lut_length_y = anim.scale_lut_length_z = 1;
                anim.rotate_lut_length_x = anim.rotate_lut_length_y = anim.rotate_lut_length_z = 1;
                anim.translate_lut_length_x = anim.translate_lut_length_y = anim.translate_lut_length_z = 1;
                anim.scale_lut_index_y = anim.rotate_lut_index_y = anim.translate_lut_index_y = 1;
                anim.scale_lut_index_z = anim.rotate_lut_index_z = anim.translate_lut_index_z = 2;
                matrix(animate_node(group, anim, 0, scale));
            } else if (op == 'U') {
                TexcoordAnimationGroup group;
                group.frame_count = 1;
                group.scales.resize(2); group.rotations.resize(1); group.translations.resize(2);
                for (auto& x : group.scales) in >> x;
                for (auto& x : group.rotations) in >> x;
                for (auto& x : group.translations) in >> x;
                raw::TexcoordAnimation anim{};
                anim.scale_lut_length_s = anim.scale_lut_length_t = anim.rotate_lut_length_z = 1;
                anim.translate_lut_length_s = anim.translate_lut_length_t = 1;
                anim.scale_lut_index_t = anim.translate_lut_index_t = 1;
                matrix(animate_texcoords(group, anim, 0));
            } else if (op == 'T' || op == 'B') {
                auto& state = instance.node_states()[0];
                in >> state.scale.x >> state.scale.y >> state.scale.z
                   >> state.angle.x >> state.angle.y >> state.angle.z
                   >> state.position.x >> state.position.y >> state.position.z;
                instance.compute_node_matrices(0);
                if (op == 'T') matrix(state.transform);
                else {
                    state.animation = state.transform;
                    instance.update_matrix_stack();
                    matrix(instance.matrix_stack()[0]);
                }
            } else if (op == 'H') {
                int mode, ignore, attachment; float value;
                in >> mode >> ignore >> attachment >> value;
                auto& root = instance.node_states()[0];
                root.transform.m41 = 2;
                root.anim_ignore_child = ignore != 0;
                formats::Matrix4 transform;
                transform.m42 = value;
                if (attachment == 1) root.before_transform = transform;
                if (attachment == 2) root.after_transform = transform;
                instance.node_states()[1].animation.m41 = 99;
                formats::Matrix4 parent; parent.m43 = 3;
                if (mode == 0) instance.animate_nodes(0, true, parent, {1,1,1});
                else instance.animate_nodes2(0, true, parent, {1,1,1});
                matrix(root.animation);
                matrix(instance.node_states()[1].animation);
            } else return 2;
        } catch (const std::exception&) { std::cout << "ERR\n"; }
    }
}


