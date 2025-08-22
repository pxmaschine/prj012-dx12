#include <Platform/DX12/DX12.h>

#include <dxgidebug.h>
#include <d3dcompiler.h>

#include <MathLib.h>
#include <Platform/PlatformContext.h>

#include <Asset.h>

//---------------------
//  Helper Functions
//---------------------
namespace 
{
  inline void check_hresult(HRESULT hr)
  {
    if (FAILED(hr))
    {
      zv_fatal("Invalid HRESULT: {}", hr);
    }
  }

  inline bool check_tearing_support()
  {
    BOOL allow_tearing = FALSE;

    // Rather than create the DXGI 1.5 factory interface directly, we create the
    // DXGI 1.4 interface and query for the 1.5 interface. This is to enable the
    // graphics debugging tools which will not support the 1.5 factory interface
    // until a future update.
    IDXGIFactory4* factory4;
    if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory4))))
    {
      IDXGIFactory5* factory5;
      if (SUCCEEDED(factory4->QueryInterface(__uuidof(IDXGIFactory5), (void**)&factory5)))
      {
        if (FAILED(factory5->CheckFeatureSupport(
                DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                &allow_tearing, sizeof(allow_tearing))))
        {
          allow_tearing = FALSE;
        }
        safe_release_com_ptr(factory5);
      }
      safe_release_com_ptr(factory4);
    }

    return allow_tearing == TRUE;
  }

  inline UINT check_msaa_quality_level(ID3D12Device* device, DXGI_FORMAT format, UINT sample_count)
  {
    // Check 4X MSAA quality support for our back buffer format.
    // All Direct3D 11 capable devices support 4X MSAA for all render 
    // target formats, so we only need to check quality support.

    D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS ms_quality_levels;
    ms_quality_levels.Format = format;
    ms_quality_levels.SampleCount = sample_count;
    ms_quality_levels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
    ms_quality_levels.NumQualityLevels = 0;
    check_hresult(device->CheckFeatureSupport(
      D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
      &ms_quality_levels,
      sizeof(ms_quality_levels)));

    UINT msaa_quality_level = ms_quality_levels.NumQualityLevels;
    zv_assert_msg(msaa_quality_level > 0, "Unexpected MSAA quality level.");

    return msaa_quality_level;
  }

  inline IDXGIAdapter4* get_adapter()
  {
    IDXGIFactory4* dxgi_factory;
    UINT create_factory_flags = 0;

  #if defined(ZV_DEBUG)
    create_factory_flags = DXGI_CREATE_FACTORY_DEBUG;
  #endif

    check_hresult(CreateDXGIFactory2(create_factory_flags, IID_PPV_ARGS(&dxgi_factory)));

    IDXGIAdapter1* dxgi_adapter1{};
    IDXGIAdapter4* dxgi_adapter4{};

    SIZE_T max_dedicated_video_memory = 0;

    for (UINT i = 0; dxgi_factory->EnumAdapters1(i, &dxgi_adapter1) != DXGI_ERROR_NOT_FOUND; ++i)
    {
      DXGI_ADAPTER_DESC1 dxgi_adapter_desc1;
      dxgi_adapter1->GetDesc1(&dxgi_adapter_desc1);

      // Check to see if the adapter can create a D3D12 device without actually
      // creating it. The adapter with the largest dedicated video memory
      // is favored.
      if ((dxgi_adapter_desc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
          SUCCEEDED(D3D12CreateDevice(dxgi_adapter1, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)) &&
          dxgi_adapter_desc1.DedicatedVideoMemory > max_dedicated_video_memory)
      {
        max_dedicated_video_memory = dxgi_adapter_desc1.DedicatedVideoMemory;
        
        safe_release_com_ptr(dxgi_adapter4);
        check_hresult(dxgi_adapter1->QueryInterface(__uuidof(IDXGIAdapter4), (void**)&dxgi_adapter4));
      }

      safe_release_com_ptr(dxgi_adapter1);
    }
    
    safe_release_com_ptr(dxgi_factory);

    return dxgi_adapter4;
  }

  inline ID3D12Device5* create_device(IDXGIAdapter4* adapter)
  {
    ID3D12Device5* d3d12_device;
    check_hresult(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&d3d12_device)));

    // Enable debug messages in debug mode.
#if defined(ZV_DEBUG)
    ID3D12InfoQueue* info_queue;
    if (SUCCEEDED(d3d12_device->QueryInterface(__uuidof(ID3D12InfoQueue), (void**)&info_queue)))
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

      check_hresult(info_queue->PushStorageFilter(&new_filter));

      safe_release_com_ptr(info_queue);
    }
#endif

    return d3d12_device;
  }

  inline IDXGISwapChain4* create_swap_chain(
      HWND window,
      ID3D12CommandQueue* command_queue,
      uint32_t width, uint32_t height,
      uint32_t buffer_count,
      bool is_tearing_supported)
  {
    IDXGISwapChain4* dxgi_swap_chain4;
    IDXGIFactory4* dxgi_factory4;
    UINT create_factory_flags = 0;

#if defined(ZV_DEBUG)
    create_factory_flags = DXGI_CREATE_FACTORY_DEBUG;
#endif

    check_hresult(CreateDXGIFactory2(create_factory_flags, IID_PPV_ARGS(&dxgi_factory4)));

    DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {};
    swap_chain_desc.Width = width;
    swap_chain_desc.Height = height;
    swap_chain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swap_chain_desc.Stereo = FALSE;
    swap_chain_desc.SampleDesc.Count = 1;
    swap_chain_desc.SampleDesc.Quality = 0;
    swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc.BufferCount = buffer_count;
    swap_chain_desc.Scaling = DXGI_SCALING_STRETCH;
    swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swap_chain_desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    // It is recommended to always allow tearing if tearing support is available.
    swap_chain_desc.Flags = is_tearing_supported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

    IDXGISwapChain1* swap_chain_1;
    check_hresult(dxgi_factory4->CreateSwapChainForHwnd(
        command_queue,
        window,
        &swap_chain_desc,
        nullptr,
        nullptr,
        &swap_chain_1));

    // Disable the Alt+Enter fullscreen toggle feature. Switching to fullscreen
    // will be handled manually.
    check_hresult(dxgi_factory4->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER));

    check_hresult(swap_chain_1->QueryInterface(__uuidof(IDXGISwapChain4), (void**)&dxgi_swap_chain4));

    safe_release_com_ptr(swap_chain_1);
    safe_release_com_ptr(dxgi_factory4);

    return dxgi_swap_chain4;
  }
}

//-------------------------------
//  DX12State Implementation
//-------------------------------

DX12State::DX12State(HWND window_handle, u32 client_width, u32 client_height, bool vsync, bool msaa_enabled)
  : m_vsync(vsync)
  , m_frame_index(0)
  , m_msaa_enabled(msaa_enabled)
{
#if defined(ZV_DEBUG)
  // Always enable the debug layer before doing anything DX12 related
  // so all possible errors generated while creating DX12 objects
  // are caught by the debug layer.
  ID3D12Debug* debug_interface;

  if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_interface))))
  {
      debug_interface->EnableDebugLayer();
      safe_release_com_ptr(debug_interface);
  }
#endif

  m_tearing_supported = check_tearing_support();

  IDXGIAdapter4* dxgi_adapter4 = get_adapter();
  ID3D12Device5* device = create_device(dxgi_adapter4);
  safe_release_com_ptr(dxgi_adapter4);
  m_device.reset(device);

  m_graphics_queue = make_unique_ptr<DX12CommandQueue>(m_device.get(), D3D12_COMMAND_LIST_TYPE_DIRECT);
  m_copy_queue = make_unique_ptr<DX12CommandQueue>(m_device.get(), D3D12_COMMAND_LIST_TYPE_COPY);

  m_rtv_staging_descriptor_heap = make_unique_ptr<DX12StagingDescriptorHeap>(m_device.get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, k_num_rtv_staging_descriptors);
  m_dsv_staging_descriptor_heap = make_unique_ptr<DX12StagingDescriptorHeap>(m_device.get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, k_num_dsv_staging_descriptors);
  m_srv_staging_descriptor_heap = make_unique_ptr<DX12StagingDescriptorHeap>(m_device.get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, k_num_srv_staging_descriptors);

  for (u32 i = 0; i < k_num_frames_in_flight; ++i)
  {
    m_srv_render_pass_descriptor_heaps[i] = make_unique_ptr<DX12RenderPassDescriptorHeap>(m_device.get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, k_num_reserved_srv_descriptors, k_num_srv_render_pass_user_descriptors);
    m_imgui_descriptors[i] = m_srv_render_pass_descriptor_heaps[i]->get_reserved_descriptor(k_imgui_reserved_descriptor_index);
  }

  m_msaa_quality_level = check_msaa_quality_level(m_device.get(), DXGI_FORMAT_R8G8B8A8_UNORM, m_msaa_enabled ? 4 : 1);

  // Create DSV heap
  D3D12_DESCRIPTOR_HEAP_DESC dsv_heap_desc = {};
  dsv_heap_desc.NumDescriptors = 1;
  dsv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
  dsv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

  // Create swap chain
  IDXGISwapChain4* swap_chain = create_swap_chain(
      window_handle,
      m_graphics_queue->get_queue(),
      client_width,
      client_height,
      k_num_back_buffers,
      m_tearing_supported);
  m_swap_chain.reset(swap_chain);

  update_render_target_views();

  for (u32 frameIndex = 0; frameIndex < k_num_frames_in_flight; frameIndex++)
  {
    m_upload_contexts[frameIndex] = make_unique_ptr<DX12UploadCommandContext>(this);
  }

  // The -1 and starting at index 1 accounts for the imgui descriptor.
  m_free_reserved_descriptor_indices.resize(k_num_reserved_srv_descriptors - 1);
  fill_sequential(m_free_reserved_descriptor_indices.begin(), m_free_reserved_descriptor_indices.end(), 1);

  create_depth_stencil_buffer(client_width, client_height);

  if (m_msaa_enabled)
  {
    create_msaa_render_target(client_width, client_height);
  }
}

DX12State::~DX12State()
{
  flush_queues();

  for (u32 buffer_index = 0; buffer_index < k_num_back_buffers; buffer_index++)
  {
    m_rtv_staging_descriptor_heap->destroy_descriptor(m_back_buffers[buffer_index]->m_rtv_descriptor);
    m_back_buffers[buffer_index]->m_resource.reset();
    m_back_buffers[buffer_index] = nullptr;
  }

  if (m_depth_stencil_buffer)
  {
    destroy_texture_resource(move_ptr(m_depth_stencil_buffer));
  }

  if (m_msaa_render_target)
  {
    destroy_texture_resource(move_ptr(m_msaa_render_target));
  }

  m_swap_chain.reset();

  for (u32 frame_index = 0; frame_index < k_num_frames_in_flight; frame_index++)
  {
    destroy_buffer_resource(m_upload_contexts[frame_index]->return_buffer_heap());
    destroy_buffer_resource(m_upload_contexts[frame_index]->return_texture_heap());
  }

  for (u32 i = 0; i < k_num_frames_in_flight; ++i)
  {
    process_destructions(i);
  }

  for (auto& buffer : m_back_buffers)
  {
    buffer.reset();
  }

  m_copy_queue.reset();
  m_graphics_queue.reset();

  m_rtv_staging_descriptor_heap.reset();
  m_dsv_staging_descriptor_heap.reset();
  m_srv_staging_descriptor_heap.reset();

  for (u32 i = 0; i < k_num_frames_in_flight; ++i)
  {
    m_srv_render_pass_descriptor_heaps[i].reset();
    m_upload_contexts[i].reset();
  }

  m_device.reset();

#ifdef ZV_DEBUG
  IDXGIDebug1* debug_interface = nullptr;
  if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug_interface))))
  {
    debug_interface->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
    safe_release_com_ptr(debug_interface);
  }
#endif
}

void DX12State::flush_queues()
{
  m_graphics_queue->flush();
  m_copy_queue->flush();
}

void DX12State::update_render_target_views()
{
  for (u32 i = 0; i < k_num_back_buffers; ++i)
  {
    ID3D12Resource* back_buffer;
    DX12Descriptor back_buffer_rtv_handle = m_rtv_staging_descriptor_heap->create_descriptor();

    D3D12_RENDER_TARGET_VIEW_DESC rtv_desc = {};
    rtv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtv_desc.Texture2D.MipSlice = 0;
    rtv_desc.Texture2D.PlaneSlice = 0;

    check_hresult(m_swap_chain->GetBuffer(i, IID_PPV_ARGS(&back_buffer)));
    m_device->CreateRenderTargetView(back_buffer, &rtv_desc, back_buffer_rtv_handle.m_cpu_handle);

    m_back_buffers[i] = make_unique_ptr<DX12TextureResource>();
    m_back_buffers[i]->m_resource.reset(back_buffer);
    m_back_buffers[i]->m_state = D3D12_RESOURCE_STATE_PRESENT;
    m_back_buffers[i]->m_rtv_descriptor = back_buffer_rtv_handle;
  }
}

void DX12State::begin_frame()
{
  // Cycle to next frame
  m_frame_index = (m_frame_index + 1) % k_num_frames_in_flight;

  // Wait for this frame's previous work to complete
  m_graphics_queue->wait_for_fence_value(m_frame_fence_values[m_frame_index].m_graphics_queue_fence);
  m_copy_queue->wait_for_fence_value(m_frame_fence_values[m_frame_index].m_copy_queue_fence);

  process_destructions(m_frame_index);

  m_upload_contexts[m_frame_index]->reset();
}

void DX12State::end_frame()
{
  m_upload_contexts[m_frame_index]->process_uploads();
  submit_context(m_upload_contexts[m_frame_index].get());

  m_frame_fence_values[m_frame_index].m_copy_queue_fence = m_copy_queue->signal_fence();
}

void DX12State::present()
{
  UINT sync_interval = m_vsync ? 1 : 0;
  UINT present_flags = (m_tearing_supported && !m_vsync) ? DXGI_PRESENT_ALLOW_TEARING : 0;
  
  check_hresult(m_swap_chain->Present(sync_interval, present_flags));

  m_frame_fence_values[m_frame_index].m_graphics_queue_fence = m_graphics_queue->signal_fence();
}

void DX12State::resize(u32 width, u32 height)
{
  // Wait for all pending GPU work to complete before resizing
  m_graphics_queue->flush();
  m_copy_queue->flush();
  
  // Release back buffers and their descriptors
  for (auto& buffer : m_back_buffers)
  {
    if (buffer && buffer->m_rtv_descriptor.is_valid())
    {
      m_rtv_staging_descriptor_heap->destroy_descriptor(buffer->m_rtv_descriptor);
    }
    buffer.reset();
  }
  
  width = ZV::max(width, 1u);
  height = ZV::max(height, 1u);

  // Resize swap chain
  check_hresult(m_swap_chain->ResizeBuffers(
    k_num_back_buffers, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 
    m_tearing_supported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0));
  
  update_render_target_views();
  create_depth_stencil_buffer(width, height);
  if (m_msaa_enabled)
  {
    create_msaa_render_target(width, height);
  }
}

void DX12State::create_depth_stencil_buffer(u32 width, u32 height)
{
  if (m_depth_stencil_buffer)
  {
    destroy_texture_resource(move_ptr(m_depth_stencil_buffer));
  }

  DX12TextureResource::Desc depth_buffer_desc;
  depth_buffer_desc.m_format = DXGI_FORMAT_D32_FLOAT;
  depth_buffer_desc.m_width = width;
  depth_buffer_desc.m_height = height;
  depth_buffer_desc.m_view_flags.set(DX12TextureViewFlags::SRV);
  depth_buffer_desc.m_view_flags.set(DX12TextureViewFlags::DSV);
  depth_buffer_desc.m_sample_desc.Count = m_msaa_enabled ? 4 : 1;
  depth_buffer_desc.m_sample_desc.Quality = m_msaa_enabled ? m_msaa_quality_level - 1 : 0;
  depth_buffer_desc.m_is_msaa_enabled = m_msaa_enabled;
  depth_buffer_desc.m_flags = m_msaa_enabled ? D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL : D3D12_RESOURCE_FLAG_NONE;

  m_depth_stencil_buffer = create_texture_resource(depth_buffer_desc);
}

void DX12State::create_msaa_render_target(u32 width, u32 height)
{
  if (m_msaa_render_target)
  {
    destroy_texture_resource(move_ptr(m_msaa_render_target));
  }

  DX12TextureResource::Desc msaa_render_target_desc;
  msaa_render_target_desc.m_format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
  msaa_render_target_desc.m_width = width;
  msaa_render_target_desc.m_height = height;
  msaa_render_target_desc.m_view_flags.set(DX12TextureViewFlags::SRV);
  msaa_render_target_desc.m_view_flags.set(DX12TextureViewFlags::RTV);
  msaa_render_target_desc.m_sample_desc.Count = 4;
  msaa_render_target_desc.m_sample_desc.Quality = m_msaa_quality_level - 1;
  msaa_render_target_desc.m_is_msaa_enabled = true;
  msaa_render_target_desc.m_flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

  m_msaa_render_target = create_texture_resource(msaa_render_target_desc);
}

UniquePtr<DX12PipelineState> DX12State::create_graphics_pipeline(const DX12PipelineState::Desc& desc)
{
  UniquePtr<DX12PipelineState> new_pipeline = make_unique_ptr<DX12PipelineState>();
  new_pipeline->m_type = DX12PipelineStateType::Graphics;

  ID3D12RootSignature* root_sig = create_root_signature(desc.m_spaces, new_pipeline->m_resource_mapping);
  new_pipeline->m_root_signature.reset(root_sig);

  // Load shaders (you'll need to compile these first)
  ID3DBlob* vs_blob;
  ID3DBlob* ps_blob;
  check_hresult(D3DReadFileToBlob(desc.m_vs_path, &vs_blob));
  check_hresult(D3DReadFileToBlob(desc.m_ps_path, &ps_blob));

  // Pipeline state desc
  D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc{};
  pso_desc.NodeMask = 0;
  pso_desc.SampleMask = 0xFFFFFFFF;
  pso_desc.PrimitiveTopologyType = desc.m_topology;
  pso_desc.InputLayout.pInputElementDescs = nullptr;
  pso_desc.InputLayout.NumElements = 0;
  pso_desc.RasterizerState = desc.m_rasterizer_desc;
  pso_desc.BlendState = desc.m_blend_desc;
  pso_desc.SampleDesc = desc.m_sample_desc;
  pso_desc.DepthStencilState = desc.m_depth_stencil_desc;
  pso_desc.DSVFormat = desc.m_render_target_desc.m_depth_stencil_format;

  pso_desc.pRootSignature = root_sig;
  pso_desc.VS = CD3DX12_SHADER_BYTECODE(vs_blob);
  pso_desc.PS = CD3DX12_SHADER_BYTECODE(ps_blob);

  pso_desc.NumRenderTargets = desc.m_render_target_desc.m_num_render_targets;
  for (u32 rtv_index = 0; rtv_index < pso_desc.NumRenderTargets; rtv_index++)
  {
    pso_desc.RTVFormats[rtv_index] = desc.m_render_target_desc.m_render_target_formats[rtv_index];
  }
  
  if (desc.m_input_layout.m_num_elements > 0)
  {
    pso_desc.InputLayout = { desc.m_input_layout.m_elements.data(), desc.m_input_layout.m_num_elements };
  }
  else
  {
    pso_desc.InputLayout = { nullptr, 0 };
  }

  ID3D12PipelineState* pso;
  check_hresult(m_device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso)));

  new_pipeline->m_pso.reset(pso);

  return new_pipeline;
}

UniquePtr<DX12BufferResource> DX12State::create_buffer_resource(const DX12BufferResource::Desc& desc)
{
  const u32 size = align_u32(static_cast<u32>(desc.m_size), 256);

  D3D12_RESOURCE_DESC resource_desc = {};
  resource_desc.Width = size;
  resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  resource_desc.Alignment = 0;
  resource_desc.Height = 1;
  resource_desc.DepthOrArraySize = 1;
  resource_desc.MipLevels = 1;
  resource_desc.Format = DXGI_FORMAT_UNKNOWN;
  resource_desc.SampleDesc.Count = 1;
  resource_desc.SampleDesc.Quality = 0;
  resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

  bool is_host_visible = desc.m_access == DX12ResourceAccess::HostWritable;

  D3D12_HEAP_TYPE heap_type = is_host_visible ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_DEFAULT;
  CD3DX12_HEAP_PROPERTIES heap_props(heap_type);

  D3D12_RESOURCE_STATES resource_state = D3D12_RESOURCE_STATE_COPY_DEST;
  if (is_host_visible)
  {
    resource_state = D3D12_RESOURCE_STATE_GENERIC_READ;
  }

  if (desc.m_buffer_type == DX12BufferResource::Desc::BufferType::VertexBuffer)
  {
    resource_state = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
  }
  else if (desc.m_buffer_type == DX12BufferResource::Desc::BufferType::IndexBuffer)
  {
    resource_state = D3D12_RESOURCE_STATE_INDEX_BUFFER;
  }

  D3D12_HEAP_FLAGS heap_flag = D3D12_HEAP_FLAG_NONE;

  ID3D12Resource* resource;
  check_hresult(m_device->CreateCommittedResource(
    &heap_props,
    heap_flag,
    &resource_desc,
    resource_state,
    nullptr,
    IID_PPV_ARGS(&resource)));

  UniquePtr<DX12BufferResource> buffer = make_unique_ptr<DX12BufferResource>();
  buffer->m_size = size;
  buffer->m_state = resource_state;
  buffer->m_resource.reset(resource);
  buffer->m_gpu_address = resource->GetGPUVirtualAddress();
  buffer->m_stride = desc.m_stride;

  if (desc.m_view_flags.is_set(DX12BufferViewFlags::CBV))
  {
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc = {};
    cbv_desc.BufferLocation = buffer->m_resource->GetGPUVirtualAddress();
    cbv_desc.SizeInBytes = static_cast<uint32_t>(buffer->m_size);

    buffer->m_cbv_descriptor = m_srv_staging_descriptor_heap->create_descriptor();
    m_device->CreateConstantBufferView(&cbv_desc, buffer->m_cbv_descriptor.m_cpu_handle);
  }

  if (desc.m_view_flags.is_set(DX12BufferViewFlags::SRV))
  {
    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_desc.Format = desc.m_is_raw_access ? DXGI_FORMAT_R32_TYPELESS : DXGI_FORMAT_UNKNOWN;
    srv_desc.Buffer.FirstElement = 0;
    srv_desc.Buffer.NumElements = static_cast<uint32_t>(desc.m_is_raw_access ? (desc.m_size / 4) : buffer->m_size);
    srv_desc.Buffer.StructureByteStride = desc.m_is_raw_access ? 0 : buffer->m_stride;
    srv_desc.Buffer.Flags = desc.m_is_raw_access ? D3D12_BUFFER_SRV_FLAG_RAW : D3D12_BUFFER_SRV_FLAG_NONE;

    buffer->m_srv_descriptor = m_srv_staging_descriptor_heap->create_descriptor();
    m_device->CreateShaderResourceView(buffer->m_resource.get(), &srv_desc, buffer->m_srv_descriptor.m_cpu_handle);

    buffer->m_descriptor_heap_index = m_free_reserved_descriptor_indices.back();
    m_free_reserved_descriptor_indices.pop_back();

    copy_srv_handle_to_reserved_table(buffer->m_srv_descriptor, buffer->m_descriptor_heap_index);
  }

  if (desc.m_view_flags.is_set(DX12BufferViewFlags::UAV))
  {
    D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
    uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uav_desc.Format = desc.m_is_raw_access ? DXGI_FORMAT_R32_TYPELESS : DXGI_FORMAT_UNKNOWN;
    uav_desc.Buffer.CounterOffsetInBytes = 0;
    uav_desc.Buffer.FirstElement = 0;
    uav_desc.Buffer.NumElements = static_cast<uint32_t>(desc.m_is_raw_access ? (desc.m_size / 4) : buffer->m_size);
    uav_desc.Buffer.StructureByteStride = desc.m_is_raw_access ? 0 : buffer->m_stride;
    uav_desc.Buffer.Flags = desc.m_is_raw_access ? D3D12_BUFFER_UAV_FLAG_RAW : D3D12_BUFFER_UAV_FLAG_NONE;

    buffer->m_uav_descriptor = m_srv_staging_descriptor_heap->create_descriptor();
    m_device->CreateUnorderedAccessView(buffer->m_resource.get(), nullptr, &uav_desc, buffer->m_uav_descriptor.m_cpu_handle);
  }

  if (is_host_visible)
  {
    buffer->m_resource->Map(0, nullptr, reinterpret_cast<void**>(&buffer->m_mapped_data));
  }

  return buffer;
}

UniquePtr<DX12TextureResource> DX12State::create_texture_resource(const DX12TextureResource::Desc &desc)
{
  D3D12_RESOURCE_DESC texture_desc{};
  texture_desc.Dimension = desc.m_dimension;
  texture_desc.Alignment = desc.m_alignment;
  texture_desc.Width = desc.m_width;
  texture_desc.Height = desc.m_height;
  texture_desc.DepthOrArraySize = desc.m_depth_or_array_size;
  texture_desc.MipLevels = desc.m_mip_levels;
  texture_desc.Format = desc.m_format;
  texture_desc.SampleDesc = desc.m_sample_desc;
  texture_desc.Layout = desc.m_layout;
  texture_desc.Flags = desc.m_flags;

  bool has_rtv = desc.m_view_flags.is_set(DX12TextureViewFlags::RTV);
  bool has_dsv = desc.m_view_flags.is_set(DX12TextureViewFlags::DSV);
  bool has_srv = desc.m_view_flags.is_set(DX12TextureViewFlags::SRV);
  bool has_uav = desc.m_view_flags.is_set(DX12TextureViewFlags::UAV);

  D3D12_RESOURCE_STATES resource_state = D3D12_RESOURCE_STATE_COPY_DEST;
  DXGI_FORMAT resource_format = texture_desc.Format;
  DXGI_FORMAT shader_resource_view_format = texture_desc.Format;

  if (has_rtv)
  {
    texture_desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    resource_state = D3D12_RESOURCE_STATE_RENDER_TARGET;
  }

  if (has_dsv)
  {
    switch (desc.m_format)
    {
    case DXGI_FORMAT_D16_UNORM:
    {
      resource_format = DXGI_FORMAT_R16_TYPELESS;
      shader_resource_view_format = DXGI_FORMAT_R16_UNORM;
      break;
    }
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
    {
      resource_format = DXGI_FORMAT_R24G8_TYPELESS;
      shader_resource_view_format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
      break;
    }
    case DXGI_FORMAT_D32_FLOAT:
    {
      resource_format = DXGI_FORMAT_R32_TYPELESS;
      shader_resource_view_format = DXGI_FORMAT_R32_FLOAT;
      break;
    }
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
    {
      resource_format = DXGI_FORMAT_R32G8X24_TYPELESS;
      shader_resource_view_format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
      break;
    }
    default:
    {
      zv_fatal("Bad depth stencil format.");
      break;
    }
    }

    texture_desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    resource_state = D3D12_RESOURCE_STATE_DEPTH_WRITE;
  }

  if (has_uav)
  {
    texture_desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    resource_state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  }

  texture_desc.Format = resource_format;

  UniquePtr<DX12TextureResource> texture = make_unique_ptr<DX12TextureResource>();
  texture->m_state = resource_state;
  // texture->m_desc = texture_desc;

  D3D12_CLEAR_VALUE clear_value = {};
  clear_value.Format = desc.m_format;

  if (has_dsv)
  {
    clear_value.DepthStencil.Depth = 1.0f;
  }

  D3D12_HEAP_TYPE heap_type = desc.m_access == DX12ResourceAccess::HostWritable ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_DEFAULT;
  CD3DX12_HEAP_PROPERTIES heap_props(heap_type);

  ID3D12Resource *resource;
  check_hresult(m_device->CreateCommittedResource(
      &heap_props,
      D3D12_HEAP_FLAG_NONE,
      &texture_desc,
      resource_state,
      (!has_rtv && !has_dsv) ? nullptr : &clear_value,
      IID_PPV_ARGS(&resource)));
  texture->m_resource.reset(resource);

  if (has_srv)
  {
    texture->m_srv_descriptor = m_srv_staging_descriptor_heap->create_descriptor();

    if (has_dsv)
    {
      D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
      srv_desc.Format = shader_resource_view_format;
      srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
      srv_desc.Texture2D.MipLevels = 1;
      srv_desc.Texture2D.MostDetailedMip = 0;
      srv_desc.Texture2D.PlaneSlice = 0;
      srv_desc.Texture2D.ResourceMinLODClamp = 0.0f;
      srv_desc.ViewDimension = desc.m_is_msaa_enabled ? D3D12_SRV_DIMENSION_TEXTURE2DMS : D3D12_SRV_DIMENSION_TEXTURE2D;

      m_device->CreateShaderResourceView(texture->m_resource.get(), &srv_desc, texture->m_srv_descriptor.m_cpu_handle);
    }
    else
    {
      D3D12_SHADER_RESOURCE_VIEW_DESC *srv_desc_pointer = nullptr;
      D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
      bool is_cube_map = desc.m_dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D && desc.m_depth_or_array_size == 6;

      if (is_cube_map)
      {
        srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv_desc.TextureCube.MostDetailedMip = 0;
        srv_desc.TextureCube.MipLevels = desc.m_mip_levels;
        srv_desc.TextureCube.ResourceMinLODClamp = 0.0f;
        srv_desc_pointer = &srv_desc;
      }

      m_device->CreateShaderResourceView(texture->m_resource.get(), srv_desc_pointer, texture->m_srv_descriptor.m_cpu_handle);
    }

    texture->m_descriptor_heap_index = m_free_reserved_descriptor_indices.back();
    m_free_reserved_descriptor_indices.pop_back();

    copy_srv_handle_to_reserved_table(texture->m_srv_descriptor, texture->m_descriptor_heap_index);
  }

  if (has_rtv)
  {
    texture->m_rtv_descriptor = m_rtv_staging_descriptor_heap->create_descriptor();
    m_device->CreateRenderTargetView(texture->m_resource.get(), nullptr, texture->m_rtv_descriptor.m_cpu_handle);
  }

  if (has_dsv)
  {
    D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc = {};
    dsv_desc.Flags = D3D12_DSV_FLAG_NONE;
    dsv_desc.Format = desc.m_format;
    dsv_desc.Texture2D.MipSlice = 0;
    dsv_desc.ViewDimension = desc.m_is_msaa_enabled ? D3D12_DSV_DIMENSION_TEXTURE2DMS : D3D12_DSV_DIMENSION_TEXTURE2D;

    texture->m_dsv_descriptor = m_dsv_staging_descriptor_heap->create_descriptor();
    m_device->CreateDepthStencilView(texture->m_resource.get(), &dsv_desc, texture->m_dsv_descriptor.m_cpu_handle);
  }

  if (has_uav)
  {
    texture->m_uav_descriptor = m_srv_staging_descriptor_heap->create_descriptor();
    m_device->CreateUnorderedAccessView(texture->m_resource.get(), nullptr, nullptr, texture->m_uav_descriptor.m_cpu_handle);
  }

  // newTexture->mIsReady = (hasRTV || hasDSV);

  return texture;
}

UniquePtr<DX12GraphicsCommandContext> DX12State::create_graphics_context()
{
  return make_unique_ptr<DX12GraphicsCommandContext>(this);
}

void DX12State::submit_context(DX12CommandContext* context)
{
  switch (context->get_type())
  {
    case D3D12_COMMAND_LIST_TYPE_DIRECT:
    {
      m_graphics_queue->execute_command_list(context->get_command_list());
      break;
    }
    case D3D12_COMMAND_LIST_TYPE_COPY:
    {
      m_copy_queue->execute_command_list(context->get_command_list());
      break;
    }
    default:
      break;
  }
}

void DX12State::copy_srv_handle_to_reserved_table(DX12Descriptor srv_handle, u32 index)
{
  for (u32 i = 0; i < k_num_frames_in_flight; ++i)
  {
    DX12Descriptor target_descriptor = m_srv_render_pass_descriptor_heaps[i]->get_reserved_descriptor(index);

    m_device->CopyDescriptorsSimple(1, target_descriptor.m_cpu_handle, srv_handle.m_cpu_handle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  }
}

void DX12State::destroy_buffer_resource(UniquePtr<DX12BufferResource> buffer)
{
  m_destruction_queues[m_frame_index].m_buffers_to_destroy.push_back(move_ptr(buffer));
}

void DX12State::destroy_texture_resource(UniquePtr<DX12TextureResource> texture)
{
  m_destruction_queues[m_frame_index].m_textures_to_destroy.push_back(move_ptr(texture));
}

void DX12State::destroy_pipeline_state(UniquePtr<DX12PipelineState> pso)
{
  m_destruction_queues[m_frame_index].m_pipelines_to_destroy.push_back(move_ptr(pso));
}

void DX12State::destroy_context(UniquePtr<DX12CommandContext> context)
{
  m_destruction_queues[m_frame_index].m_contexts_to_destroy.push_back(move_ptr(context));
}

void DX12State::process_destructions(u32 frame_index)
{
  auto& destruction_queue = m_destruction_queues[frame_index];

  for(auto& buffer_to_destroy : destruction_queue.m_buffers_to_destroy)
  {
    if (buffer_to_destroy->m_cbv_descriptor.is_valid())
    {
      m_srv_staging_descriptor_heap->destroy_descriptor(buffer_to_destroy->m_cbv_descriptor);
    }

    if (buffer_to_destroy->m_srv_descriptor.is_valid())
    {
      m_srv_staging_descriptor_heap->destroy_descriptor(buffer_to_destroy->m_srv_descriptor);
      m_free_reserved_descriptor_indices.push_back(buffer_to_destroy->m_descriptor_heap_index);
    }

    if (buffer_to_destroy->m_uav_descriptor.is_valid())
    {
      m_srv_staging_descriptor_heap->destroy_descriptor(buffer_to_destroy->m_uav_descriptor);
    }

    if (buffer_to_destroy->m_mapped_data != nullptr)
    {
      buffer_to_destroy->m_resource->Unmap(0, nullptr);
    }

    buffer_to_destroy->m_resource.reset();
  }

  for (auto& texture_to_destroy : destruction_queue.m_textures_to_destroy)
  {
    if (texture_to_destroy->m_rtv_descriptor.is_valid())
    {
      m_rtv_staging_descriptor_heap->destroy_descriptor(texture_to_destroy->m_rtv_descriptor);
    }

    if (texture_to_destroy->m_dsv_descriptor.is_valid())
    {
      m_dsv_staging_descriptor_heap->destroy_descriptor(texture_to_destroy->m_dsv_descriptor);
    }

    if (texture_to_destroy->m_srv_descriptor.is_valid())
    {
      m_srv_staging_descriptor_heap->destroy_descriptor(texture_to_destroy->m_srv_descriptor);
      m_free_reserved_descriptor_indices.push_back(texture_to_destroy->m_descriptor_heap_index);
    }

    if (texture_to_destroy->m_uav_descriptor.is_valid())
    {
      m_srv_staging_descriptor_heap->destroy_descriptor(texture_to_destroy->m_uav_descriptor);
    }

    texture_to_destroy->m_resource.reset();
  }

  for (auto& pipeline_to_destroy : destruction_queue.m_pipelines_to_destroy)
  {
    pipeline_to_destroy->m_root_signature.reset();
    pipeline_to_destroy->m_pso.reset();
  }

  destruction_queue.m_buffers_to_destroy.clear();
  destruction_queue.m_textures_to_destroy.clear();
  destruction_queue.m_pipelines_to_destroy.clear();
  destruction_queue.m_contexts_to_destroy.clear();
}

UniquePtr<DX12TextureData> DX12State::create_texture_data(TextureAsset* texture_asset)
{
  D3D12_RESOURCE_DIMENSION dimension = static_cast<D3D12_RESOURCE_DIMENSION>(texture_asset->m_dimension);
  bool is_3d_texture = dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D;
  DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

  if (texture_asset->m_format == TextureFormat::Linear)
  {
    if (texture_asset->m_num_channels == 1)
    {
      format = DXGI_FORMAT_R8_UNORM;
    }
    else if (texture_asset->m_num_channels == 2)
    {
      format = DXGI_FORMAT_R8G8_UNORM;
    }
    else if (texture_asset->m_num_channels >= 3)
    {
      format = DXGI_FORMAT_R8G8B8A8_UNORM;
    }
    else
    {
      zv_assert_msg(false, "Invalid number of channels");
    }
  }
  else
  {
    zv_assert_msg(texture_asset->m_num_channels >= 3, "SRGB texture requires at least 3 channels");
    format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
  }

  DX12TextureResource::Desc desc;
  desc.m_format = format;
  desc.m_width = texture_asset->m_width;
  desc.m_height = texture_asset->m_height;
  desc.m_flags = D3D12_RESOURCE_FLAG_NONE;
  desc.m_depth_or_array_size = static_cast<UINT16>(is_3d_texture ? texture_asset->m_depth : texture_asset->m_array_size);
  desc.m_mip_levels = static_cast<UINT16>(texture_asset->m_mip_levels);
  desc.m_sample_desc.Count = 1;
  desc.m_sample_desc.Quality = 0;
  desc.m_dimension = is_3d_texture ? D3D12_RESOURCE_DIMENSION_TEXTURE3D : D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  desc.m_layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
  desc.m_alignment = 0;
  desc.m_view_flags = DX12TextureViewFlags::SRV;

  UniquePtr<DX12TextureData> texture_data = make_unique_ptr<DX12TextureData>();
  texture_data->m_texture_resource = create_texture_resource(desc);

  // Calculate subresource layouts using GetCopyableFootprints
  D3D12_RESOURCE_DESC resource_desc = {};
  resource_desc.Dimension = desc.m_dimension;
  resource_desc.Width = desc.m_width;
  resource_desc.Height = desc.m_height;
  resource_desc.DepthOrArraySize = desc.m_depth_or_array_size;
  resource_desc.MipLevels = desc.m_mip_levels;
  resource_desc.Format = desc.m_format;
  resource_desc.SampleDesc = desc.m_sample_desc;
  resource_desc.Layout = desc.m_layout;
  resource_desc.Flags = desc.m_flags;

  u32 num_sub_resources = desc.m_mip_levels * desc.m_depth_or_array_size;
  DX12SubResourceLayouts sub_resource_layouts = {};
  u32 num_rows[k_max_texture_subresource_count] = {};
  u64 row_sizes_in_bytes[k_max_texture_subresource_count] = {};
  u64 total_size = 0;

  m_device->GetCopyableFootprints(
    &resource_desc, 
    0, 
    num_sub_resources, 
    0, 
    sub_resource_layouts.data(), 
    num_rows, 
    row_sizes_in_bytes, 
    &total_size
  );

  // Create properly laid out texture data
  texture_data->m_texture_data = make_unique_ptr<u8[]>(total_size);
  texture_data->m_size = total_size;
  texture_data->m_num_sub_resources = num_sub_resources;
  texture_data->m_sub_resource_layouts = sub_resource_layouts;

  for (u32 array_index = 0; array_index < desc.m_depth_or_array_size; array_index++)
  {
    for (u32 mip_index = 0; mip_index < desc.m_mip_levels; mip_index++)
    {
      const u32 sub_resource_index = mip_index + (array_index * desc.m_mip_levels);
      
      const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& sub_resource_layout = sub_resource_layouts[sub_resource_index];
      const u32 sub_resource_height = num_rows[sub_resource_index];
      const u32 sub_resource_pitch = align_u32(sub_resource_layout.Footprint.RowPitch, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
      const u32 sub_resource_depth = sub_resource_layout.Footprint.Depth;
      u8* dest_sub_resource_memory = texture_data->m_texture_data.get() + sub_resource_layout.Offset;

      // For now, we'll handle the simple case where we have one subresource
      // In a full implementation, you'd need to calculate the source data for each mip level
      if (sub_resource_index == 0) // First/only subresource
      {
        const u8* src_data = texture_asset->m_data.get();
        const u32 src_pitch = texture_asset->m_width * texture_asset->m_num_channels;
        
        for (u32 slice_index = 0; slice_index < sub_resource_depth; slice_index++)
        {
          for (u32 height = 0; height < sub_resource_height; height++)
          {
            // Copy the row data, handling pitch differences
            const u32 copy_size = ZV::min(sub_resource_pitch, src_pitch);
            memcpy(dest_sub_resource_memory, src_data, copy_size);
            dest_sub_resource_memory += sub_resource_pitch;
            src_data += src_pitch;
          }
        }
      }
      else
      {
        // For additional mip levels, you would need to generate the downsampled data
        // This is a simplified version - in practice you'd need proper mip generation
        // For now, we'll just zero out additional mip levels
        memset(dest_sub_resource_memory, 0, sub_resource_pitch * sub_resource_height * sub_resource_depth);
      }
    }
  }

  return texture_data;
}

StaticArray<const CD3DX12_STATIC_SAMPLER_DESC, 6> DX12State::get_static_samplers() const
{
	const CD3DX12_STATIC_SAMPLER_DESC point_wrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC point_clamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linear_wrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linear_clamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropic_wrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		16);                              // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropic_clamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		16);                               // maxAnisotropy

	return { 
		point_wrap, point_clamp,
		linear_wrap, linear_clamp, 
		anisotropic_wrap, anisotropic_clamp };
}

ID3D12RootSignature* DX12State::create_root_signature(
  const DX12PipelineResourceSpaces& spaces, 
  DX12PipelineResourceMapping& resource_mapping)
{
  DynamicArray<D3D12_ROOT_PARAMETER1> root_parameters;
  StaticArray<DynamicArray<D3D12_DESCRIPTOR_RANGE1>, DX12ResourceSpace::NumSpaces> desciptor_ranges;

  for (u32 space_id = 0; space_id < DX12ResourceSpace::NumSpaces; ++space_id)
  {
    DX12PipelineResourceSpace* current_space = spaces[space_id];
    DynamicArray<D3D12_DESCRIPTOR_RANGE1>& current_descriptor_range = desciptor_ranges[space_id];

    if (current_space)
    {
      const DX12BufferResource *cbv = current_space->get_cbv();
      const DynamicArray<DX12PipelineResourceBinding>& uavs = current_space->get_uavs();
      const DynamicArray<DX12PipelineResourceBinding>& srvs = current_space->get_srvs();

      if (cbv)
      {
        D3D12_ROOT_PARAMETER1 root_parameter{};
        root_parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        root_parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        root_parameter.Descriptor.RegisterSpace = space_id;
        root_parameter.Descriptor.ShaderRegister = 0;

        resource_mapping.m_cbv_mapping[space_id] = static_cast<u32>(root_parameters.size());
        root_parameters.push_back(root_parameter);
      }

      if (uavs.empty() && srvs.empty())
      {
        continue;
      }

      for (auto &uav : uavs)
      {
        D3D12_DESCRIPTOR_RANGE1 range{};
        range.BaseShaderRegister = uav.m_binding_index;
        range.NumDescriptors = 1;
        range.OffsetInDescriptorsFromTableStart = static_cast<u32>(current_descriptor_range.size());
        range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        range.RegisterSpace = space_id;
        range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;

        current_descriptor_range.push_back(range);
      }

      for (auto &srv : srvs)
      {
        D3D12_DESCRIPTOR_RANGE1 range{};
        range.BaseShaderRegister = srv.m_binding_index;
        range.NumDescriptors = 1;
        range.OffsetInDescriptorsFromTableStart = static_cast<u32>(current_descriptor_range.size());
        range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
        range.RegisterSpace = space_id;

        current_descriptor_range.push_back(range);
      }

      D3D12_ROOT_PARAMETER1 descriptor_table_for_space{};
      descriptor_table_for_space.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
      descriptor_table_for_space.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
      descriptor_table_for_space.DescriptorTable.NumDescriptorRanges = static_cast<u32>(current_descriptor_range.size());
      descriptor_table_for_space.DescriptorTable.pDescriptorRanges = current_descriptor_range.data();

      resource_mapping.m_table_mapping[space_id] = static_cast<u32>(root_parameters.size());
      root_parameters.push_back(descriptor_table_for_space);
    }
  }

  auto static_samplers = get_static_samplers();

  D3D12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc{};
  root_signature_desc.Desc_1_1.NumParameters = static_cast<u32>(root_parameters.size());
  root_signature_desc.Desc_1_1.pParameters = root_parameters.data();
  root_signature_desc.Desc_1_1.NumStaticSamplers = (UINT)static_samplers.size();
  root_signature_desc.Desc_1_1.pStaticSamplers = static_samplers.data();
  root_signature_desc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
  //root_signature_desc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED | D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED;
  root_signature_desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;

  ID3DBlob* root_signature_blob = nullptr;
  ID3DBlob* error_blob = nullptr;
  HRESULT hr = D3D12SerializeVersionedRootSignature(&root_signature_desc, &root_signature_blob, &error_blob);

  if (error_blob != nullptr)
  {
    zv_error("Failed to serialize root signature: {}", (char*)error_blob->GetBufferPointer());
  }
  check_hresult(hr);

  ID3D12RootSignature* root_signature = nullptr;
  check_hresult(m_device->CreateRootSignature(0, root_signature_blob->GetBufferPointer(), root_signature_blob->GetBufferSize(), IID_PPV_ARGS(&root_signature)));

  return root_signature;
}

//-------------------------------
//  DX12CommandQueue
//-------------------------------

DX12CommandQueue::DX12CommandQueue(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type)
{
  D3D12_COMMAND_QUEUE_DESC desc = {};
  desc.Type = type;
  desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
  desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
  desc.NodeMask = 0;

  ID3D12CommandQueue* queue;
  check_hresult(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&queue)));
  m_queue.reset(queue);

  ID3D12Fence* fence;
  check_hresult(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
  m_fence.reset(fence);

  m_fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  zv_assert(m_fence_event);
}

DX12CommandQueue::~DX12CommandQueue()
{
  if (m_fence_event)
  {
    CloseHandle(m_fence_event);
  }

  m_fence = nullptr;
  m_queue = nullptr;
}

void DX12CommandQueue::flush()
{
  wait_for_fence_value(m_next_fence_value - 1);
}

bool DX12CommandQueue::is_fence_complete(u64 fence_value)
{
  if (fence_value > m_last_completed_fence_value)
  {
    poll_current_fence_value();
  }

  return fence_value <= m_last_completed_fence_value;
}

void DX12CommandQueue::wait_for_fence_value(u64 fence_value)
{
  if (is_fence_complete(fence_value))
  {
    return;
  }

  {
    ScopedLock lock_guard(m_event_mutex);

    check_hresult(m_fence->SetEventOnCompletion(fence_value, m_fence_event));
    WaitForSingleObjectEx(m_fence_event, INFINITE, false);
    m_last_completed_fence_value = fence_value;
  }
}

u64 DX12CommandQueue::poll_current_fence_value()
{
  m_last_completed_fence_value = ZV::max(m_last_completed_fence_value, m_fence->GetCompletedValue());
  return m_last_completed_fence_value;
}

u64 DX12CommandQueue::execute_command_list(ID3D12GraphicsCommandList* command_list)
{
  check_hresult(command_list->Close());
  
  ID3D12CommandList* command_lists[] = { command_list };
  m_queue->ExecuteCommandLists(1, command_lists);
  
  return signal_fence();
}

u64 DX12CommandQueue::signal_fence()
{
  ScopedLock lock_guard(m_fence_mutex);

  m_queue->Signal(m_fence.get(), m_next_fence_value);

  return m_next_fence_value++;
}

//-------------------------------
//  DX12Context Implementation
//-------------------------------

DX12CommandContext::DX12CommandContext(DX12State* dx12_state, D3D12_COMMAND_LIST_TYPE type)
  : m_dx12_state(dx12_state)
  , m_type(type)
  , m_num_pending_barriers(0)
{
  for (u32 i = 0; i < k_num_frames_in_flight; ++i)
  {
    ID3D12CommandAllocator* allocator;
    check_hresult(m_dx12_state->get_device()->CreateCommandAllocator(type, IID_PPV_ARGS(&allocator)));
    m_allocators[i].reset(allocator);
  }

  ID3D12GraphicsCommandList* command_list;
  check_hresult(m_dx12_state->get_device()->CreateCommandList1(0, type, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&command_list)));
  m_command_list.reset(command_list);
}

DX12CommandContext::~DX12CommandContext()
{
}

void DX12CommandContext::reset()
{
  u32 frame_id = m_dx12_state->get_frame_id();

  check_hresult(m_allocators[frame_id]->Reset());
  check_hresult(m_command_list->Reset(m_allocators[frame_id].get(), nullptr));

  if (m_type != D3D12_COMMAND_LIST_TYPE_COPY)
  {
    m_srv_render_pass_descriptor_heap = m_dx12_state->get_srv_heap(frame_id);
    m_srv_render_pass_descriptor_heap->reset();

    ID3D12DescriptorHeap* heaps_to_bind[1];
    heaps_to_bind[0] = m_dx12_state->get_srv_heap(frame_id)->get_heap();
    // heaps_to_bind[1] = m_dx12_state->get_sampler_heap().get_heap();

    m_command_list->SetDescriptorHeaps(1, heaps_to_bind);
  }
}

void DX12CommandContext::add_barrier(DX12Resource* resource, D3D12_RESOURCE_STATES new_state)
{
  if (m_num_pending_barriers >= k_max_resource_barriers)
  {
    flush_barriers();
  }

  D3D12_RESOURCE_STATES old_state = resource->m_state;

  if (m_type == D3D12_COMMAND_LIST_TYPE_COMPUTE)
  {
    constexpr D3D12_RESOURCE_STATES k_valid_compute_context_states = (D3D12_RESOURCE_STATE_UNORDERED_ACCESS | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE |
                                                                      D3D12_RESOURCE_STATE_COPY_DEST | D3D12_RESOURCE_STATE_COPY_SOURCE);

    check_hresult((old_state & k_valid_compute_context_states) == old_state);
    check_hresult((new_state & k_valid_compute_context_states) == new_state);
  }

  if (old_state != new_state)
  {
    D3D12_RESOURCE_BARRIER &barrier_desc = m_pending_barriers[m_num_pending_barriers];
    m_num_pending_barriers++;

    barrier_desc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier_desc.Transition.pResource = resource->m_resource.get();
    barrier_desc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier_desc.Transition.StateBefore = old_state;
    barrier_desc.Transition.StateAfter = new_state;
    barrier_desc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

    resource->m_state = new_state;
  }
  else if (new_state == D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
  {
    D3D12_RESOURCE_BARRIER &barrier_desc = m_pending_barriers[m_num_pending_barriers];
    m_num_pending_barriers++;

    barrier_desc.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier_desc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier_desc.UAV.pResource = resource->m_resource.get();
  }
}

void DX12CommandContext::flush_barriers()
{
  if (m_num_pending_barriers > 0)
  {
    m_command_list->ResourceBarrier(m_num_pending_barriers, m_pending_barriers.data());
    m_num_pending_barriers = 0;
  }
}

//-------------------------------
//  DX12GraphicsContext Implementation
//-------------------------------

DX12GraphicsCommandContext::DX12GraphicsCommandContext(DX12State* dx12_state)
  : DX12CommandContext(dx12_state, D3D12_COMMAND_LIST_TYPE_DIRECT)
{
}

void DX12GraphicsCommandContext::set_render_targets(u32 num_rtvs, const D3D12_CPU_DESCRIPTOR_HANDLE* rtvs, D3D12_CPU_DESCRIPTOR_HANDLE dsv)
{
  m_command_list->OMSetRenderTargets(num_rtvs, rtvs, false, dsv.ptr != 0 ? &dsv : nullptr);
}

void DX12GraphicsCommandContext::set_viewport_and_scissor(u32 width, u32 height)
{
  D3D12_VIEWPORT viewport = {};
  viewport.Width = static_cast<f32>(width);
  viewport.Height = static_cast<f32>(height);
  viewport.MinDepth = 0.0f;
  viewport.MaxDepth = 1.0f;
  viewport.TopLeftX = 0.0f;
  viewport.TopLeftY = 0.0f;

  D3D12_RECT scissor = {};
  scissor.right = width;
  scissor.bottom = height;

  m_command_list->RSSetViewports(1, &viewport);
  m_command_list->RSSetScissorRects(1, &scissor);
}

void DX12GraphicsCommandContext::clear_render_target(DX12TextureResource* target, f32 color[4])
{
  m_command_list->ClearRenderTargetView(target->m_rtv_descriptor.m_cpu_handle, color, 0, nullptr);
}

void DX12GraphicsCommandContext::clear_depth_stencil_target(DX12TextureResource* target, f32 depth, u8 stencil)
{
  m_command_list->ClearDepthStencilView(target->m_dsv_descriptor.m_cpu_handle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, depth, stencil, 0, nullptr);
}

void DX12GraphicsCommandContext::set_pipeline(const DX12PipelineInfo& pipeline_info)
{
  const bool pipeline_expected_bound_externally = !pipeline_info.m_pipeline; //imgui

  if (!pipeline_expected_bound_externally)
  {
    m_command_list->SetPipelineState(pipeline_info.m_pipeline->m_pso.get());
    m_command_list->SetGraphicsRootSignature(pipeline_info.m_pipeline->m_root_signature.get());
  }

  m_current_pipeline = pipeline_info.m_pipeline;

  D3D12_CPU_DESCRIPTOR_HANDLE render_target_handles[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT]{};
  D3D12_CPU_DESCRIPTOR_HANDLE depth_stencil_handle{ 0 };

  size_t render_target_count = pipeline_info.m_render_targets.size();

  for (size_t target_index = 0; target_index < render_target_count; target_index++)
  {
    render_target_handles[target_index] = pipeline_info.m_render_targets[target_index]->m_rtv_descriptor.m_cpu_handle;
  }

  if (pipeline_info.m_depth_stencil_target)
  {
    depth_stencil_handle = pipeline_info.m_depth_stencil_target->m_dsv_descriptor.m_cpu_handle;
  }

  set_render_targets(static_cast<u32>(render_target_count), render_target_handles, depth_stencil_handle);
}

void DX12GraphicsCommandContext::set_pipeline_resources(u32 space, DX12PipelineResourceSpace* resources)
{
  zv_assert(m_current_pipeline);
  zv_assert(resources->is_locked());

  static const u32 k_max_handles_per_binding = 16;
  static const u32 k_single_descriptor_range_copy_array[k_max_handles_per_binding]{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 ,1 };
  
  const DX12BufferResource* cbv = resources->get_cbv();
  const auto& uavs = resources->get_uavs();
  const auto& srvs = resources->get_srvs();
  const u32 num_table_handles = static_cast<u32>(uavs.size() + srvs.size());
  D3D12_CPU_DESCRIPTOR_HANDLE handles[k_max_handles_per_binding]{};
  u32 current_handle_index = 0;
  zv_assert(num_table_handles <= k_max_handles_per_binding);

  if (cbv)
  {
      auto cbv_mapping = m_current_pipeline->m_resource_mapping.m_cbv_mapping[space];
      // zv_assert(cbv_mapping.has_value());

      m_command_list->SetGraphicsRootConstantBufferView(cbv_mapping, cbv->m_gpu_address);

      // switch (m_current_pipeline->m_type)
      // {
      // case PipelineType::graphics:
      //     mCommandList->SetGraphicsRootConstantBufferView(cbvMapping.value(), cbv->mVirtualAddress);
      //     break;
      // case PipelineType::compute:
      //     mCommandList->SetComputeRootConstantBufferView(cbvMapping.value(), cbv->mVirtualAddress);
      //     break;
      // default:
      //     assert(false);
      //     break;
      // }
  }

  if (num_table_handles == 0)
  {
    return;
  }

  for (auto &uav : uavs)
  {
    if (uav.m_resource->m_type == DX12ResourceType::Buffer)
    {
      handles[current_handle_index++] = static_cast<DX12BufferResource *>(uav.m_resource)->m_uav_descriptor.m_cpu_handle;
    }
    else
    {
      handles[current_handle_index++] = static_cast<DX12TextureResource *>(uav.m_resource)->m_uav_descriptor.m_cpu_handle;
    }
  }

  for (auto &srv : srvs)
  {
    if (srv.m_resource->m_type == DX12ResourceType::Buffer)
    {
      handles[current_handle_index++] = static_cast<DX12BufferResource *>(srv.m_resource)->m_srv_descriptor.m_cpu_handle;
    }
    else
    {
      handles[current_handle_index++] = static_cast<DX12TextureResource *>(srv.m_resource)->m_srv_descriptor.m_cpu_handle;
    }
  }

  DX12Descriptor block_start = m_srv_render_pass_descriptor_heap->allocate_user_descriptor_block(num_table_handles);
  m_dx12_state->get_device()->CopyDescriptors(1, &block_start.m_cpu_handle, &num_table_handles, num_table_handles, handles, k_single_descriptor_range_copy_array, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

  auto table_mapping = m_current_pipeline->m_resource_mapping.m_table_mapping[space];
  // zv_assert(table_mapping.has_value());

  m_command_list->SetGraphicsRootDescriptorTable(table_mapping, block_start.m_gpu_handle);

  // switch (m_current_pipeline->m_type)
  // {
  // case PipelineType::graphics:
  //     mCommandList->SetGraphicsRootDescriptorTable(tableMapping.value(), blockStart.mGPUHandle);
  //     break;
  // case PipelineType::compute:
  //     mCommandList->SetComputeRootDescriptorTable(tableMapping.value(), blockStart.mGPUHandle);
  //     break;
  // default:
  //     assert(false);
  //     break;
  // }
}

void DX12GraphicsCommandContext::set_vertex_buffer(const DX12BufferResource* vertex_buffer)
{
  D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view = {};
  vertex_buffer_view.BufferLocation = vertex_buffer->m_gpu_address;
  vertex_buffer_view.StrideInBytes = vertex_buffer->m_stride;
  vertex_buffer_view.SizeInBytes = vertex_buffer->m_size;
  
  m_command_list->IASetVertexBuffers(0, 1, &vertex_buffer_view);
}

void DX12GraphicsCommandContext::set_index_buffer(const DX12BufferResource* index_buffer)
{
    D3D12_INDEX_BUFFER_VIEW index_buffer_view;
    index_buffer_view.Format = index_buffer->m_stride == 4 ? DXGI_FORMAT_R32_UINT : index_buffer->m_stride == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_UNKNOWN;
    index_buffer_view.SizeInBytes = static_cast<uint32_t>(index_buffer->m_size);
    index_buffer_view.BufferLocation = index_buffer->m_resource->GetGPUVirtualAddress();

    m_command_list->IASetIndexBuffer(&index_buffer_view);
}

void DX12GraphicsCommandContext::set_primitive_topology(D3D12_PRIMITIVE_TOPOLOGY topology)
{
  m_command_list->IASetPrimitiveTopology(topology);
}

void DX12GraphicsCommandContext::resolve_msaa_render_target(DX12TextureResource* msaa_rt, DX12TextureResource* current_back_buffer)
{
  m_command_list->ResolveSubresource(current_back_buffer->m_resource.get(), 0, msaa_rt->m_resource.get(), 0, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
}

void DX12GraphicsCommandContext::draw_indexed(u32 index_count, u32 start_index, u32 base_vertex)
{
  m_command_list->DrawIndexedInstanced(index_count, 1, start_index, base_vertex, 0);
}

void DX12GraphicsCommandContext::draw(u32 vertex_count, u32 start_vertex)
{
  m_command_list->DrawInstanced(vertex_count, 1, start_vertex, 0);
}

//-------------------------------
//  DX12UploadContext Implementation
//-------------------------------

DX12UploadCommandContext::DX12UploadCommandContext(DX12State *dx12_state)
    : DX12CommandContext(dx12_state, D3D12_COMMAND_LIST_TYPE_COPY)
{
  DX12BufferResource::Desc buffer_upload_heap_desc{};
  buffer_upload_heap_desc.m_size = k_buffer_upload_heap_size;
  buffer_upload_heap_desc.m_access = DX12ResourceAccess::HostWritable;

  m_buffer_upload_heap = move_ptr(m_dx12_state->create_buffer_resource(buffer_upload_heap_desc));

  DX12BufferResource::Desc texture_upload_heap_desc{};
  texture_upload_heap_desc.m_size = k_texture_upload_heap_size;
  texture_upload_heap_desc.m_access = DX12ResourceAccess::HostWritable;

  m_texture_upload_heap = move_ptr(m_dx12_state->create_buffer_resource(texture_upload_heap_desc));
}

void DX12UploadCommandContext::record_buffer_upload(DX12BufferResource* dest_buffer, const void* data, u32 size)
{
  BufferUpload upload = {};
  upload.m_dest_buffer = dest_buffer->m_resource.get();
  upload.m_data = data;
  upload.m_size = size;

  m_pending_buffer_uploads.emplace_back(upload);
}

void DX12UploadCommandContext::record_texture_upload(DX12TextureData* texture_data)
{
  TextureUpload upload = {};
  upload.m_dest_texture = texture_data->m_texture_resource->m_resource.get();
  upload.m_data = texture_data->m_texture_data.get();
  upload.m_size = texture_data->m_size;
  upload.m_num_sub_resources = texture_data->m_num_sub_resources;
  upload.m_sub_resource_layouts = texture_data->m_sub_resource_layouts;

  m_pending_texture_uploads.emplace_back(upload);
}

void DX12UploadCommandContext::process_uploads()
{
  process_buffer_uploads();
  process_texture_uploads();
}

UniquePtr<DX12BufferResource> DX12UploadCommandContext::return_buffer_heap()
{
  return move_ptr(m_buffer_upload_heap);
}

UniquePtr<DX12BufferResource> DX12UploadCommandContext::return_texture_heap()
{
  return move_ptr(m_texture_upload_heap);
}

void DX12UploadCommandContext::process_buffer_uploads()
{
  if (m_pending_buffer_uploads.empty())
  {
    return;
  }

  u64 buffer_upload_heap_offset = 0;
  size_t processed = 0;

  for (; processed < m_pending_buffer_uploads.size(); ++processed)
  {
      const auto& current_upload = m_pending_buffer_uploads[processed];

      // Drop impossible uploads so they don't block forever
      if (current_upload.m_size > m_buffer_upload_heap->m_size)
      {
        zv_warning(
          "Single buffer upload ({} bytes) exceeds heap size ({}). Skipping.",
          static_cast<unsigned long long>(current_upload.m_size),
          static_cast<unsigned long long>(m_buffer_upload_heap->m_size)
        );
        continue;
      }

      if ((buffer_upload_heap_offset + current_upload.m_size) > m_buffer_upload_heap->m_size)
      {
          zv_warning("Buffer upload heap is full. Pushing remaining uploads to next frame.");
          break;
      }

      memcpy(m_buffer_upload_heap->m_mapped_data + buffer_upload_heap_offset, current_upload.m_data, current_upload.m_size);

      m_command_list->CopyBufferRegion(
          current_upload.m_dest_buffer, 0,
          m_buffer_upload_heap->m_resource.get(), buffer_upload_heap_offset,
          current_upload.m_size);

      buffer_upload_heap_offset += current_upload.m_size;
  }

  m_pending_buffer_uploads.erase(m_pending_buffer_uploads.begin(), m_pending_buffer_uploads.begin() + processed);
}

void DX12UploadCommandContext::process_texture_uploads()
{
    if (m_pending_texture_uploads.empty())
    {
      return;
    }

    u64 texture_upload_heap_offset = 0;
    std::size_t processed = 0;

    for (; processed < m_pending_texture_uploads.size(); ++processed)
    {
        TextureUpload& current_upload = m_pending_texture_uploads[processed];

        // Drop impossible uploads so they don't block forever
        if (current_upload.m_size > m_texture_upload_heap->m_size)
        {
            zv_warning(
                "Single texture upload ({} bytes) exceeds heap size ({}). Skipping.",
                static_cast<unsigned long long>(current_upload.m_size),
                static_cast<unsigned long long>(m_texture_upload_heap->m_size)
            );
            continue;
        }

        if ((texture_upload_heap_offset + current_upload.m_size) > m_texture_upload_heap->m_size)
        {
            zv_warning("Texture upload heap is full. Pushing remaining uploads to next frame.");
            break;
        }

        memcpy(m_texture_upload_heap->m_mapped_data + texture_upload_heap_offset, current_upload.m_data, current_upload.m_size);

        // Copy texture subresources
        for (u32 j = 0; j < current_upload.m_num_sub_resources; ++j)
        {
            D3D12_TEXTURE_COPY_LOCATION dest_location = {};
            dest_location.pResource = current_upload.m_dest_texture;
            dest_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            dest_location.SubresourceIndex = j;

            D3D12_TEXTURE_COPY_LOCATION source_location = {};
            source_location.pResource = m_texture_upload_heap->m_resource.get();
            source_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            source_location.PlacedFootprint = current_upload.m_sub_resource_layouts[j];
            source_location.PlacedFootprint.Offset += texture_upload_heap_offset;

            m_command_list->CopyTextureRegion(&dest_location, 0, 0, 0, &source_location, nullptr);
        }

        texture_upload_heap_offset += current_upload.m_size;
        texture_upload_heap_offset = align_u64(texture_upload_heap_offset, 512);
    }

    // Remove only uploads we handled (or explicitly skipped as too large)
    if (processed > 0)
    {
        m_pending_texture_uploads.erase(
            m_pending_texture_uploads.begin(),
            m_pending_texture_uploads.begin() + static_cast<std::ptrdiff_t>(processed));
    }
}

//-------------------------------
//  DX12DescriptorHeap Implementation
//-------------------------------

DX12DescriptorHeap::DX12DescriptorHeap(ID3D12Device *device, D3D12_DESCRIPTOR_HEAP_TYPE heap_type, u32 num_descriptors, bool is_shader_visible)
    : m_heap_type(heap_type), m_max_descriptors(num_descriptors), m_is_shader_visible(is_shader_visible)
{
  D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
  heap_desc.NumDescriptors = m_max_descriptors;
  heap_desc.Type = m_heap_type;
  heap_desc.Flags = m_is_shader_visible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  heap_desc.NodeMask = 0;

  ID3D12DescriptorHeap* descriptor_heap;
  check_hresult(device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&descriptor_heap)));
  m_descriptor_heap.reset(descriptor_heap);

  m_heap_start.m_cpu_handle = m_descriptor_heap->GetCPUDescriptorHandleForHeapStart();

  if (m_is_shader_visible)
  {
    m_heap_start.m_gpu_handle = m_descriptor_heap->GetGPUDescriptorHandleForHeapStart();
  }

  m_descriptor_size = device->GetDescriptorHandleIncrementSize(m_heap_type);
}

DX12DescriptorHeap::~DX12DescriptorHeap()
{
}

//-------------------------------
//  DX12StagingDescriptorHeap Implementation
//-------------------------------

DX12StagingDescriptorHeap::DX12StagingDescriptorHeap(ID3D12Device *device, D3D12_DESCRIPTOR_HEAP_TYPE heap_type, u32 num_descriptors)
    : DX12DescriptorHeap(device, heap_type, num_descriptors, false)
{
  m_free_descriptors.reserve(num_descriptors);
}

DX12StagingDescriptorHeap::~DX12StagingDescriptorHeap()
{
  if (m_active_handle_count != 0)
  {
    zv_fatal("There were active handles when the descriptor heap was destroyed. Look for leaks.");
  }
}

DX12Descriptor DX12StagingDescriptorHeap::create_descriptor()
{
  ScopedLock lock_guard(m_usage_mutex);

  u32 new_handle_id = 0;

  if (m_current_descriptor_index < m_max_descriptors)
  {
    new_handle_id = m_current_descriptor_index;
    m_current_descriptor_index++;
  }
  else if (m_free_descriptors.size() > 0)
  {
    new_handle_id = m_free_descriptors.back();
    m_free_descriptors.pop_back();
  }
  else
  {
    zv_fatal("Ran out of dynamic descriptor heap handles, need to increase heap size.");
  }

  DX12Descriptor new_descriptor;
  D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle = m_heap_start.m_cpu_handle;
  cpu_handle.ptr += static_cast<uint64_t>(new_handle_id) * m_descriptor_size;
  new_descriptor.m_cpu_handle = cpu_handle;
  new_descriptor.m_heap_index = new_handle_id;

  m_active_handle_count++;

  return new_descriptor;
}

void DX12StagingDescriptorHeap::destroy_descriptor(DX12Descriptor descriptor)
{
  ScopedLock lock_guard(m_usage_mutex);

  m_free_descriptors.push_back(descriptor.m_heap_index);

  if (m_active_handle_count == 0)
  {
    zv_fatal("Freeing heap handles when there should be none left");
    return;
  }

  m_active_handle_count--;
}

//-------------------------------
//  DX12RenderPassDescriptorHeap Implementation
//-------------------------------

DX12RenderPassDescriptorHeap::DX12RenderPassDescriptorHeap(
  ID3D12Device *device, 
  D3D12_DESCRIPTOR_HEAP_TYPE heap_type, 
  u32 reserved_count, 
  u32 user_count)
    : DX12DescriptorHeap(device, heap_type, reserved_count + user_count, true)
    , m_reserved_handle_count(reserved_count)
    , m_current_descriptor_index(reserved_count)
{
}

DX12Descriptor DX12RenderPassDescriptorHeap::get_reserved_descriptor(u32 index)
{
  zv_assert(index < m_reserved_handle_count);

  D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle = m_heap_start.m_cpu_handle;
  D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle = m_heap_start.m_gpu_handle;
  cpu_handle.ptr += static_cast<uint64_t>(index) * m_descriptor_size;
  gpu_handle.ptr += static_cast<uint64_t>(index) * m_descriptor_size;

  DX12Descriptor descriptor;
  descriptor.m_heap_index = index;
  descriptor.m_cpu_handle = cpu_handle;
  descriptor.m_gpu_handle = gpu_handle;

  return descriptor;
}

DX12Descriptor DX12RenderPassDescriptorHeap::allocate_user_descriptor_block(u32 count)
{
  u32 new_handle_id = 0;

  {
    ScopedLock lock_guard(m_usage_mutex);

    u32 block_end = m_current_descriptor_index + count;

    if (block_end <= m_max_descriptors)
    {
      new_handle_id = m_current_descriptor_index;
      m_current_descriptor_index = block_end;
    }
    else
    {
      zv_fatal("Ran out of render pass descriptor heap handles, need to increase heap size.");
    }
  }

  DX12Descriptor new_descriptor;
  new_descriptor.m_heap_index = new_handle_id;

  D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle = m_heap_start.m_cpu_handle;
  cpu_handle.ptr += static_cast<uint64_t>(new_handle_id) * m_descriptor_size;
  new_descriptor.m_cpu_handle = cpu_handle;

  D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle = m_heap_start.m_gpu_handle;
  gpu_handle.ptr += static_cast<uint64_t>(new_handle_id) * m_descriptor_size;
  new_descriptor.m_gpu_handle = gpu_handle;

  return new_descriptor;
}

void DX12RenderPassDescriptorHeap::reset()
{
  m_current_descriptor_index = m_reserved_handle_count;
}

//-------------------------------
//  DX12PipelineResourceSpace Implementation
//-------------------------------

bool sort_pipeline_bindings(DX12PipelineResourceBinding a, DX12PipelineResourceBinding b)
{
  return a.m_binding_index < b.m_binding_index;
}

void DX12PipelineResourceSpace::set_cbv(DX12BufferResource* resource)
{
  if (m_is_locked)
  {
    if (m_cbv == nullptr)
    {
      zv_error("Setting unused binding in a locked resource space");
    }
    else
    {
      m_cbv = resource;
    }
  }
  else
  {
    m_cbv = resource;
  }
}

void DX12PipelineResourceSpace::set_srv(const DX12PipelineResourceBinding& binding)
{
  u32 current_index = get_index_of_binding_index(m_srvs, binding.m_binding_index);

  if (m_is_locked)
  {
    if (current_index == UINT_MAX)
    {
      zv_error("Setting unused binding in a locked resource space");
    }
    else
    {
      m_srvs[current_index] = binding;
    }
  }
  else
  {
    if (current_index == UINT_MAX)
    {
      m_srvs.push_back(binding);
      sort_container(m_srvs.begin(), m_srvs.end(), sort_pipeline_bindings);
    }
    else
    {
      m_srvs[current_index] = binding;
    }
  }
}

void DX12PipelineResourceSpace::set_uav(const DX12PipelineResourceBinding& binding)
{
  u32 current_index = get_index_of_binding_index(m_uavs, binding.m_binding_index);

  if (m_is_locked)
  {
    if (current_index == UINT_MAX)
    {
      zv_error("Setting unused binding in a locked resource space");
    }
    else
    {
      m_uavs[current_index] = binding;
    }
  }
  else
  {
    if (current_index == UINT_MAX)
    {
      m_uavs.push_back(binding);
      sort_container(m_uavs.begin(), m_uavs.end(), sort_pipeline_bindings);
    }
    else
    {
      m_uavs[current_index] = binding;
    }
  }
}

void DX12PipelineResourceSpace::lock()
{
  m_is_locked = true;
}

u32 DX12PipelineResourceSpace::get_index_of_binding_index(const DynamicArray<DX12PipelineResourceBinding>& bindings, u32 binding_index)
{
  const u32 num_bindings = static_cast<u32>(bindings.size());
  for (u32 vector_index = 0; vector_index < num_bindings; vector_index++)
  {
    if (bindings.at(vector_index).m_binding_index == binding_index)
    {
      return vector_index;
    }
  }

  return UINT_MAX;
}
