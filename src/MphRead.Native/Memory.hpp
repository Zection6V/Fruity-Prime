#pragma once

#include <array>
#include <cstdint>
#include <future>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace MphRead
{
    class Scene;
    template <typename T>
    class ManagedArray;
}

namespace MphRead::Memory
{
    class AIAggro;
    class AIContext;
    class CArtifact;
    class CAreaVolume;
    class CBeamEffect;
    class CBeamProjectile;
    class CBomb;
    class CCameraSequence;
    class CDoor;
    class CEnemyBase;
    class CEntity;
    class CFlagBase;
    class CForceField;
    class CHalfturret;
    class CItemInstance;
    class CItemSpawn;
    class CJumpPad;
    class CLightSource;
    class CMorphCamera;
    class CNodeDefense;
    class CObject;
    class COctolithFlag;
    class CPlatform;
    class CPlayer;
    class CPlayerSpawn;
    class CPointModule;
    class CTeleporter;
    class CTriggerVolume;

    class Memory
    {
    public:
        struct SystemInfo final
        {
            std::uint16_t ProcessorArchitecture = 0;
            std::uint16_t Reserved = 0;
            std::uint32_t PageSize = 0;
            std::intptr_t MinimumApplicationAddress = 0;
            std::intptr_t MaximumApplicationAddress = 0;
            std::intptr_t ActiveProcessorMask = 0;
            std::uint32_t NumberOfProcessors = 0;
            std::uint32_t ProcessorType = 0;
            std::uint32_t AllocationGranularity = 0;
            std::uint16_t ProcessorLevel = 0;
            std::uint16_t ProcessorRevision = 0;
        };

        struct MemoryInfo64 final
        {
            std::int64_t BaseAddress = 0;
            std::int64_t AllocationBase = 0;
            std::int32_t AllocationProtect = 0;
            std::int32_t Padding1 = 0;
            std::int64_t RegionSize = 0;
            std::int32_t State = 0;
            std::int32_t Protect = 0;
            std::int32_t lType = 0;
            std::int32_t Padding2 = 0;
        };

        static constexpr std::int32_t Offset = 0x02000000;

        [[nodiscard]] static std::shared_ptr<Memory> Start(
            ::MphRead::Scene* scene = nullptr, bool blocking = true);

        [[nodiscard]] std::shared_ptr<ManagedArray<std::uint8_t>> Buffer() const noexcept;
        [[nodiscard]] std::shared_ptr<std::shared_future<void>> Task() const noexcept;

        void WriteMemory(
            std::intptr_t address,
            std::shared_ptr<ManagedArray<std::uint8_t>> value,
            std::int32_t size);
        void WriteMemory(
            std::int32_t address,
            std::shared_ptr<ManagedArray<std::uint8_t>> value,
            std::int32_t size);

        Memory(const Memory&) = delete;
        Memory(Memory&&) = delete;
        Memory& operator=(const Memory&) = delete;
        Memory& operator=(Memory&&) = delete;
        ~Memory();

    private:
        class AddressInfo
        {
        public:
            class SaveAddressInfo
            {
            public:
                const std::int32_t Story;
                const std::int32_t Type3;
                const std::int32_t Settings;
                const std::int32_t License;
                const std::int32_t Friends;

                SaveAddressInfo(
                    std::int32_t story,
                    std::int32_t type3,
                    std::int32_t settings,
                    std::int32_t license,
                    std::int32_t friends) noexcept;
            };

            const std::int32_t EntityListHead;
            const std::int32_t FrameCount;
            const std::int32_t PlayerUA;
            const std::int32_t Players;
            const std::int32_t CamSeqData;
            const std::int32_t GameState;
            const std::int32_t RoomDesc;
            const std::int32_t Rng2;
            const std::shared_ptr<SaveAddressInfo> Save;

            AddressInfo(
                std::int32_t gameState,
                std::int32_t entityListHead,
                std::int32_t frameCount,
                std::int32_t players,
                std::int32_t playerUa,
                std::int32_t camSeqData,
                std::int32_t roomDesc,
                std::int32_t rng2,
                std::shared_ptr<SaveAddressInfo> save) noexcept;
        };

        static std::shared_ptr<AddressInfo> Addresses;
        static const std::array<
            std::pair<std::string_view, std::shared_ptr<AddressInfo>>, 2> AllAddresses;

        static constexpr std::int32_t _size = 0x400000;

        Memory(
            std::int32_t processId,
            std::int64_t processStartTimeMilliseconds,
            ::MphRead::Scene* scene);

        static void GetSystemInfo(SystemInfo& lpSystemInfo);
        [[nodiscard]] static std::intptr_t OpenProcess(
            std::int32_t dwDesiredAccess, bool bInheritHandle, std::int32_t dwProcessId);
        [[nodiscard]] static std::int32_t VirtualQueryEx(
            std::intptr_t hProcess,
            std::intptr_t lpAddress,
            MemoryInfo64& lpBuffer,
            std::uint32_t dwLength);
        [[nodiscard]] static bool ReadProcessMemory(
            std::intptr_t hProcess,
            std::intptr_t lpBaseAddress,
            std::uint8_t* lpBuffer,
            std::int32_t nSize,
            std::intptr_t& lpNumberOfBytesRead);
        [[nodiscard]] static bool WriteProcessMemory(
            std::intptr_t hProcess,
            std::intptr_t lpBaseAddress,
            const std::uint8_t* lpBuffer,
            std::int32_t nSize,
            std::intptr_t& lpNumberOfBytesRead);
        [[nodiscard]] static std::uint32_t GetLastError();

        void DoProcess();
        void PrintAiContext();
        void SetBaseAddress();
        void Run(bool blocking, std::shared_ptr<Memory> self);
        void RunTaskBody();
        void RefreshMemory();
        void GetEntities();

        [[nodiscard]] std::shared_ptr<CEntity> GetEntity(std::intptr_t address);
        [[nodiscard]] std::shared_ptr<CEntity> GetEntity(std::int32_t address);

        [[nodiscard]] std::intptr_t ProcessHandle();
        [[nodiscard]] std::uint16_t ReadUInt16FromBuffer(std::int32_t offset) const;
        [[nodiscard]] std::int32_t ReadInt32FromBuffer(std::int32_t offset) const;

        std::vector<std::string> _mem{};
        const std::shared_ptr<ManagedArray<std::shared_ptr<AIAggro>>> _aggroItems;

        ::MphRead::Scene* const _scene;
        const std::int32_t _processId;
        const std::int64_t _processStartTimeMilliseconds;
        const std::shared_ptr<ManagedArray<std::uint8_t>> _buffer;
        std::intptr_t _baseAddress = 0;
        std::intptr_t _processHandle = 0;

        std::vector<std::shared_ptr<CEntity>> _entities{};
        std::unordered_map<std::intptr_t, std::shared_ptr<CEntity>> _temp{};

        const std::shared_ptr<ManagedArray<std::shared_ptr<CPlayer>>> _players;
        std::string _sb{};

        std::shared_ptr<std::shared_future<void>> _task{};
    };
}
