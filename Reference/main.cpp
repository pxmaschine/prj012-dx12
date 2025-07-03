#include "D3D12Lite.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx12.h"
#include "Shaders/Shared.h"

using namespace D3D12Lite;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WndProc(HWND hwnd, UINT umessage, WPARAM wparam, LPARAM lparam)
{
    if (ImGui_ImplWin32_WndProcHandler(hwnd, umessage, wparam, lparam))
    {
        return true;
    }

    switch (umessage)
    {
    case WM_KEYDOWN:
        if (wparam == VK_ESCAPE)
        {
            PostQuitMessage(0);
            return 0;
        }
        else
        {
            return DefWindowProc(hwnd, umessage, wparam, lparam);
        }

    case WM_DESTROY:
        [[fallthrough]];
    case WM_CLOSE:
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(hwnd, umessage, wparam, lparam);
    }
}

class Renderer
{
public:
    Renderer(HWND windowHandle, Uint2 screenSize)
    {
        mDevice = std::make_unique<Device>(windowHandle, screenSize);
        mGraphicsContext = mDevice->CreateGraphicsContext();

        InitializeTriangleResources();
        
        InitializeMeshResources();

        InitializeImGui(windowHandle);
    }

    ~Renderer()
    {
        mDevice->WaitForIdle();
        mDevice->DestroyShader(std::move(mTriangleVertexShader));
        mDevice->DestroyShader(std::move(mTrianglePixelShader));
        mDevice->DestroyPipelineStateObject(std::move(mTrianglePSO));
        mDevice->DestroyBuffer(std::move(mTriangleVertexBuffer));
        mDevice->DestroyBuffer(std::move(mTriangleConstantBuffer));


        mDevice->DestroyTexture(std::move(mDepthBuffer));
        mDevice->DestroyTexture(std::move(mWoodTexture));
        mDevice->DestroyBuffer(std::move(mMeshVertexBuffer));
        mDevice->DestroyBuffer(std::move(mMeshPassConstantBuffer));
        
        mDevice->DestroyShader(std::move(mMeshVertexShader));
        mDevice->DestroyShader(std::move(mMeshPixelShader));
        mDevice->DestroyPipelineStateObject(std::move(mMeshPSO));

        for (uint32_t frameIndex = 0; frameIndex < NUM_FRAMES_IN_FLIGHT; frameIndex++)
        {
            mDevice->DestroyBuffer(std::move(mMeshConstantBuffers[frameIndex]));
        }

        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();

        mDevice->DestroyContext(std::move(mGraphicsContext));
        mDevice = nullptr;
    }

    void InitializeTriangleResources()
    {
        std::array<TriangleVertex, 3> vertices;
        vertices[0].position = { -0.5f, -0.5f };
        vertices[0].color = { 1.0f, 0.0f, 0.0f };
        vertices[1].position = { 0.0f, 0.5f };
        vertices[1].color = { 0.0f, 1.0f, 0.0f };
        vertices[2].position = { 0.5f, -0.5f };
        vertices[2].color = { 0.0f, 0.0f, 1.0f };

        BufferCreationDesc triangleBufferDesc{};
        triangleBufferDesc.mSize = sizeof(vertices);
        triangleBufferDesc.mAccessFlags = BufferAccessFlags::hostWritable;
        triangleBufferDesc.mViewFlags = BufferViewFlags::srv;
        triangleBufferDesc.mStride = sizeof(TriangleVertex);
        triangleBufferDesc.mIsRawAccess = true;

        mTriangleVertexBuffer = mDevice->CreateBuffer(triangleBufferDesc);
        mTriangleVertexBuffer->SetMappedData(&vertices, sizeof(vertices));

        BufferCreationDesc triangleConstantDesc{};
        triangleConstantDesc.mSize = sizeof(TriangleConstants);
        triangleConstantDesc.mAccessFlags = BufferAccessFlags::hostWritable;
        triangleConstantDesc.mViewFlags = BufferViewFlags::cbv;

        TriangleConstants triangleConstants;
        triangleConstants.vertexBufferIndex = mTriangleVertexBuffer->mDescriptorHeapIndex;

        mTriangleConstantBuffer = mDevice->CreateBuffer(triangleConstantDesc);
        mTriangleConstantBuffer->SetMappedData(&triangleConstants, sizeof(TriangleConstants));

        ShaderCreationDesc triangleShaderVSDesc;
        triangleShaderVSDesc.mShaderName = L"Triangle.hlsl";
        triangleShaderVSDesc.mEntryPoint = L"VertexShader";
        triangleShaderVSDesc.mType = ShaderType::vertex;

        ShaderCreationDesc triangleShaderPSDesc;
        triangleShaderPSDesc.mShaderName = L"Triangle.hlsl";
        triangleShaderPSDesc.mEntryPoint = L"PixelShader";
        triangleShaderPSDesc.mType = ShaderType::pixel;

        mTriangleVertexShader = mDevice->CreateShader(triangleShaderVSDesc);
        mTrianglePixelShader = mDevice->CreateShader(triangleShaderPSDesc);

        mTrianglePerObjectSpace.SetCBV(mTriangleConstantBuffer.get());
        mTrianglePerObjectSpace.Lock();

        PipelineResourceLayout resourceLayout;
        resourceLayout.mSpaces[PER_OBJECT_SPACE] = &mTrianglePerObjectSpace;

        GraphicsPipelineDesc trianglePipelineDesc = GetDefaultGraphicsPipelineDesc();
        trianglePipelineDesc.mVertexShader = mTriangleVertexShader.get();
        trianglePipelineDesc.mPixelShader = mTrianglePixelShader.get();
        trianglePipelineDesc.mRenderTargetDesc.mNumRenderTargets = 1;
        trianglePipelineDesc.mRenderTargetDesc.mRenderTargetFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

        mTrianglePSO = mDevice->CreateGraphicsPipeline(trianglePipelineDesc, resourceLayout);
    }

    void InitializeMeshResources()
    {
        MeshVertex meshVertices[36] = {
            {{1.0f, -1.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},
            {{1.0f, -1.0f, -1.0f}, {1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
            {{-1.0f, -1.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},
            {{-1.0f, -1.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},
            {{1.0f, -1.0f, -1.0f}, {1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
            {{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
            {{1.0f, 1.0f, -1.0f}, {1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
            {{1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
            {{-1.0f, 1.0f, -1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
            {{-1.0f, 1.0f, -1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
            {{1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
            {{-1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
            {{-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},
            {{-1.0f, 1.0f, -1.0f}, {1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
            {{-1.0f, -1.0f, 1.0f}, {0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},
            {{-1.0f, -1.0f, 1.0f}, {0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},
            {{-1.0f, 1.0f, -1.0f}, {1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
            {{-1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
            {{-1.0f, -1.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
            {{-1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
            {{1.0f, -1.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
            {{1.0f, -1.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
            {{-1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
            {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
            {{1.0f, -1.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
            {{1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
            {{1.0f, -1.0f, -1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
            {{1.0f, -1.0f, -1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
            {{1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
            {{1.0f, 1.0f, -1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
            {{1.0f, -1.0f, -1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
            {{1.0f, 1.0f, -1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
            {{-1.0f, -1.0f, -1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
            {{-1.0f, -1.0f, -1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
            {{1.0f, 1.0f, -1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
            {{-1.0f, 1.0f, -1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
        };

        BufferCreationDesc meshVertexBufferDesc{};
        meshVertexBufferDesc.mSize = sizeof(meshVertices);
        meshVertexBufferDesc.mAccessFlags = BufferAccessFlags::gpuOnly;
        meshVertexBufferDesc.mViewFlags = BufferViewFlags::srv;
        meshVertexBufferDesc.mStride = sizeof(MeshVertex);
        meshVertexBufferDesc.mIsRawAccess = true;

        mMeshVertexBuffer = mDevice->CreateBuffer(meshVertexBufferDesc);

        auto bufferUpload = std::make_unique<BufferUpload>();
        bufferUpload->mBuffer = mMeshVertexBuffer.get();
        bufferUpload->mBufferData = std::make_unique<uint8_t[]>(sizeof(meshVertices));
        bufferUpload->mBufferDataSize = sizeof(meshVertices);

        memcpy_s(bufferUpload->mBufferData.get(), sizeof(meshVertices), meshVertices, sizeof(meshVertices));

        mDevice->GetUploadContextForCurrentFrame().AddBufferUpload(std::move(bufferUpload));

        mWoodTexture = mDevice->CreateTextureFromFile("Wood.dds");

        BufferCreationDesc meshConstantDesc{};
        meshConstantDesc.mSize = sizeof(MeshConstants);
        meshConstantDesc.mAccessFlags = BufferAccessFlags::hostWritable;
        meshConstantDesc.mViewFlags = BufferViewFlags::cbv;

        for (uint32_t frameIndex = 0; frameIndex < NUM_FRAMES_IN_FLIGHT; frameIndex++)
        {
            mMeshConstantBuffers[frameIndex] = mDevice->CreateBuffer(meshConstantDesc);
        }

        BufferCreationDesc meshPassConstantDesc{};
        meshPassConstantDesc.mSize = sizeof(MeshPassConstants);
        meshPassConstantDesc.mAccessFlags = BufferAccessFlags::hostWritable;
        meshPassConstantDesc.mViewFlags = BufferViewFlags::cbv;

        Uint2 screenSize = mDevice->GetScreenSize();

        float fieldOfView = 3.14159f / 4.0f;
        float aspectRatio = (float)screenSize.x / (float)screenSize.y;
        Vector3 cameraPosition = Vector3(-3.0f, 3.0f, -8.0f);

        MeshPassConstants passConstants;
        passConstants.viewMatrix = Matrix::CreateLookAt(cameraPosition, Vector3(0, 0, 0), Vector3(0, 1, 0));
        passConstants.projectionMatrix = Matrix::CreatePerspectiveFieldOfView(fieldOfView, aspectRatio, 0.001f, 1000.0f);
        passConstants.cameraPosition = cameraPosition;

        mMeshPassConstantBuffer = mDevice->CreateBuffer(meshPassConstantDesc);
        mMeshPassConstantBuffer->SetMappedData(&passConstants, sizeof(MeshPassConstants));

        TextureCreationDesc depthBufferDesc;
        depthBufferDesc.mResourceDesc.Format = DXGI_FORMAT_D32_FLOAT;
        depthBufferDesc.mResourceDesc.Width = screenSize.x;
        depthBufferDesc.mResourceDesc.Height = screenSize.y;
        depthBufferDesc.mViewFlags = TextureViewFlags::srv | TextureViewFlags::dsv;

        mDepthBuffer = mDevice->CreateTexture(depthBufferDesc);

        ShaderCreationDesc meshShaderVSDesc;
        meshShaderVSDesc.mShaderName = L"Mesh.hlsl";
        meshShaderVSDesc.mEntryPoint = L"VertexShader";
        meshShaderVSDesc.mType = ShaderType::vertex;

        ShaderCreationDesc meshShaderPSDesc;
        meshShaderPSDesc.mShaderName = L"Mesh.hlsl";
        meshShaderPSDesc.mEntryPoint = L"PixelShader";
        meshShaderPSDesc.mType = ShaderType::pixel;

        mMeshVertexShader = mDevice->CreateShader(meshShaderVSDesc);
        mMeshPixelShader = mDevice->CreateShader(meshShaderPSDesc);

        GraphicsPipelineDesc meshPipelineDesc = GetDefaultGraphicsPipelineDesc();
        meshPipelineDesc.mVertexShader = mMeshVertexShader.get();
        meshPipelineDesc.mPixelShader = mMeshPixelShader.get();
        meshPipelineDesc.mRenderTargetDesc.mNumRenderTargets = 1;
        meshPipelineDesc.mRenderTargetDesc.mRenderTargetFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        meshPipelineDesc.mDepthStencilDesc.DepthEnable = true;
        meshPipelineDesc.mRenderTargetDesc.mDepthStencilFormat = depthBufferDesc.mResourceDesc.Format;
        meshPipelineDesc.mDepthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;

        mMeshPerObjectResourceSpace.SetCBV(mMeshConstantBuffers[0].get());
        mMeshPerObjectResourceSpace.Lock();

        mMeshPerPassResourceSpace.SetCBV(mMeshPassConstantBuffer.get());
        mMeshPerPassResourceSpace.Lock();

        PipelineResourceLayout meshResourceLayout;
        meshResourceLayout.mSpaces[PER_OBJECT_SPACE] = &mMeshPerObjectResourceSpace;
        meshResourceLayout.mSpaces[PER_PASS_SPACE] = &mMeshPerPassResourceSpace;

        mMeshPSO = mDevice->CreateGraphicsPipeline(meshPipelineDesc, meshResourceLayout);
    }

    void InitializeImGui(HWND windowHandle)
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        ImGui::StyleColorsDark();

        Descriptor descriptor = mDevice->GetImguiDescriptor(0);
        Descriptor descriptor2 = mDevice->GetImguiDescriptor(1);

        ImGui_ImplWin32_Init(windowHandle);
        ImGui_ImplDX12_Init(mDevice->GetDevice(), NUM_FRAMES_IN_FLIGHT,
            DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, nullptr,
            descriptor.mCPUHandle, descriptor.mGPUHandle, descriptor2.mCPUHandle, descriptor2.mGPUHandle);
    }

    void RenderClearColorTutorial()
    {
        mDevice->BeginFrame();

        TextureResource& backBuffer = mDevice->GetCurrentBackBuffer();

        mGraphicsContext->Reset();
        mGraphicsContext->AddBarrier(backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
        mGraphicsContext->FlushBarriers();

        mGraphicsContext->ClearRenderTarget(backBuffer, Color(0.3f, 0.3f, 0.8f));

        mGraphicsContext->AddBarrier(backBuffer, D3D12_RESOURCE_STATE_PRESENT);
        mGraphicsContext->FlushBarriers();

        mDevice->SubmitContextWork(*mGraphicsContext);

        mDevice->EndFrame();
        mDevice->Present();
    }

    void RenderTriangleTutorial()
    {
        mDevice->BeginFrame();

        TextureResource& backBuffer = mDevice->GetCurrentBackBuffer();

        mGraphicsContext->Reset();
        mGraphicsContext->AddBarrier(backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
        mGraphicsContext->FlushBarriers();

        mGraphicsContext->ClearRenderTarget(backBuffer, Color(0.3f, 0.3f, 0.8f));

        PipelineInfo pipeline;
        pipeline.mPipeline = mTrianglePSO.get();
        pipeline.mRenderTargets.push_back(&backBuffer);

        mGraphicsContext->SetPipeline(pipeline);
        mGraphicsContext->SetPipelineResources(PER_OBJECT_SPACE, mTrianglePerObjectSpace);
        mGraphicsContext->SetDefaultViewPortAndScissor(mDevice->GetScreenSize());
        mGraphicsContext->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        mGraphicsContext->Draw(3);

        mGraphicsContext->AddBarrier(backBuffer, D3D12_RESOURCE_STATE_PRESENT);
        mGraphicsContext->FlushBarriers();

        mDevice->SubmitContextWork(*mGraphicsContext);

        mDevice->EndFrame();
        mDevice->Present();
    }

    void RenderMeshTutorial()
    {
        mDevice->BeginFrame();

        TextureResource& backBuffer = mDevice->GetCurrentBackBuffer();

        mGraphicsContext->Reset();
        mGraphicsContext->AddBarrier(backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
        mGraphicsContext->AddBarrier(*mDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE);
        mGraphicsContext->FlushBarriers();

        mGraphicsContext->ClearRenderTarget(backBuffer, Color(0.3f, 0.3f, 0.8f));
        mGraphicsContext->ClearDepthStencilTarget(*mDepthBuffer, 1.0f, 0);

        static float rotation = 0.0f;
        rotation += 0.0001f;

        if (mMeshVertexBuffer->mIsReady && mWoodTexture->mIsReady)
        {
            MeshConstants meshConstants;
            meshConstants.vertexBufferIndex = mMeshVertexBuffer->mDescriptorHeapIndex;
            meshConstants.textureIndex = mWoodTexture->mDescriptorHeapIndex;
            meshConstants.worldMatrix = Matrix::CreateRotationY(rotation);

            mMeshConstantBuffers[mDevice->GetFrameId()]->SetMappedData(&meshConstants, sizeof(MeshConstants));

            mMeshPerObjectResourceSpace.SetCBV(mMeshConstantBuffers[mDevice->GetFrameId()].get());

            PipelineInfo pipeline;
            pipeline.mPipeline = mMeshPSO.get();
            pipeline.mRenderTargets.push_back(&backBuffer);
            pipeline.mDepthStencilTarget = mDepthBuffer.get();

            mGraphicsContext->SetPipeline(pipeline);
            mGraphicsContext->SetPipelineResources(PER_OBJECT_SPACE, mMeshPerObjectResourceSpace);
            mGraphicsContext->SetPipelineResources(PER_PASS_SPACE, mMeshPerPassResourceSpace);
            mGraphicsContext->SetDefaultViewPortAndScissor(mDevice->GetScreenSize());
            mGraphicsContext->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            mGraphicsContext->Draw(36);
        }

        mGraphicsContext->AddBarrier(backBuffer, D3D12_RESOURCE_STATE_PRESENT);
        mGraphicsContext->FlushBarriers();

        mDevice->SubmitContextWork(*mGraphicsContext);

        mDevice->EndFrame();
        mDevice->Present();
    }

    void RenderImGui()
    {
        mDevice->BeginFrame();

        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ImGui::ShowDemoWindow();
        ImGui::Render();

        TextureResource& backBuffer = mDevice->GetCurrentBackBuffer();

        mGraphicsContext->Reset();
        mGraphicsContext->AddBarrier(backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
        mGraphicsContext->FlushBarriers();

        mGraphicsContext->ClearRenderTarget(backBuffer, Color(0.3f, 0.3f, 0.8f));

        PipelineInfo pipeline;
        pipeline.mPipeline = nullptr;
        pipeline.mRenderTargets.push_back(&backBuffer);

        mGraphicsContext->SetPipeline(pipeline);
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), mGraphicsContext->GetCommandList());
        
        mGraphicsContext->AddBarrier(backBuffer, D3D12_RESOURCE_STATE_PRESENT);
        mGraphicsContext->FlushBarriers();

        mDevice->SubmitContextWork(*mGraphicsContext);

        mDevice->EndFrame();
        mDevice->Present();
    }

    void Render()
    {
        //RenderClearColorTutorial();
        //RenderTriangleTutorial();
        //RenderMeshTutorial();
        RenderImGui();
    }

private:
    //All
    std::unique_ptr<Device> mDevice;
    std::unique_ptr<GraphicsContext> mGraphicsContext;

    //Triangle
    std::unique_ptr<BufferResource> mTriangleVertexBuffer;
    std::unique_ptr<BufferResource> mTriangleConstantBuffer;
    std::unique_ptr<Shader> mTriangleVertexShader;
    std::unique_ptr<Shader> mTrianglePixelShader;
    std::unique_ptr<PipelineStateObject> mTrianglePSO;
    PipelineResourceSpace mTrianglePerObjectSpace;

    //Mesh
    std::unique_ptr<TextureResource> mDepthBuffer;
    std::unique_ptr<TextureResource> mWoodTexture;
    std::unique_ptr<BufferResource> mMeshVertexBuffer;
    std::array<std::unique_ptr<BufferResource>, NUM_FRAMES_IN_FLIGHT> mMeshConstantBuffers;
    std::unique_ptr<BufferResource> mMeshPassConstantBuffer;
    PipelineResourceSpace mMeshPerObjectResourceSpace;
    PipelineResourceSpace mMeshPerPassResourceSpace;
    std::unique_ptr<Shader> mMeshVertexShader;
    std::unique_ptr<Shader> mMeshPixelShader;
    std::unique_ptr<PipelineStateObject> mMeshPSO;
};

int main()
{
    std::wstring applicationName = L"D3D12 Tutorial";
    Uint2 windowSize = { 1920, 1080 };
    HINSTANCE moduleHandle = GetModuleHandle(nullptr);

    WNDCLASSEX wc = { 0 };
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = moduleHandle;
    wc.hIcon = LoadIcon(nullptr, IDI_WINLOGO);
    wc.hIconSm = wc.hIcon;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszMenuName = nullptr;
    wc.lpszClassName = applicationName.c_str();
    wc.cbSize = sizeof(WNDCLASSEX);
    RegisterClassEx(&wc);

    HWND windowHandle = CreateWindowEx(WS_EX_APPWINDOW, applicationName.c_str(), applicationName.c_str(),
        WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_OVERLAPPED | WS_SIZEBOX,
        (GetSystemMetrics(SM_CXSCREEN) - windowSize.x) / 2, (GetSystemMetrics(SM_CYSCREEN) - windowSize.y) / 2, windowSize.x, windowSize.y,
        nullptr, nullptr, moduleHandle, nullptr);

    ShowWindow(windowHandle, SW_SHOW);
    SetForegroundWindow(windowHandle);
    SetFocus(windowHandle);
    ShowCursor(true);

    std::unique_ptr<Renderer> renderer = std::make_unique<Renderer>(windowHandle, windowSize);

    bool shouldExit = false;
    while (!shouldExit)
    {
        MSG msg{ 0 };
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        if (msg.message == WM_QUIT)
        {
            shouldExit = true;
        }

        renderer->Render();
    }

    renderer = nullptr;

    DestroyWindow(windowHandle);
    windowHandle = nullptr;

    UnregisterClass(applicationName.c_str(), moduleHandle);
    moduleHandle = nullptr;

    return 0;
}