#pragma once

#include <dxgi1_6.h>

#include <ThirdParty/d3dx12/d3dx12.h>

#include <CoreDefs.h>
#include <BitFlags.h>
#include <Platform/DX12/DX12Utility.h>
#include <Log.h>

class DX12State;
struct TextureAsset;

constexpr u8 k_num_frames_in_flight = 2;
constexpr u8 k_num_back_buffers = 3;
constexpr u32 k_num_rtv_staging_descriptors = 256;
constexpr u32 k_num_dsv_staging_descriptors = 32;
constexpr u32 k_num_srv_staging_descriptors = 4096;
constexpr u32 k_num_reserved_srv_descriptors = 8192;
constexpr u32 k_num_srv_render_pass_user_descriptors = 65536;
constexpr u32 k_invalid_resource_table_index = UINT_MAX;
constexpr u32 k_max_texture_subresource_count = 32;
constexpr u32 k_max_resource_barriers = 16;
constexpr u32 k_max_input_layout_elements = 16;
constexpr u32 k_imgui_reserved_descriptor_index = 0;

constexpr u32 k_buffer_upload_heap_size = Megabytes(32);
constexpr u32 k_texture_upload_heap_size = Megabytes(128);

struct DX12Descriptor
{
  bool is_valid() const { return m_cpu_handle.ptr != 0; }
  bool is_referenced_by_shader() const { return m_gpu_handle.ptr != 0; }

  D3D12_CPU_DESCRIPTOR_HANDLE m_cpu_handle{ 0 };
  D3D12_GPU_DESCRIPTOR_HANDLE m_gpu_handle{ 0 };
  u32 m_heap_index = 0;
};

class DX12DescriptorHeap
{
public:
    DX12DescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heap_type, u32 num_descriptors, bool is_shader_visible);
    virtual ~DX12DescriptorHeap();

    ID3D12DescriptorHeap* get_heap() const { return m_descriptor_heap.get(); }
    D3D12_DESCRIPTOR_HEAP_TYPE get_heap_type() const { return m_heap_type; }
    D3D12_CPU_DESCRIPTOR_HANDLE get_heap_cpu_start() const { return m_heap_start.m_cpu_handle; }
    D3D12_GPU_DESCRIPTOR_HANDLE get_heap_gpu_start() const { return m_heap_start.m_gpu_handle; }
    u32 get_max_descriptors() const { return m_max_descriptors; }
    u32 get_descriptor_size() const { return m_descriptor_size; }

protected:
    D3D12_DESCRIPTOR_HEAP_TYPE m_heap_type = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
    u32 m_max_descriptors = 0;
    u32 m_descriptor_size = 0;
    bool m_is_shader_visible = false;
    UniquePtr<ID3D12DescriptorHeap, COMDeleter<ID3D12DescriptorHeap>> m_descriptor_heap = nullptr;
    DX12Descriptor m_heap_start{};
};

class DX12StagingDescriptorHeap final : public DX12DescriptorHeap
{
public:
    DX12StagingDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heap_type, u32 num_descriptors);
    ~DX12StagingDescriptorHeap();

    DX12Descriptor create_descriptor();
    void destroy_descriptor(DX12Descriptor descriptor);

private:
    DynamicArray<u32> m_free_descriptors;
    u32 m_current_descriptor_index = 0;
    u32 m_active_handle_count = 0;
    Mutex m_usage_mutex;
};

class DX12RenderPassDescriptorHeap final : public DX12DescriptorHeap
{
public:
  DX12RenderPassDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heap_type, u32 reserved_count, u32 user_count);

  void reset();
  DX12Descriptor allocate_user_descriptor_block(u32 count);
  DX12Descriptor get_reserved_descriptor(u32 index);

private:
  u32 m_reserved_handle_count = 0;
  u32 m_current_descriptor_index = 0;
  Mutex m_usage_mutex;
};

enum class DX12ResourceAccess : u8
{
  GpuOnly = 0,
  HostWritable = 1,
};

enum class DX12ResourceType : u8
{
  Buffer  = 0,
  Texture = 1,
};

struct DX12Resource
{
  UniquePtr<ID3D12Resource, COMDeleter<ID3D12Resource>> m_resource;
  D3D12_GPU_VIRTUAL_ADDRESS m_gpu_address;
  u32 m_size;
  D3D12_RESOURCE_STATES m_state = D3D12_RESOURCE_STATE_COMMON;
  u32 m_descriptor_heap_index = k_invalid_resource_table_index;
  DX12ResourceType m_type = DX12ResourceType::Buffer;
};

enum class DX12BufferViewFlags : u8
{
  None = 0,
  CBV = 1 << 0,
  SRV = 1 << 1,
  UAV = 1 << 2,
};
using DX12BufferViewBitFlags = BitFlags<DX12BufferViewFlags>;

struct DX12BufferResource final : public DX12Resource
{
  struct Desc
  {
    DX12ResourceAccess m_access = DX12ResourceAccess::GpuOnly;
    u32 m_size = 0;
    u32 m_stride = 0;
    DX12BufferViewBitFlags m_view_flags{ DX12BufferViewFlags::None };
    bool m_is_raw_access = false;
    enum class BufferType : u8
    {
      GenericBuffer = 0,
      VertexBuffer = 1,
      IndexBuffer = 2,
    } m_buffer_type = BufferType::GenericBuffer;
  };

  DX12BufferResource() : DX12Resource()
  {
    m_type = DX12ResourceType::Buffer;
  }

  void copy_data(void* data, u32 data_size, u32 offset = 0)
  {
    zv_assert(m_mapped_data != nullptr && data != nullptr && data_size > 0 && data_size <= m_size);
    u32 aligned_size = align_u32(data_size, 256);
    memcpy_s(m_mapped_data + aligned_size * offset, m_size, data, data_size);
  }

  u32 m_stride;
  u8* m_mapped_data = nullptr;
  DX12Descriptor m_cbv_descriptor{};
  DX12Descriptor m_srv_descriptor{};
  DX12Descriptor m_uav_descriptor{};
};

enum class DX12TextureViewFlags : u8
{
  None = 0,
  RTV = 1 << 0,
  DSV = 1 << 1,
  SRV = 1 << 2,
  UAV = 1 << 3,
};
using DX12TextureViewBitFlags = BitFlags<DX12TextureViewFlags>;

struct DX12TextureResource final : public DX12Resource
{
  struct Desc
  {
    D3D12_RESOURCE_DIMENSION m_dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    u64 m_alignment = 0;
    u64 m_width = 0;
    u32 m_height = 0;
    u16 m_depth_or_array_size = 1;
    u16 m_mip_levels = 1;
    DXGI_FORMAT m_format = DXGI_FORMAT_UNKNOWN;
    DXGI_SAMPLE_DESC m_sample_desc{ 1, 0 };
    D3D12_TEXTURE_LAYOUT m_layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    D3D12_RESOURCE_FLAGS m_flags = D3D12_RESOURCE_FLAG_NONE;
    DX12TextureViewBitFlags m_view_flags{ DX12TextureViewFlags::None };
    DX12ResourceAccess m_access = DX12ResourceAccess::GpuOnly;
    bool m_is_msaa_enabled = false;
  };

  DX12TextureResource() : DX12Resource()
  {
    m_type = DX12ResourceType::Texture;
  }

  DX12Descriptor m_rtv_descriptor{};
  DX12Descriptor m_dsv_descriptor{};
  DX12Descriptor m_srv_descriptor{};
  DX12Descriptor m_uav_descriptor{};
};

// TODO: move
using DX12SubResourceLayouts = StaticArray<D3D12_PLACED_SUBRESOURCE_FOOTPRINT, k_max_texture_subresource_count>;

// TODO: Clean or move
struct DX12TextureData
{
  UniquePtr<DX12TextureResource> m_texture_resource;
  UniquePtr<u8[]> m_texture_data;
  u64 m_size;
  u32 m_num_sub_resources;
  DX12SubResourceLayouts m_sub_resource_layouts;
};

enum class DX12PipelineStateType : u8
{
  Graphics,
  Compute,
};

namespace DX12ResourceSpace
{
  enum
  {
    PerObjectSpace   = 0,
    PerMaterialSpace = 1,
    PerPassSpace     = 2,
    PerFrameSpace    = 3,
    NumSpaces        = 4,
  };
}

struct DX12PipelineResourceBinding
{
  u32 m_binding_index = 0;
  DX12Resource* m_resource = nullptr;
};

class DX12PipelineResourceSpace
{
public:
  void set_cbv(DX12BufferResource* resource);
  void set_srv(const DX12PipelineResourceBinding& binding);
  void set_uav(const DX12PipelineResourceBinding& binding);
  void lock();

  const DX12BufferResource* get_cbv() const { return m_cbv; }
  const DynamicArray<DX12PipelineResourceBinding>& get_uavs() const { return m_uavs; }
  const DynamicArray<DX12PipelineResourceBinding>& get_srvs() const { return m_srvs; }

  bool is_locked() const { return m_is_locked; }

private:
  u32 get_index_of_binding_index(const std::vector<DX12PipelineResourceBinding>& bindings, u32 binding_index);

private:
  DX12BufferResource* m_cbv = nullptr;
  DynamicArray<DX12PipelineResourceBinding> m_uavs;
  DynamicArray<DX12PipelineResourceBinding> m_srvs;
  bool m_is_locked = false;
};

struct DX12PipelineResourceMapping
{
  StaticArray<u32, DX12ResourceSpace::NumSpaces> m_cbv_mapping{};
  StaticArray<u32, DX12ResourceSpace::NumSpaces> m_table_mapping{};
};

using DX12PipelineResourceSpaces = StaticArray<DX12PipelineResourceSpace*, DX12ResourceSpace::NumSpaces>;

struct DX12PipelineInputLayout
{
  StaticArray<D3D12_INPUT_ELEMENT_DESC, k_max_input_layout_elements> m_elements;
  u32 m_num_elements = 0;
};

struct DX12RenderTargetDesc
{
    StaticArray<DXGI_FORMAT, D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT> m_render_target_formats{ DXGI_FORMAT_UNKNOWN };
    u8 m_num_render_targets = 0;
    DXGI_FORMAT m_depth_stencil_format = DXGI_FORMAT_UNKNOWN;
};

struct DX12PipelineState
{
  struct Desc
  {
    const wchar_t* m_vs_path;
    const wchar_t* m_ps_path;

    D3D12_RASTERIZER_DESC m_rasterizer_desc{};
    D3D12_BLEND_DESC m_blend_desc{};
    D3D12_DEPTH_STENCIL_DESC m_depth_stencil_desc{};
    DX12RenderTargetDesc m_render_target_desc{};
    DXGI_SAMPLE_DESC m_sample_desc{};
    D3D12_PRIMITIVE_TOPOLOGY_TYPE m_topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    DX12PipelineInputLayout m_input_layout;
    DX12PipelineResourceSpaces m_spaces{ nullptr };
  };

  UniquePtr<ID3D12PipelineState, COMDeleter<ID3D12PipelineState>> m_pso;
  UniquePtr<ID3D12RootSignature, COMDeleter<ID3D12RootSignature>> m_root_signature;
  DX12PipelineStateType m_type = DX12PipelineStateType::Graphics;
  DX12PipelineResourceMapping m_resource_mapping;
};

inline DX12PipelineState::Desc get_default_pipeline_state_desc()
{
  DX12PipelineState::Desc desc;
  desc.m_rasterizer_desc.FillMode = D3D12_FILL_MODE_SOLID;
  desc.m_rasterizer_desc.CullMode = D3D12_CULL_MODE_BACK;
  desc.m_rasterizer_desc.FrontCounterClockwise = false;
  desc.m_rasterizer_desc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
  desc.m_rasterizer_desc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
  desc.m_rasterizer_desc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
  desc.m_rasterizer_desc.DepthClipEnable = true;
  desc.m_rasterizer_desc.MultisampleEnable = false;
  desc.m_rasterizer_desc.AntialiasedLineEnable = false;
  desc.m_rasterizer_desc.ForcedSampleCount = 0;
  desc.m_rasterizer_desc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

  desc.m_blend_desc.AlphaToCoverageEnable = false;
  desc.m_blend_desc.IndependentBlendEnable = false;

  const D3D12_RENDER_TARGET_BLEND_DESC default_render_target_blend_desc =
  {
    false, false,
    D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
    D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
    D3D12_LOGIC_OP_NOOP,
    D3D12_COLOR_WRITE_ENABLE_ALL,
  };

  for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
  {
    desc.m_blend_desc.RenderTarget[i] = default_render_target_blend_desc;
  }

  desc.m_depth_stencil_desc.DepthEnable = false;
  desc.m_depth_stencil_desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
  desc.m_depth_stencil_desc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
  desc.m_depth_stencil_desc.StencilEnable = false;
  desc.m_depth_stencil_desc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
  desc.m_depth_stencil_desc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;

  const D3D12_DEPTH_STENCILOP_DESC default_stencil_op =
  {D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS};

  desc.m_depth_stencil_desc.FrontFace = default_stencil_op;
  desc.m_depth_stencil_desc.BackFace = default_stencil_op;

  desc.m_sample_desc.Count = 1;
  desc.m_sample_desc.Quality = 0;

  return desc;
}

struct DX12PipelineInfo
{
  DX12PipelineState* m_pipeline = nullptr;
  DynamicArray<DX12TextureResource*> m_render_targets;
  DX12TextureResource* m_depth_stencil_target = nullptr;
};

// Handle command list lifecycle and barriers
class DX12CommandContext
{
public:
  DX12CommandContext(DX12State* dx12_state, D3D12_COMMAND_LIST_TYPE type);
  virtual ~DX12CommandContext();

  ID3D12GraphicsCommandList* get_command_list() { return m_command_list.get(); }
  D3D12_COMMAND_LIST_TYPE get_type() { return m_type; }

  void reset();
  void add_barrier(DX12Resource* resource, D3D12_RESOURCE_STATES new_state);
  void flush_barriers();

protected:
  DX12State* m_dx12_state = nullptr;
  D3D12_COMMAND_LIST_TYPE m_type = D3D12_COMMAND_LIST_TYPE_DIRECT;
  UniquePtr<ID3D12GraphicsCommandList, COMDeleter<ID3D12GraphicsCommandList>> m_command_list = nullptr;
  StaticArray<UniquePtr<ID3D12CommandAllocator, COMDeleter<ID3D12CommandAllocator>>, k_num_frames_in_flight> m_allocators{ nullptr };
  StaticArray<D3D12_RESOURCE_BARRIER, k_max_resource_barriers> m_pending_barriers;
  u32 m_num_pending_barriers;
  DX12RenderPassDescriptorHeap* m_srv_render_pass_descriptor_heap = nullptr;
  D3D12_CPU_DESCRIPTOR_HANDLE m_srv_render_pass_descriptor_heap_handle{ 0 };
};

// Specialized context for rendering operations
class DX12GraphicsCommandContext : public DX12CommandContext
{
public:
  explicit DX12GraphicsCommandContext(DX12State* dx12_state);

  // Render state
  void set_render_targets(u32 num_rtvs, const D3D12_CPU_DESCRIPTOR_HANDLE* rtvs, D3D12_CPU_DESCRIPTOR_HANDLE dsv);
  void set_viewport_and_scissor(u32 width, u32 height);
  void clear_render_target(DX12TextureResource* target, f32 color[4]);
  void clear_depth_stencil_target(DX12TextureResource* target, f32 depth = 1.0f, u8 stencil = 0);

  // Pipeline state
  // void set_pipeline_state(DX12PipelineState* pipeline_state);
  void set_pipeline(const DX12PipelineInfo& pipeline_info);
  void set_pipeline_resources(u32 space, DX12PipelineResourceSpace* resources);
  void set_vertex_buffer(const DX12BufferResource* vertex_buffer);
  void set_index_buffer(const DX12BufferResource* index_buffer);

  void set_primitive_topology(D3D12_PRIMITIVE_TOPOLOGY topology);

  void resolve_msaa_render_target(DX12TextureResource* msaa_rt, DX12TextureResource* current_back_buffer);

  // Drawing
  void draw_indexed(u32 index_count, u32 start_index = 0, u32 base_vertex = 0);
  void draw(u32 vertex_count, u32 start_vertex = 0);

private:
  DX12PipelineState* m_current_pipeline = nullptr;
};

// Specialized context for resource uploads
class DX12UploadCommandContext : public DX12CommandContext
{
public:
  explicit DX12UploadCommandContext(DX12State* dx12_state);

  // For cleanup
  UniquePtr<DX12BufferResource> return_buffer_heap();
  UniquePtr<DX12BufferResource> return_texture_heap();

  // Upload operations
  void record_buffer_upload(DX12BufferResource* dest_buffer, const void* data, u32 size);
  void record_texture_upload(DX12TextureData* texture_data);
  void process_uploads();

private:
  void process_buffer_uploads();
  void process_texture_uploads();

private:
  struct BufferUpload
  {
    ID3D12Resource* m_dest_buffer;
    const void* m_data;
    u64 m_size;
  };
  DynamicArray<BufferUpload> m_pending_buffer_uploads{};

  struct TextureUpload
  {
    ID3D12Resource* m_dest_texture;
    const void* m_data;
    u64 m_size;
    u32 m_num_sub_resources = 0;
    DX12SubResourceLayouts m_sub_resource_layouts{ 0 };
  };
  DynamicArray<TextureUpload> m_pending_texture_uploads{};

  UniquePtr<DX12BufferResource> m_buffer_upload_heap = nullptr;
  UniquePtr<DX12BufferResource> m_texture_upload_heap = nullptr;
};

class DX12CommandQueue
{
public:
  DX12CommandQueue(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type);
  ~DX12CommandQueue();

  ID3D12CommandQueue* get_queue() { return m_queue.get(); }

  void flush();
  bool is_fence_complete(u64 fence_value);
  void wait_for_fence_value(u64 fence_value);

  u64 poll_current_fence_value();
  u64 get_last_completed_fence_value() { return m_last_completed_fence_value; }
  u64 get_next_fence_value() { return m_next_fence_value; }
  u64 execute_command_list(ID3D12GraphicsCommandList* command_list);
  u64 signal_fence();

private:
  UniquePtr<ID3D12CommandQueue, COMDeleter<ID3D12CommandQueue>> m_queue;
  UniquePtr<ID3D12Fence, COMDeleter<ID3D12Fence>> m_fence;
  HANDLE m_fence_event;
  u64 m_next_fence_value = 1;
  u64 m_last_completed_fence_value = 0;
  Mutex m_fence_mutex;
  Mutex m_event_mutex;
};

// Output mode enum for controlling HDR/SDR rendering
enum class DX12OutputMode
{
  SDR,
  HDR10,
  scRGB
};

class DX12State
{
public:
  DX12State(HWND window_handle, u32 client_width, u32 client_height, bool vsync = false, bool msaa_enabled = false, DX12OutputMode output_mode = DX12OutputMode::SDR);
  ~DX12State();

  void flush_queues();

  // Frame management
  void begin_frame();
  void end_frame();
  void present();
  void resize(u32 width, u32 height);

  u32 get_frame_id() const { return m_frame_index; }

  constexpr DX12OutputMode get_output_mode() const { return m_output_mode; }
  constexpr DXGI_FORMAT get_back_buffer_format() const;
  constexpr DXGI_FORMAT get_msaa_format() const;
  constexpr f32 get_reference_white_nits() const { return m_reference_white_nits; }

  DX12Descriptor& get_imgui_descriptor(u32 index) { return m_imgui_descriptors[index]; }

  // Context creation
  UniquePtr<DX12GraphicsCommandContext> create_graphics_context();
  DX12UploadCommandContext* get_upload_context_for_current_frame() { return m_upload_contexts[m_frame_index].get(); }
  
  // Context submission
  void submit_context(DX12CommandContext* context);

  // Basic getters
  ID3D12Device5* get_device() { return m_device.get(); }
  ID3D12CommandQueue* get_graphics_queue() { return m_graphics_queue->get_queue(); }
  IDXGISwapChain4* get_swap_chain() { return m_swap_chain.get(); }
  DX12RenderPassDescriptorHeap* get_srv_heap(u32 frame_index) { return m_srv_render_pass_descriptor_heaps[frame_index].get(); }
  DX12TextureResource* get_back_buffer(u32 frame_index) { return m_back_buffers[frame_index].get(); }

  // Render target access
  DX12TextureResource* get_current_back_buffer() { return m_back_buffers[m_swap_chain->GetCurrentBackBufferIndex()].get(); }
  DX12TextureResource* get_depth_stencil_buffer() { return m_depth_stencil_buffer.get(); }
  DX12TextureResource* get_msaa_render_target() { return m_msaa_render_target.get(); }

  u32 get_msaa_quality_level() const { return m_msaa_quality_level; }

  UniquePtr<DX12BufferResource> create_buffer_resource(const DX12BufferResource::Desc& desc);
  UniquePtr<DX12TextureResource> create_texture_resource(const DX12TextureResource::Desc& desc);
  UniquePtr<DX12TextureData> create_texture_data(TextureAsset* texture_asset);

  void destroy_buffer_resource(UniquePtr<DX12BufferResource> buffer);
  void destroy_texture_resource(UniquePtr<DX12TextureResource> texture);
  void destroy_pipeline_state(UniquePtr<DX12PipelineState> pipeline_state);
  void destroy_context(UniquePtr<DX12CommandContext> context);

  // Pipeline state
  UniquePtr<DX12PipelineState> create_graphics_pipeline(const DX12PipelineState::Desc& desc);
  
private:
  void update_render_target_views();
  void create_depth_stencil_buffer(u32 width, u32 height);
  void create_msaa_render_target(u32 width, u32 height);
  void copy_srv_handle_to_reserved_table(DX12Descriptor srv_handle, u32 index);
  void process_destructions(u32 frame_index);

  StaticArray<const CD3DX12_STATIC_SAMPLER_DESC, 6> get_static_samplers() const;

  ID3D12RootSignature* create_root_signature(
    const DX12PipelineResourceSpaces& spaces, 
    DX12PipelineResourceMapping& resource_mapping);

private:
  // Core D3D12 objects
  UniquePtr<ID3D12Device5, COMDeleter<ID3D12Device5>> m_device;
  UniquePtr<IDXGISwapChain4, COMDeleter<IDXGISwapChain4>> m_swap_chain;
  UniquePtr<DX12CommandQueue> m_graphics_queue;
  UniquePtr<DX12CommandQueue> m_copy_queue;

  // Frame management
  u32 m_frame_index;
  
  struct FrameFences
  {
    u64 m_graphics_queue_fence = 0;
    u64 m_copy_queue_fence = 0;
  };
  StaticArray<FrameFences, k_num_frames_in_flight> m_frame_fence_values;

  StaticArray<UniquePtr<DX12UploadCommandContext>, k_num_frames_in_flight> m_upload_contexts;

  UniquePtr<DX12StagingDescriptorHeap> m_rtv_staging_descriptor_heap;
  UniquePtr<DX12StagingDescriptorHeap> m_dsv_staging_descriptor_heap;
  UniquePtr<DX12StagingDescriptorHeap> m_srv_staging_descriptor_heap;
  StaticArray<DX12Descriptor, k_num_frames_in_flight> m_imgui_descriptors;
  DynamicArray<u32> m_free_reserved_descriptor_indices;
  StaticArray<UniquePtr<DX12RenderPassDescriptorHeap>, k_num_frames_in_flight> m_srv_render_pass_descriptor_heaps;

  // Render targets
  StaticArray<UniquePtr<DX12TextureResource>, k_num_back_buffers> m_back_buffers;

  UniquePtr<DX12TextureResource> m_depth_stencil_buffer{ nullptr };
  UniquePtr<DX12TextureResource> m_msaa_render_target{ nullptr };

  // State
  bool m_vsync;
  bool m_tearing_supported;
  bool m_msaa_enabled;
  u32 m_msaa_quality_level;
  DX12OutputMode m_output_mode;
  f32 m_reference_white_nits = 80.0f;    // The reference brightness level of the display.

  struct DX12DestructionQueue
  {
    DynamicArray<UniquePtr<DX12BufferResource>> m_buffers_to_destroy;
    DynamicArray<UniquePtr<DX12TextureResource>> m_textures_to_destroy;
    DynamicArray<UniquePtr<DX12PipelineState>> m_pipelines_to_destroy;
    DynamicArray<UniquePtr<DX12CommandContext>> m_contexts_to_destroy;
  };
  StaticArray<DX12DestructionQueue, k_num_frames_in_flight> m_destruction_queues;
};
