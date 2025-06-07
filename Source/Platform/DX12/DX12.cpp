// Windows Runtime Library. Needed for Microsoft::WRL::ComPtr<> template class.
#include <wrl.h>
using namespace Microsoft::WRL;  // TODO: namespace

// DirectX 12 specific headers.
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

// D3D12 extension library.
#include <ThirdParty/d3dx12/d3dx12.h>


// The number of swap chain back buffers.
const u8 k_num_frames = 3;

ComPtr<ID3D12Device2> g_device;
ComPtr<IDXGISwapChain4> g_swap_chain;

ComPtr<ID3D12Resource> g_back_buffers[k_num_frames];
UINT g_current_back_buffer_index;

u64 g_frame_fence_values[k_num_frames] = {};

ComPtr<ID3D12DescriptorHeap> g_rtv_descriptor_heap;
UINT g_rtv_descriptor_size;

static bool g_vsync = true;
static bool g_tearing_supported = false;

static constexpr DWORD k_fence_wait_timeout_ms = 10000; // 10 seconds

//////////////////
// Cube Example //
//////////////////

// Vertex buffer for the cube.
ComPtr<ID3D12Resource> g_vertex_buffer;
D3D12_VERTEX_BUFFER_VIEW g_vertex_buffer_view;
// Index buffer for the cube.
ComPtr<ID3D12Resource> g_index_buffer;
D3D12_INDEX_BUFFER_VIEW g_index_buffer_view;

// Depth buffer.
ComPtr<ID3D12Resource> g_depth_buffer;
// Descriptor heap for depth buffer.
ComPtr<ID3D12DescriptorHeap> g_dsv_heap;

// Root signature
ComPtr<ID3D12RootSignature> g_root_signature;

// Pipeline state object.
ComPtr<ID3D12PipelineState> g_pipeline_state;

D3D12_VIEWPORT g_viewport;
D3D12_RECT g_scissor_rect;

float g_fov;

DirectX::XMMATRIX g_model_matrix;
DirectX::XMMATRIX g_view_matrix;
DirectX::XMMATRIX g_projection_matrix;

bool g_content_loaded;

// Vertex data for a colored cube.
struct VertexPosColor
{
  DirectX::XMFLOAT3 m_position;
  DirectX::XMFLOAT3 m_color;
};

static VertexPosColor g_vertices[8] = {
  { DirectX::XMFLOAT3(-1.0f, -1.0f, -1.0f), DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f) }, // 0
  { DirectX::XMFLOAT3(-1.0f,  1.0f, -1.0f), DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f) }, // 1
  { DirectX::XMFLOAT3(1.0f,  1.0f, -1.0f), DirectX::XMFLOAT3(1.0f, 1.0f, 0.0f) }, // 2
  { DirectX::XMFLOAT3(1.0f, -1.0f, -1.0f), DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f) }, // 3
  { DirectX::XMFLOAT3(-1.0f, -1.0f,  1.0f), DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f) }, // 4
  { DirectX::XMFLOAT3(-1.0f,  1.0f,  1.0f), DirectX::XMFLOAT3(0.0f, 1.0f, 1.0f) }, // 5
  { DirectX::XMFLOAT3(1.0f,  1.0f,  1.0f), DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f) }, // 6
  { DirectX::XMFLOAT3(1.0f, -1.0f,  1.0f), DirectX::XMFLOAT3(1.0f, 0.0f, 1.0f) }  // 7
};

static WORD g_indicies[36] =
{
  0, 1, 2, 0, 2, 3,
  4, 6, 5, 4, 7, 6,
  4, 5, 1, 4, 1, 0,
  3, 2, 6, 3, 6, 7,
  1, 5, 6, 1, 6, 2,
  4, 0, 3, 4, 3, 7
};

////////////
/// DX12 ///
////////////

// TODO: Remove this
// From DXSampleHelper.h
// Source: https://github.com/Microsoft/DirectX-Graphics-Samples
inline void throw_if_failed(HRESULT hr)
{
  if (FAILED(hr))
  {
    throw std::exception();
  }
}

struct DX12CommandQueue
{
  ComPtr<ID3D12CommandQueue> m_command_queue = nullptr;
  ComPtr<ID3D12Fence> m_fence = nullptr;
  HANDLE m_fence_event = nullptr;
  u64 m_fence_value = 0;
  D3D12_COMMAND_LIST_TYPE m_type;

  void create(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE queue_type)
  {
    m_type = queue_type;

    D3D12_COMMAND_QUEUE_DESC desc = {};
    desc.Type = queue_type;
    desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    desc.NodeMask = 0;

    HRESULT hr = device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_command_queue));
    if (FAILED(hr))
    {
      // TODO: Handle failure (log or assert)
    }

    hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
    if (FAILED(hr))
    {
      // TODO: Handle failure (log or assert)
    }

    m_fence_value = 0;

    m_fence_event = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (m_fence_event == nullptr)
    {
      // TODO: Handle failure
    }
  }

  void shutdown()
  {
    if (m_fence_event)
    {
      ::CloseHandle(m_fence_event);
    }

    m_fence_event = nullptr;
    m_fence = nullptr;
    m_command_queue = nullptr;
  }

  u64 signal()
  {
    const u64 current_fence = ++m_fence_value;
    m_command_queue->Signal(m_fence.Get(), current_fence);
    return current_fence;
  }

  void wait_for_fence_value(u64 target_fence_value)
  {
    if (m_fence->GetCompletedValue() < target_fence_value)
    {
      // TODO: Replace with error handling
      throw_if_failed(m_fence->SetEventOnCompletion(target_fence_value, m_fence_event));
      DWORD result = ::WaitForSingleObject(m_fence_event, k_fence_wait_timeout_ms);
      if (result != WAIT_OBJECT_0)
      {
        // TODO: error handling
        // // Handle timeout (e.g., log and crash)
        // // NEVER silently ignore this
        // OutputDebugStringA("GPU fence wait timed out!\n");
        // __debugbreak(); // Or assert(false)
      }
    }
  }

  void flush()
  {
    u64 current = signal();
    wait_for_fence_value(current);
  }

  u64 execute(ComPtr<ID3D12GraphicsCommandList2> command_list)
  {
    // TODO: error handling
    throw_if_failed(command_list->Close());

    ID3D12CommandList* const command_lists[] = {
        command_list.Get()
    };
    m_command_queue->ExecuteCommandLists(1, command_lists);

    return signal();
  }
};

struct DX12CommandContext
{
  ComPtr<ID3D12GraphicsCommandList2> m_command_list = nullptr;
  ComPtr<ID3D12CommandAllocator> m_allocator = nullptr;
  D3D12_COMMAND_LIST_TYPE m_type;

  void reset()
  {
    // TODO: Error handling
    throw_if_failed(m_allocator->Reset());
    // TODO: Error handling
    throw_if_failed(m_command_list->Reset(m_allocator.Get(), nullptr)); // No PSO binding here
  }

  void set_allocator_as_private_data()
  {
    m_command_list->SetPrivateDataInterface(__uuidof(ID3D12CommandAllocator), m_allocator.Get());
  }
};

struct DX12CommandContextEntry
{
  uint64_t m_fence_value;
  DX12CommandContext m_context;
};

struct DX12CommandContextPool
{
  std::vector<DX12CommandContextEntry> m_context_pool;     // in-flight
  std::vector<DX12CommandContext> m_available_contexts;    // ready to reuse

  DX12CommandContext create(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type)
  {
    DX12CommandContext ctx = {};
    // TODO: error handling
    throw_if_failed(device->CreateCommandAllocator(type, IID_PPV_ARGS(&ctx.m_allocator)));
    // TODO: error handling
    throw_if_failed(device->CreateCommandList(0, type, ctx.m_allocator.Get(), nullptr, IID_PPV_ARGS(&ctx.m_command_list)));
    // TODO: error handling
    throw_if_failed(ctx.m_command_list->Close());  // initially closed for reuse
    ctx.m_type = type;
    ctx.set_allocator_as_private_data();
    return ctx;
  }

  DX12CommandContext get(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type)
  {
    // Try to find an available context matching the requested type
    for (size_t i = 0; i < m_available_contexts.size(); ++i)
    {
      if (m_available_contexts[i].m_type == type)
      {
        DX12CommandContext ctx = std::move(m_available_contexts[i]);
        m_available_contexts.erase(m_available_contexts.begin() + i);
        ctx.reset();
        return ctx;
      }
    }

    // None available, create new one
    DX12CommandContext ctx = create(device, type);
    ctx.reset();
    return ctx;
  }

  void recycle(u64 fence_value, const DX12CommandContext& ctx)
  {
    m_context_pool.push_back({ fence_value, ctx });
  }

  void update(ComPtr<ID3D12Fence> fence)
  {
    const u64 completed = fence->GetCompletedValue();
    auto it = m_context_pool.begin();
    while (it != m_context_pool.end())
    {
      if (completed >= it->m_fence_value)
      {
        m_available_contexts.push_back(std::move(it->m_context));
        it = m_context_pool.erase(it);
      }
      else
      {
        ++it;
      }
    }
  }

  void shutdown()
  {
    m_context_pool.clear();
    m_available_contexts.clear();
  }
};

DX12CommandQueue g_command_queue_graphics;
DX12CommandQueue g_command_queue_compute;
DX12CommandQueue g_command_queue_copy;
DX12CommandContextPool g_context_pool;


void dx12_enable_debug_layer()
{
  // Always enable the debug layer before doing anything DX12 related
  // so all possible errors generated while creating DX12 objects
  // are caught by the debug layer.
  ComPtr<ID3D12Debug> debug_interface;

  // TODO: Replace with error handling
  throw_if_failed(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_interface)));

  debug_interface->EnableDebugLayer();
}

bool dx12_check_tearing_support()
{
  BOOL allow_tearing = FALSE;

  // Rather than create the DXGI 1.5 factory interface directly, we create the
  // DXGI 1.4 interface and query for the 1.5 interface. This is to enable the
  // graphics debugging tools which will not support the 1.5 factory interface
  // until a future update.
  ComPtr<IDXGIFactory4> factory4;
  if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory4))))
  {
    ComPtr<IDXGIFactory5> factory5;
    if (SUCCEEDED(factory4.As(&factory5)))
    {
      if (FAILED(factory5->CheckFeatureSupport(
              DXGI_FEATURE_PRESENT_ALLOW_TEARING,
              &allow_tearing, sizeof(allow_tearing))))
      {
        allow_tearing = FALSE;
      }
    }
  }

  return allow_tearing == TRUE;
}

ComPtr<IDXGIAdapter4> dx12_get_adapter(bool use_warp)
{
  ComPtr<IDXGIFactory4> dxgi_factory;
  UINT create_factory_flags = 0;

#if defined(_DEBUG)
  create_factory_flags = DXGI_CREATE_FACTORY_DEBUG;
#endif

  // TODO: Replace with error handling
  throw_if_failed(CreateDXGIFactory2(create_factory_flags, IID_PPV_ARGS(&dxgi_factory)));

  ComPtr<IDXGIAdapter1> dxgi_adapter1;
  ComPtr<IDXGIAdapter4> dxgi_adapter4;

  if (use_warp)
  {
    // TODO: Replace with error handling
    throw_if_failed(dxgi_factory->EnumWarpAdapter(IID_PPV_ARGS(&dxgi_adapter1)));
    // TODO: Replace with error handling
    throw_if_failed(dxgi_adapter1.As(&dxgi_adapter4));
  }
  else
  {
    SIZE_T max_dedicated_video_memory = 0;

    for (UINT i = 0; dxgi_factory->EnumAdapters1(i, &dxgi_adapter1) != DXGI_ERROR_NOT_FOUND; ++i)
    {
      DXGI_ADAPTER_DESC1 dxgi_adapter_desc1;
      dxgi_adapter1->GetDesc1(&dxgi_adapter_desc1);

      // Check to see if the adapter can create a D3D12 device without actually
      // creating it. The adapter with the largest dedicated video memory
      // is favored.
      if ((dxgi_adapter_desc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
          SUCCEEDED(D3D12CreateDevice(dxgi_adapter1.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)) &&
          dxgi_adapter_desc1.DedicatedVideoMemory > max_dedicated_video_memory)
      {
        max_dedicated_video_memory = dxgi_adapter_desc1.DedicatedVideoMemory;
        // TODO: Replace with error handling
        throw_if_failed(dxgi_adapter1.As(&dxgi_adapter4));
      }
    }
  }

  return dxgi_adapter4;
}

ComPtr<ID3D12Device2> dx12_create_device(ComPtr<IDXGIAdapter4> adapter)
{
  ComPtr<ID3D12Device2> d3d12_device2;
  // TODO: Replace with error handling
  throw_if_failed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&d3d12_device2)));

  // Enable debug messages in debug mode.
#if defined(_DEBUG)
  ComPtr<ID3D12InfoQueue> info_queue;
  if (SUCCEEDED(d3d12_device2.As(&info_queue)))
  {
    info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
    info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
    info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);
    // Suppress whole categories of messages
    // D3D12_MESSAGE_CATEGORY Categories[] = {};

    // Suppress messages based on their severity level
    D3D12_MESSAGE_SEVERITY severities[] =
        {
            D3D12_MESSAGE_SEVERITY_INFO};

    // Suppress individual messages by their ID
    D3D12_MESSAGE_ID DenyIds[] = {
        D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE, // I'm really not sure how to avoid this message.
        D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,                       // This warning occurs when using capture frame while graphics debugging.
        D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,                     // This warning occurs when using capture frame while graphics debugging.
    };

    D3D12_INFO_QUEUE_FILTER new_filter = {};
    // new_filter.DenyList.NumCategories = _countof(Categories);
    // new_filter.DenyList.pCategoryList = Categories;
    new_filter.DenyList.NumSeverities = _countof(severities);
    new_filter.DenyList.pSeverityList = severities;
    new_filter.DenyList.NumIDs = _countof(DenyIds);
    new_filter.DenyList.pIDList = DenyIds;

    // TODO: Replace with error handling
    throw_if_failed(info_queue->PushStorageFilter(&new_filter));
  }
#endif

  return d3d12_device2;
}

ComPtr<IDXGISwapChain4> dx12_create_swap_chain(
    HWND window,
    ComPtr<ID3D12CommandQueue> command_queue,
    uint32_t width, uint32_t height,
    uint32_t buffer_count)
{
  ComPtr<IDXGISwapChain4> dxgi_swap_chain4;
  ComPtr<IDXGIFactory4> dxgi_factory4;
  UINT create_factory_flags = 0;

#if defined(_DEBUG)
  create_factory_flags = DXGI_CREATE_FACTORY_DEBUG;
#endif

  // TODO: Replace with error handling
  throw_if_failed(CreateDXGIFactory2(create_factory_flags, IID_PPV_ARGS(&dxgi_factory4)));

  DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {};
  swap_chain_desc.Width = width;
  swap_chain_desc.Height = height;
  swap_chain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  swap_chain_desc.Stereo = FALSE;
  swap_chain_desc.SampleDesc = {1, 0};
  swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swap_chain_desc.BufferCount = buffer_count;
  swap_chain_desc.Scaling = DXGI_SCALING_STRETCH;
  swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  swap_chain_desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
  // It is recommended to always allow tearing if tearing support is available.
  swap_chain_desc.Flags = dx12_check_tearing_support() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0; // TODO: Duplicate check for tearing

  ComPtr<IDXGISwapChain1> swapChain1;
  // TODO: Replace with error handling
  throw_if_failed(dxgi_factory4->CreateSwapChainForHwnd(
      command_queue.Get(),
      window,
      &swap_chain_desc,
      nullptr,
      nullptr,
      &swapChain1));

  // Disable the Alt+Enter fullscreen toggle feature. Switching to fullscreen
  // will be handled manually.
  // TODO: Replace with error handling
  throw_if_failed(dxgi_factory4->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER));

  // TODO: Replace with error handling
  throw_if_failed(swapChain1.As(&dxgi_swap_chain4));

  return dxgi_swap_chain4;
}

ComPtr<ID3D12DescriptorHeap> dx12_create_descriptor_heap(
    ComPtr<ID3D12Device2> device,
    D3D12_DESCRIPTOR_HEAP_TYPE type,
    uint32_t num_descriptors)
{
  ComPtr<ID3D12DescriptorHeap> descriptor_heap;

  D3D12_DESCRIPTOR_HEAP_DESC desc = {};
  desc.NumDescriptors = num_descriptors;
  desc.Type = type;

  // TODO: Replace with error handling
  throw_if_failed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptor_heap)));

  return descriptor_heap;
}

void dx12_update_render_target_views(
    ComPtr<ID3D12Device2> device,
    ComPtr<IDXGISwapChain4> swap_chain,
    ComPtr<ID3D12DescriptorHeap> descriptor_heap)
{
  auto rtv_descriptor_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

  CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle(descriptor_heap->GetCPUDescriptorHandleForHeapStart());

  for (int i = 0; i < k_num_frames; ++i)
  {
    ComPtr<ID3D12Resource> back_buffer;
    // TODO: Replace with error handling
    throw_if_failed(swap_chain->GetBuffer(i, IID_PPV_ARGS(&back_buffer)));

    device->CreateRenderTargetView(back_buffer.Get(), nullptr, rtv_handle);

    g_back_buffers[i] = back_buffer;

    rtv_handle.Offset(rtv_descriptor_size);
  }
}

void dx12_flush_command_queues()
{
  g_command_queue_graphics.flush();
  g_command_queue_compute.flush();
  g_command_queue_copy.flush();
}

void dx12_resize_depth_buffer(u32 width, u32 height)
{
  // TODO: Remove this I think
  if (!g_content_loaded)
  {
    return;
  }

  // Flush any GPU commands that might be referencing the depth buffer.
  dx12_flush_command_queues();

  width = ZV::max(1u, width);
  height = ZV::max(1u, height);

  auto device = g_device;

  // Resize screen dependent resources.
  // Create a depth buffer.
  D3D12_CLEAR_VALUE optimized_clear_value = {};
  optimized_clear_value.Format = DXGI_FORMAT_D32_FLOAT;
  optimized_clear_value.DepthStencil = {1.0f, 0};

  CD3DX12_HEAP_PROPERTIES heap_props(D3D12_HEAP_TYPE_DEFAULT);
  CD3DX12_RESOURCE_DESC tex_desc = CD3DX12_RESOURCE_DESC::Tex2D(
      DXGI_FORMAT_D32_FLOAT, width, height,
      1,  0,  1,  0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
  // TODO: error handling
  throw_if_failed(device->CreateCommittedResource(
      &heap_props,
      D3D12_HEAP_FLAG_NONE,
      &tex_desc,
      D3D12_RESOURCE_STATE_DEPTH_WRITE,
      &optimized_clear_value,
      IID_PPV_ARGS(&g_depth_buffer)));

  // Update the depth-stencil view.
  D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
  dsv.Format = DXGI_FORMAT_D32_FLOAT;
  dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
  dsv.Texture2D.MipSlice = 0;
  dsv.Flags = D3D12_DSV_FLAG_NONE;

  device->CreateDepthStencilView(g_depth_buffer.Get(), &dsv,
                                 g_dsv_heap->GetCPUDescriptorHandleForHeapStart());
}

void dx12_resize(u32 width, u32 height)
{
  // Don't allow 0 size swap chain back buffers.
  width = ZV::max(1u, width);
  height = ZV::max(1u, height);

  // Flush the GPU queue to make sure the swap chain's back buffers
  // are not being referenced by an in-flight command list.
  g_command_queue_graphics.flush();

  for (int i = 0; i < k_num_frames; ++i)
  {
    // Any references to the back buffers must be released
    // before the swap chain can be resized.
    g_back_buffers[i].Reset();
  }

  DXGI_SWAP_CHAIN_DESC swap_chain_desc = {};
  // TODO: Replace with error handling
  throw_if_failed(g_swap_chain->GetDesc(&swap_chain_desc));
  // TODO: Replace with error handling
  throw_if_failed(
      g_swap_chain->ResizeBuffers(
          k_num_frames,
          width,
          height,
          swap_chain_desc.BufferDesc.Format,
          swap_chain_desc.Flags));

  g_current_back_buffer_index = g_swap_chain->GetCurrentBackBufferIndex();

  dx12_update_render_target_views(g_device, g_swap_chain, g_rtv_descriptor_heap);

  g_viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));

  dx12_resize_depth_buffer(width, height);
}

D3D12_CPU_DESCRIPTOR_HANDLE dx12_get_current_render_target_view()
{
  return CD3DX12_CPU_DESCRIPTOR_HANDLE(
          g_rtv_descriptor_heap->GetCPUDescriptorHandleForHeapStart(),
          g_current_back_buffer_index,
          g_rtv_descriptor_size);
}

void dx12_transition_resource(
  ComPtr<ID3D12GraphicsCommandList2> command_list,
  ComPtr<ID3D12Resource> resource,
  D3D12_RESOURCE_STATES before_state, D3D12_RESOURCE_STATES after_state)
{
    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        resource.Get(),
        before_state, after_state);

    command_list->ResourceBarrier(1, &barrier);
}

void dx12_clear_rtv(ComPtr<ID3D12GraphicsCommandList2> command_list, D3D12_CPU_DESCRIPTOR_HANDLE rtv, FLOAT* clear_color)
{
  command_list->ClearRenderTargetView(rtv, clear_color, 0, nullptr);
}

void dx12_clear_depth(
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> command_list,
  D3D12_CPU_DESCRIPTOR_HANDLE dsv, 
  FLOAT depth)
{
    command_list->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, depth, 0, 0, nullptr);
}

UINT dx12_present()
{
  UINT sync_interval = g_vsync ? 1 : 0;
  UINT present_flags = g_tearing_supported && !g_vsync ? DXGI_PRESENT_ALLOW_TEARING : 0;
  // TODO: Replace with error handling
  throw_if_failed(g_swap_chain->Present(sync_interval, present_flags));

  return g_swap_chain->GetCurrentBackBufferIndex();
}

void dx12_render_frame()
{
  // Update pool: recycle any completed command contexts
  g_context_pool.update(g_command_queue_graphics.m_fence.Get());

  // Acquire a command context from the pool
  DX12CommandContext ctx = g_context_pool.get(g_device, g_command_queue_graphics.m_type);

  auto back_buffer = g_back_buffers[g_current_back_buffer_index];
  auto rtv = dx12_get_current_render_target_view();
  auto dsv = g_dsv_heap->GetCPUDescriptorHandleForHeapStart();

  // Clear the render target.
  {
    dx12_transition_resource(ctx.m_command_list, back_buffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

    FLOAT clear_color[] = {0.1f, 0.1f, 0.1f, 1.0f};

    dx12_clear_rtv(ctx.m_command_list, rtv, clear_color);
    dx12_clear_depth(ctx.m_command_list, dsv, 1.0f);
  }

  // Update render data
  ctx.m_command_list->SetPipelineState(g_pipeline_state.Get());
  ctx.m_command_list->SetGraphicsRootSignature(g_root_signature.Get());

  ctx.m_command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  ctx.m_command_list->IASetVertexBuffers(0, 1, &g_vertex_buffer_view);
  ctx.m_command_list->IASetIndexBuffer(&g_index_buffer_view);

  ctx.m_command_list->RSSetViewports(1, &g_viewport);
  ctx.m_command_list->RSSetScissorRects(1, &g_scissor_rect);

  ctx.m_command_list->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

  // Update the MVP matrix
  DirectX::XMMATRIX mvp_matrix = DirectX::XMMatrixMultiply(g_model_matrix, g_view_matrix);
  mvp_matrix = XMMatrixMultiply(mvp_matrix, g_projection_matrix);
  ctx.m_command_list->SetGraphicsRoot32BitConstants(0, sizeof(DirectX::XMMATRIX) / 4, &mvp_matrix, 0);

  ctx.m_command_list->DrawIndexedInstanced(_countof(g_indicies), 1, 0, 0, 0);

  // Present
  {
    dx12_transition_resource(ctx.m_command_list, back_buffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

    u64 fence_value = g_command_queue_graphics.execute(ctx.m_command_list);
    g_frame_fence_values[g_current_back_buffer_index] = fence_value;

    g_current_back_buffer_index = dx12_present();

    g_command_queue_graphics.wait_for_fence_value(g_frame_fence_values[g_current_back_buffer_index]);

    g_context_pool.recycle(fence_value, ctx);
  }
}

void dx12_init(HWND window, u32 client_width, u32 client_height, bool use_warp)
{
#if defined(_DEBUG)
  dx12_enable_debug_layer();
#endif

  g_tearing_supported = dx12_check_tearing_support();

  ComPtr<IDXGIAdapter4> dxgi_adapter4 = dx12_get_adapter(use_warp);

  g_device = dx12_create_device(dxgi_adapter4);

  g_command_queue_graphics.create(g_device, D3D12_COMMAND_LIST_TYPE_DIRECT);
  g_command_queue_compute.create(g_device, D3D12_COMMAND_LIST_TYPE_COMPUTE);
  g_command_queue_copy.create(g_device, D3D12_COMMAND_LIST_TYPE_COPY);

  g_swap_chain = dx12_create_swap_chain(
      window,
      g_command_queue_graphics.m_command_queue,
      client_width,
      client_height,
      k_num_frames);

  g_current_back_buffer_index = g_swap_chain->GetCurrentBackBufferIndex();

  g_rtv_descriptor_heap = dx12_create_descriptor_heap(g_device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, k_num_frames);
  g_rtv_descriptor_size = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

  dx12_update_render_target_views(g_device, g_swap_chain, g_rtv_descriptor_heap);
}

void dx12_shutdown()
{
  dx12_flush_command_queues();

  g_context_pool.shutdown();
  g_command_queue_graphics.shutdown();
  g_command_queue_compute.shutdown();
  g_command_queue_copy.shutdown();
}

// #include <Platform/DX12/DX12Cube.cpp>

void dx12_update_buffer_resource(
    ComPtr<ID3D12GraphicsCommandList2> command_list,
    ID3D12Resource **dest_resource,
    ID3D12Resource **intermediate_resource,
    size_t num_elements, size_t element_size, const void *buffer_data,
    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE)
{
  auto device = g_device;

  size_t buffer_size = num_elements * element_size;

  // Create a committed resource for the GPU resource in a default heap.
  CD3DX12_HEAP_PROPERTIES dest_heap_props(D3D12_HEAP_TYPE_DEFAULT);
  CD3DX12_RESOURCE_DESC dest_resource_desc = CD3DX12_RESOURCE_DESC::Buffer(buffer_size, flags);
  // TODO: Error handling
  throw_if_failed(device->CreateCommittedResource(
      &dest_heap_props,
      D3D12_HEAP_FLAG_NONE,
      &dest_resource_desc,
      D3D12_RESOURCE_STATE_COMMON,
      nullptr,
      IID_PPV_ARGS(dest_resource)));

  // Create an committed resource for the upload.
  if (buffer_data)
  {
    CD3DX12_HEAP_PROPERTIES intermediate_heap_props(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC intermediate_resource_desc = CD3DX12_RESOURCE_DESC::Buffer(buffer_size);
    // TODO: Error handling
    throw_if_failed(device->CreateCommittedResource(
        &intermediate_heap_props,
        D3D12_HEAP_FLAG_NONE,
        &intermediate_resource_desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(intermediate_resource)));

    D3D12_SUBRESOURCE_DATA subresourceData = {};
    subresourceData.pData = buffer_data;
    subresourceData.RowPitch = buffer_size;
    subresourceData.SlicePitch = subresourceData.RowPitch;

    UpdateSubresources(command_list.Get(),
                       *dest_resource, *intermediate_resource,
                       0, 0, 1, &subresourceData);
  }
}

bool dx12_cube_example_init(u32 client_width, u32 client_height)
{
  g_scissor_rect = CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX);
  g_viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(client_width), static_cast<float>(client_height));
  g_fov = 45.0f;
  g_content_loaded = false;

  // Check for DirectX Math library support.
  if (!DirectX::XMVerifyCPUSupport())
  {
      MessageBoxA(NULL, "Failed to verify DirectX Math library support.", "Error", MB_OK | MB_ICONERROR);
      return false;
  }

  auto device = g_device;
  auto command_queue = g_command_queue_copy;
  auto ctx = g_context_pool.get(device, command_queue.m_type);

  // Upload vertex buffer data.
  ComPtr<ID3D12Resource> intermediate_vertex_buffer;
  dx12_update_buffer_resource(ctx.m_command_list,
      &g_vertex_buffer, &intermediate_vertex_buffer,
      _countof(g_vertices), sizeof(VertexPosColor), g_vertices);

  // Create the vertex buffer view.
  g_vertex_buffer_view.BufferLocation = g_vertex_buffer->GetGPUVirtualAddress();
  g_vertex_buffer_view.SizeInBytes = sizeof(g_vertices);
  g_vertex_buffer_view.StrideInBytes = sizeof(VertexPosColor);

  // Upload index buffer data.
  ComPtr<ID3D12Resource> intermediate_index_buffer;
  dx12_update_buffer_resource(ctx.m_command_list,
      &g_index_buffer, &intermediate_index_buffer,
      _countof(g_indicies), sizeof(WORD), g_indicies);

  // Create index buffer view.
  g_index_buffer_view.BufferLocation = g_index_buffer->GetGPUVirtualAddress();
  g_index_buffer_view.Format = DXGI_FORMAT_R16_UINT;
  g_index_buffer_view.SizeInBytes = sizeof(g_indicies);

  // Create the descriptor heap for the depth-stencil view.
  D3D12_DESCRIPTOR_HEAP_DESC dsv_heap_desc = {};
  dsv_heap_desc.NumDescriptors = 1;
  dsv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
  dsv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  // TODO: error handling
  throw_if_failed(device->CreateDescriptorHeap(&dsv_heap_desc, IID_PPV_ARGS(&g_dsv_heap)));

  // Load the vertex shader.
  ComPtr<ID3DBlob> vertex_shader_blob;
  // TODO: error handling
  throw_if_failed(D3DReadFileToBlob(L"Assets/Shaders/cube_vs.cso", &vertex_shader_blob));

  // Load the pixel shader.
  ComPtr<ID3DBlob> pixel_shader_blob;
  // TODO: error handling
  throw_if_failed(D3DReadFileToBlob(L"Assets/Shaders/cube_ps.cso", &pixel_shader_blob));

  // Create the vertex input layout
  D3D12_INPUT_ELEMENT_DESC input_layout[] = {
      { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
      { "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
  };

  // Create a root signature.
  D3D12_FEATURE_DATA_ROOT_SIGNATURE feature_data = {};
  feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
  if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &feature_data, sizeof(feature_data))))
  {
      feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
  }

  // Allow input layout and deny unnecessary access to certain pipeline stages.
  D3D12_ROOT_SIGNATURE_FLAGS root_signature_flags =
      D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
      D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
      D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
      D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
      D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

  // A single 32-bit constant root parameter that is used by the vertex shader.
  CD3DX12_ROOT_PARAMETER1 root_parameters[1];
  root_parameters[0].InitAsConstants(sizeof(DirectX::XMMATRIX) / 4, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);

  CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc;
  root_signature_desc.Init_1_1(_countof(root_parameters), root_parameters, 0, nullptr, root_signature_flags);

  // Serialize the root signature.
  ComPtr<ID3DBlob> root_signature_blob;
  ComPtr<ID3DBlob> eroor_blob;
  // TODO: error handling
  throw_if_failed(D3DX12SerializeVersionedRootSignature(&root_signature_desc,
      feature_data.HighestVersion, &root_signature_blob, &eroor_blob));
  // Create the root signature.
  // TODO: error handling
  throw_if_failed(device->CreateRootSignature(0, root_signature_blob->GetBufferPointer(),
      root_signature_blob->GetBufferSize(), IID_PPV_ARGS(&g_root_signature)));

  struct PipelineStateStream
  {
      CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE root_signature;
      CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT input_layout;
      CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY primitive_topology_type;
      CD3DX12_PIPELINE_STATE_STREAM_VS vs;
      CD3DX12_PIPELINE_STATE_STREAM_PS ps;
      CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT dsv_formats;
      CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS rtv_formats;
  } pipeline_state_stream;

  D3D12_RT_FORMAT_ARRAY rtv_formats = {};
  rtv_formats.NumRenderTargets = 1;
  rtv_formats.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

  pipeline_state_stream.root_signature = g_root_signature.Get();
  pipeline_state_stream.input_layout = { input_layout, _countof(input_layout) };
  pipeline_state_stream.primitive_topology_type = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  pipeline_state_stream.vs = CD3DX12_SHADER_BYTECODE(vertex_shader_blob.Get());
  pipeline_state_stream.ps = CD3DX12_SHADER_BYTECODE(pixel_shader_blob.Get());
  pipeline_state_stream.dsv_formats = DXGI_FORMAT_D32_FLOAT;
  pipeline_state_stream.rtv_formats = rtv_formats;

  D3D12_PIPELINE_STATE_STREAM_DESC pipeline_state_stream_desc = {
      sizeof(PipelineStateStream), &pipeline_state_stream
  };
  // TODO: error handling
  throw_if_failed(device->CreatePipelineState(&pipeline_state_stream_desc, IID_PPV_ARGS(&g_pipeline_state)));

  u64 fence_value = command_queue.execute(ctx.m_command_list);
  command_queue.wait_for_fence_value(fence_value);

  g_context_pool.recycle(fence_value, ctx);

  g_content_loaded = true;

  // Resize/Create the depth buffer.
  dx12_resize_depth_buffer(client_width, client_height);

  return true;
}
