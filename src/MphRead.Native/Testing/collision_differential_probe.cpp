#include "Formats/collision_query.hpp"
#include "Entities/scene.hpp"

#include <iomanip>
#include <iostream>

using namespace fruityprime;
using formats::Vector3;

scene::EntityVolume volume(int kind, Vector3 p, Vector3 direction,
                           float radius, float height) {
    scene::EntityVolume v;
    v.kind = kind == 0 ? scene::VolumeKind::Box
        : kind == 1 ? scene::VolumeKind::Cylinder : scene::VolumeKind::Sphere;
    v.box_position = v.cylinder_position = v.sphere_position = {p.x, p.y, p.z};
    v.box_vector1 = {1, 0, 0};
    v.box_vector2 = {0, 1, 0};
    v.box_vector3 = {0, 0, 1};
    v.box_dot1 = radius;
    v.box_dot2 = height;
    v.box_dot3 = 2 * radius;
    v.cylinder_vector = {direction.x, direction.y, direction.z};
    v.cylinder_radius = v.sphere_radius = radius;
    v.cylinder_dot = height;
    return v;
}

int main() {
    std::cout << std::setprecision(9);
    int op, kind1, kind2;
    Vector3 a, b, c, d;
    float radius, height;
    int sentinel = 0;
    while (std::cin >> op >> kind1 >> kind2
        >> a.x >> a.y >> a.z >> b.x >> b.y >> b.z
        >> c.x >> c.y >> c.z >> d.x >> d.y >> d.z >> radius >> height) {
        const auto one = volume(kind1, a, d, radius, height);
        const auto two = volume(kind2, b, d, radius * 0.5F, height);
        collision::Result r{7, 0x2138, {11, 12, 13, 14}, 15,
                            {16, 17, 18}, 19, &sentinel,
                            {20, 21, 22}, {23, 24, 25}};
        bool hit = false;
        switch (op) {
        case 0: hit = collision::check_sphere_overlap_volume(one, b, radius * 0.5F, r); break;
        case 1: hit = collision::check_cylinder_overlap_volume(one, b, c, radius * 0.5F, r); break;
        case 2: hit = collision::check_cylinders_overlap(a, b, c, d, height, radius, r); break;
        case 3: hit = collision::check_volumes_overlap(one, two, r); break;
        case 4: hit = collision::check_cylinder_overlap_sphere(a, b, c, radius, r); break;
        case 5: hit = collision::check_cylinder_intersect_plane(a, b, {d, height}, r); break;
        case 6: hit = collision::check_cylinder_between_points(a, b, c, height, radius, r); break;
        default: return 2;
        }
        std::cout << hit << ' ' << int(r.field0) << ' ' << r.flags << ' '
            << r.plane.x << ' ' << r.plane.y << ' ' << r.plane.z << ' ' << r.plane.w << ' '
            << r.field14 << ' ' << r.position.x << ' ' << r.position.y << ' ' << r.position.z << ' '
            << r.distance << ' ' << (r.entity_collision != nullptr) << ' '
            << r.edge_point1.x << ' ' << r.edge_point1.y << ' ' << r.edge_point1.z << ' '
            << r.edge_point2.x << ' ' << r.edge_point2.y << ' ' << r.edge_point2.z << '\n';
    }
}
