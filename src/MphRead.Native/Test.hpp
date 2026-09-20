#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace MphRead
{
    template <typename T>
    class Enumerable;

    class Model;

    class Test final
    {
    public:
        Test() = delete;
        Test(const Test&) = delete;
        Test& operator=(const Test&) = delete;

        static void ParseAllModels();
        static void TestAllModels();
        static void TestModelFiles();
        static void TestAllRooms();
        static void TestAllNodes();
        static void TestAllEntities();
        static void TestAllEntityMessages();
        static void TestTriggerVolumes();
        static void TestAreaVolumes();
        static void LightColor(std::uint32_t arg);
        [[nodiscard]] static bool TestBytes(const std::string& one, const std::string& two);
        static void TestDlistBounds();
        static void TestNodeBounds();

    private:
        [[nodiscard]] static Enumerable<std::shared_ptr<Model>> GetAllModels();
        [[nodiscard]] static Enumerable<std::shared_ptr<Model>> GetAllRooms();
        static void WriteAllModels();
        static void Nop() noexcept;
    };
}
