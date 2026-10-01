// Included inside the OpenGL implementation namespace, after the device
// forward declaration. These native objects never cross the RHI boundary.
class OpenGlBuffer final : public Buffer
{
public:
    OpenGlBuffer(OpenGlGraphicsDevice& device, const BufferDesc& desc, std::int32_t name)
        : _device(&device), _desc(desc), _name(name) {}
    ~OpenGlBuffer() override;
    const BufferDesc& Desc() const noexcept override { return _desc; }
    std::int32_t Name() const noexcept { return _name; }
    OpenGlGraphicsDevice* Device() const noexcept { return _device; }
    void Detach() noexcept { _device = nullptr; }
private:
    OpenGlGraphicsDevice* _device;
    BufferDesc _desc;
    std::int32_t _name;
};

class OpenGlBindingLayout final : public BindingLayout
{
public:
    OpenGlBindingLayout(OpenGlGraphicsDevice& device, BindingLayoutDesc desc)
        : _device(&device), _desc(std::move(desc))
    {
        std::unordered_set<std::uint32_t> bindings;
        for (const auto& entry : _desc.entries)
        {
            if (!entry.count || entry.stages == ShaderStage::None
                || !bindings.insert(entry.binding).second)
                throw std::invalid_argument("OpenGL RHI: invalid or duplicate binding declaration.");
        }
    }
    const BindingLayoutDesc& Desc() const noexcept override { return _desc; }
    OpenGlGraphicsDevice* Device() const noexcept { return _device; }
private:
    OpenGlGraphicsDevice* _device;
    BindingLayoutDesc _desc;
};

// A set keeps its layout description alive independently of the caller's
// temporary layout object, just as pipeline layout descriptions do.
class OpenGlBindingSet final : public BindingSet
{
public:
    OpenGlBindingSet(OpenGlGraphicsDevice& device, const BindingSetDesc& desc);
    const BindingSetDesc& Desc() const noexcept override { return _desc; }
    OpenGlGraphicsDevice* Device() const noexcept { return _device; }
private:
    OpenGlGraphicsDevice* _device;
    OpenGlBindingLayout _layout;
    BindingSetDesc _desc;
};

struct OpenGlSamplerStorage final
{
    OpenGlSamplerStorage(OpenGlGraphicsDevice& device, SamplerDesc desc);
    ~OpenGlSamplerStorage();
    OpenGlGraphicsDevice* Device;
    SamplerDesc Desc;
    unsigned Name = 0;
};

class OpenGlSampler final : public Sampler
{
public:
    explicit OpenGlSampler(std::shared_ptr<OpenGlSamplerStorage> storage) : _storage(std::move(storage)) {}
    const SamplerDesc& Desc() const noexcept override { return _storage->Desc; }
    unsigned Name() const noexcept { return _storage->Name; }
    OpenGlGraphicsDevice* Device() const noexcept { return _storage->Device; }
private:
    std::shared_ptr<OpenGlSamplerStorage> _storage;
};
