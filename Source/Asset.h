#pragma once

#include <Geometry.h>
#include <CoreDefs.h>
#include <Utility.h>
#include <MathLib.h>

#include <Platform/DX12/DX12.h>

#include <Shaders/Shared.h>

struct TextureLoadInfo;

enum class AssetState : u8
{
    Unloaded,
    Queued,
    Loaded,
    Locked,
};

enum class AssetType : u8
{
    Texture,
    Mesh,
    Model,
};

struct AssetId
{
    static constexpr size_t k_max_name_length = 128;

    using IdentifierType = FixedSizeString<k_max_name_length>;

private:
    IdentifierType m_name;
    StringHash m_hash;

public:
    constexpr AssetId() = default;

    // Construct from string literal or C-string
    explicit constexpr AssetId(const char* name)
        : m_name(name)
        , m_hash(m_name)
    {}

    template <size_t N>
    constexpr AssetId(const char (&literal)[N])
        : m_name(literal)
        , m_hash(m_name)
    {}

    explicit constexpr AssetId(const IdentifierType& name)
        : m_name(name)
        , m_hash(m_name)
    {}

    void set(const char* name) { m_name.assign(name); m_hash = StringHash(m_name); }
    template <size_t N>
    void set(const char (&literal)[N]) { m_name.assign(literal); m_hash = StringHash(m_name); }
    void set(const IdentifierType& name) { m_name = name; m_hash = StringHash(m_name); }

    constexpr const IdentifierType& name() const { return m_name; }
    constexpr StringHash hash() const { return m_hash; }
    constexpr bool is_valid() const { return m_hash != 0; }

    bool operator==(const AssetId& other) const { return m_hash == other.m_hash; }
    bool operator!=(const AssetId& other) const { return !(*this == other); }
    bool operator<(const AssetId& other) const { return m_hash < other.m_hash; }
};

template<>
struct std::hash<AssetId>
{
	[[nodiscard]] size_t operator()(const AssetId& id) const noexcept
	{
        return static_cast<size_t>(id.hash());
	}
};

struct Asset
{
    AssetId m_id;
    AssetState m_state = AssetState::Unloaded;
    AssetType m_type;
    u16 m_ref_count = 0;
    // u32 m_size = 0;
    // u8* m_data = nullptr;
    // TODO: FileHandle m_file_handle;

    Asset(AssetType type) : m_type(type) {}
    Asset(const AssetId& id, AssetType type) : m_id(id), m_type(type) {}
};

enum class TextureDimension : u8
{
    Unkown = 0,
    Buffer = 1,
    Texture1D = 2,
    Texture2D = 3,
    Texture3D = 4,
};

enum class TextureFormat : u8
{
    SRGB = 0,
    Linear = 1,
};

struct TextureAsset : public Asset
{
    // struct Desc
    // {
    //     const char* m_path;
    //     TextureFormat m_format = TextureFormat::SRGB;
    //     u32 m_request_channels = 4;
    // };

    u32 m_width;
    u32 m_height;
    u32 m_num_channels;
    UniquePtr<u8[]> m_data;
    TextureDimension m_dimension;
    TextureFormat m_format = TextureFormat::SRGB;
    u16 m_mip_levels = 1;
    u16 m_depth = 1;       // Should be 1 for 1D or 2D textures
    u16 m_array_size = 1;  // For cubemap, this is a multiple of 6

    // TODO: Remove?
    DX12TextureData* m_texture_data = nullptr;

    // TODO: Rename?
    bool is_ready() const { return m_texture_data != nullptr; }

    // UniquePtr<DX12TextureData> m_texture_data = nullptr;

    // bool is_loaded() const { return m_texture_data != nullptr; }

    TextureAsset() : Asset(AssetType::Texture) {}
    TextureAsset(const AssetId& id) : Asset(id, AssetType::Texture) {}
};

enum class SubmeshHandle : s32 { Invalid = -1 };

// TODO: Moved here for now, due to circular dependency with Rendering.h
enum class MaterialTextureType : u32
{
  Albedo = 0,
  Normal = 1,
  MetallicRoughness = 2,
  AO = 3,
  Emissive = 4,
  Specular = 5,
  Overlay = 6,
  NumTextureTypes = 7,
};

static constexpr u32 k_num_material_textures = static_cast<u32>(MaterialTextureType::NumTextureTypes);

enum class ChannelPacking : u8
{
  None = 0,
  Metalness = 1 << 0,  // Blue channel
  Roughness = 1 << 1,  // Green channel
};
using ChannelPackingFlags = BitFlags<ChannelPacking>;
DEFINE_BITMASK_OPERATORS(ChannelPacking);

struct MaterialTextureInfo
{
  MaterialTextureType m_type;
  ChannelPackingFlags m_channel_packing{ ChannelPacking::None };
};

struct MaterialInfo
{
    AssetId m_albedo_texture_id{};
    AssetId m_normal_texture_id{};
    AssetId m_metallic_roughness_texture_id{};
    AssetId m_ao_texture_id{};
    AssetId m_emissive_texture_id{};
    AssetId m_specular_texture_id{};
    AssetId m_overlay_texture_id{};

    // TODO: We should probably do this for each texture type; Create an array of texture infos???
    ChannelPackingFlags m_channel_packing{ ChannelPacking::None };

    Vector3 m_albedo_color = {1.0f, 1.0f, 1.0f};
    f32 m_roughness = 0.5f;
    f32 m_metalness = 0.0f;
    f32 m_specular = 0.5f;
    f32 m_emissive = 0.0f;
    SamplerAddressMode m_sampler_mode = SamplerAddressMode::Wrap;
};

struct SubmeshData
{
    MeshGeometryData m_data{};
    Matrix m_local_transform{};
    Matrix m_world_transform{};
    MaterialInfo m_material_info{};

    SubmeshHandle m_parent = SubmeshHandle::Invalid;
    DynamicArray<SubmeshHandle> m_children = {};
};

struct ModelAsset : public Asset
{
    DynamicArray<SubmeshData> m_submeshes;

    ModelAsset() : Asset(AssetType::Model) {}
    ModelAsset(const AssetId& id) : Asset(id, AssetType::Model) {}
};

// struct MeshAsset : public Asset
// {
//     // TODO: Check Frank Luna's implementation
// };


namespace Assets
{
    void initialize();
    void shutdown();

    void load_texture_asset(const AssetId& id);
    TextureAsset* get_texture_asset(const AssetId& id);

    void load_model_asset(const AssetId& id);
    ModelAsset* get_model_asset(const AssetId& id);

    // TODO: unload functions?
}
