#include "Renderer.hpp"
#include <iomanip>
#include <iostream>
#include <sstream>

int main() {
    using namespace fruityprime;
    std::cout << std::setprecision(9);
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream in(line);
        if (line.starts_with("F ")) {
            char op; int type, after, steps;
            float length, delay, dt;
            in >> op >> type >> length >> delay >> after >> dt >> steps;
            renderer::FadeController fade;
            fade.set(static_cast<formats::FadeType>(type), length, true,
                     static_cast<renderer::AfterFade>(after), delay);
            for (int i = 0; i < steps; ++i) fade.update(dt);
            const auto& s = fade.state();
            std::cout << int(s.type) << ' ' << s.percent << ' ' << s.color << ' '
                << s.opacity << ' ' << s.ended << ' ' << int(s.after) << ' ' << s.active << '\n';
            continue;
        }
        renderer::Viewport viewport;
        renderer::Projection projection;
        int clip;
        in >> viewport.width >> viewport.height >> projection.fov_radians
           >> projection.near_clip >> projection.far_clip >> clip;
        projection.use_clip = clip != 0;
        formats::Matrix4 view;
        in >> view.m11 >> view.m12 >> view.m13 >> view.m14
           >> view.m21 >> view.m22 >> view.m23 >> view.m24
           >> view.m31 >> view.m32 >> view.m33 >> view.m34
           >> view.m41 >> view.m42 >> view.m43 >> view.m44;
        formats::Vector3 position;
        in >> position.x >> position.y >> position.z;
        try {
            const auto matrix = projection.matrix(viewport);
            const auto frustum = renderer::build_frustum(view, position,
                projection.fov_radians, viewport, projection.near_clip);
            for (int i = 0; i < 4; ++i) {
                const auto r = matrix.row(i);
                std::cout << r.x << ' ' << r.y << ' ' << r.z << ' ' << r.w << ' ';
            }
            for (int i = 0; i < frustum.count; ++i) {
                const auto& p = frustum.planes[i];
                std::cout << p.plane.x << ' ' << p.plane.y << ' ' << p.plane.z << ' ' << p.plane.w << ' '
                    << p.x_index1 << ' ' << p.x_index2 << ' ' << p.y_index1 << ' ' << p.y_index2 << ' '
                    << p.z_index1 << ' ' << p.z_index2 << ' ';
            }
            std::cout << '\n';
        } catch (const std::exception&) { std::cout << "ERR\n"; }
    }
}
