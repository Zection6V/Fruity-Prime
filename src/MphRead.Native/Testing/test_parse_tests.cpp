#include "Testing/test_parse.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <stdexcept>
#include <string_view>

int main() {
    using namespace fruityprime::formats;
    using namespace fruityprime::testing::parse;

    const Vector3 vector = parse_vector3(
        "00 10 00 00 00 F0 FF FF 00 20 00 00");
    assert(std::fabs(vector.x - 1.0F) < 0.0001F);
    assert(std::fabs(vector.y + 1.0F) < 0.0001F);
    assert(std::fabs(vector.z - 2.0F) < 0.0001F);

    const std::array<std::string_view, 12> matrix_words{
        "00001000", "00000000", "00000000", "00000000",
        "00001000", "00000000", "00000000", "00000000",
        "00001000", "00000000", "00000000", "00000000"};
    const Matrix4x3 matrix = parse_matrix12(matrix_words);
    assert(matrix.m11 == 1.0F && matrix.m22 == 1.0F
           && matrix.m33 == 1.0F && matrix.m43 == 0.0F);

    const Matrix4x3 from_bytes = parse_matrix48(
        "00 10 00 00 00 00 00 00 00 00 00 00 "
        "00 00 00 00 00 10 00 00 00 00 00 00 "
        "00 00 00 00 00 00 00 00 00 10 00 00 "
        "00 00 00 00 00 00 00 00 00 00 00 00");
    assert(from_bytes.m11 == 1.0F && from_bytes.m22 == 1.0F
           && from_bytes.m33 == 1.0F);

    const Matrix4 matrix16 = parse_matrix16(
        "00001000 00000000 00000000 00000000 "
        "00000000 00001000 00000000 00000000 "
        "00000000 00000000 00001000 00000000 "
        "00000000 00000000 00000000 00001000");
    assert(matrix16.m11 == 1.0F && matrix16.m22 == 1.0F
           && matrix16.m33 == 1.0F && matrix16.m44 == 1.0F);

    const Matrix3 vectors = test_vectors(
        {0.0F, 0.0F, 1.0F}, {0.0F, 1.0F, 0.0F}, {-1.0F, 0.0F, 0.0F});
    assert(std::fabs(vectors.m11) < 0.0001F);
    assert(std::fabs(vectors.m22 - 1.0F) < 0.0001F);
    assert(std::fabs(vectors.m31 + 1.0F) < 0.0001F);
    assert(std::fabs(vectors.m33) < 0.0001F);

    bool rejected = false;
    try {
        (void)parse_vector3("00 00");
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
    rejected = false;
    try {
        (void)parse_matrix16("bad");
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
    return 0;
}
