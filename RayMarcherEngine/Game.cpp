#include "pch.h"
#include "Game.h"

extern void ExitGame() noexcept;

using namespace DirectX;

using Microsoft::WRL::ComPtr;

Game::Game() noexcept :
    m_window(nullptr),
    m_outputWidth(1280),
    m_outputHeight(720),
    m_featureLevel(D3D_FEATURE_LEVEL_9_1)
{
}

// Initialize the Direct3D resources required to run.
void Game::Initialise(HWND window, int width, int height)
{
    m_window = window;
    m_outputWidth = std::max(width, 1);
    m_outputHeight = std::max(height, 1);

    CreateDevice();

    CreateResources();

    InitialiseImGui(window);

    CreateConstantBuffers();

    CreateCameras(width, height);
    
    CreateGameObjects();

    //m_timer.SetFixedTimeStep(true);
    //m_timer.SetTargetElapsedSeconds(1.0 / 60);
}

// Executes the basic game loop.
void Game::Tick()
{
    m_timer.Tick([&]()
    {
        Update(m_timer);
    });

    Render();
}

// Updates the world.
void Game::Update(DX::StepTimer const& timer)
{
    float elapsedTime = float(timer.GetElapsedSeconds());

    // TODO: Add your game logic here.
    m_gameObject->Update(elapsedTime, m_d3dContext.Get());
}

// Draws the scene.
void Game::Render()
{
    // Don't try to render anything before the first Update.
    if (m_timer.GetFrameCount() == 0)
    {
        return;
    }

    // Bind render target to intermediate RenderTargetView
    SetRenderTargetAndClear(m_rttRenderTargetView.Get(), m_rttDepthStencilView.Get());

    // Start the Dear ImGui frame
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // Create ImGui dockspace
    ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

    // get the game object world transform
    XMMATRIX mGO = XMLoadFloat4x4(m_gameObject->GetTransform());

    // Get the game object's shader
    Shader* shader = m_gameObject->GetShader().get();

    // Set active vertex layout
    m_d3dContext->IASetInputLayout(shader->GetVertexLayout().Get());

    // Update constant buffer
    static ConstantBuffer::RenderSettings rs;
    ImGui::Begin("Render Settings", (bool*)0, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::DragInt("Max Steps", (int*)&rs.maxSteps, 1.0f, 1, 1000);
        ImGui::DragFloat("Max Dist", &rs.maxDist, .5f, 1.0f, 10000.0f);
        ImGui::DragFloat("Threshold", &rs.intersectionThreshold, 0.0001f, 0.0001f, 0.3f);
    ImGui::End();

    static ConstantBuffer::WorldCamera cam;
    cam.position[0] = m_camera->GetCameraPosition().x;
    cam.position[1] = m_camera->GetCameraPosition().y;
    cam.position[2] = m_camera->GetCameraPosition().z;
    cam.fov = m_camera->GetFOV();
    cam.cameraType = (int)m_camera->GetCameraType();
    cam.resolution[0] = m_viewportSize.x;
    cam.resolution[1] = m_viewportSize.y;
    cam.view = m_camera->CalculateViewMatrix();

    static ConstantBuffer cb1;
    cb1.renderSettings = rs;
    cb1.camera = cam;
    ImGui::Begin("Scene Controls", (bool*)0, ImGuiWindowFlags_AlwaysAutoResize);
    for (int i = 0; i < OBJECT_COUNT; i++) {
        if (ImGui::CollapsingHeader((std::string("Object ") + std::to_string(i)).c_str())) {
            ConstantBuffer::WorldObject obj = cb1.object[i];

            ImGui::PushID(i);
            ImGui::Checkbox("IsActive", (bool*)&obj.isActive);
            ImGui::DragFloat3("Position", &obj.position[0], 0.015f);
            const char* items[] = { "Sphere", "Box", "Torus", "Cone", "Cylinder" };
            int selection = obj.objectType;
            ImGui::Combo("Camera Type", &selection, items, 5);
            obj.objectType = selection;
            ImGui::DragFloat3("Params", &obj.params[0], 0.015f);
            ImGui::PopID();

            cb1.object[i] = obj;
        }
    }
    ImGui::End();

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    ZeroMemory(&mappedResource, sizeof(D3D11_MAPPED_SUBRESOURCE));
    m_d3dContext->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    memcpy(mappedResource.pData, &cb1, sizeof(ConstantBuffer));
    //m_d3dContext->UpdateSubresource(m_constantBuffer.Get(), 0, nullptr, &cb1, 0, 0);
    m_d3dContext->Unmap(m_constantBuffer.Get(), 0);

    // Render the quad
    m_d3dContext->VSSetShader(shader->GetVertexShader().Get(), nullptr, 0);
    m_d3dContext->PSSetShader(shader->GetPixelShader().Get(), nullptr, 0);
    m_d3dContext->PSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());

    m_gameObject->Render(m_d3dContext.Get());

    // Bind render target to back buffer
    SetRenderTargetAndClear(m_renderTargetView.Get(), m_depthStencilView.Get());

    // Create content in ImGui window
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2());
    ImGui::Begin("Viewport");
        // Detect viewport window resize
        ImVec2 curVpSize = ImGui::GetContentRegionAvail();
        if (m_viewportSize.x != curVpSize.x || m_viewportSize.y != curVpSize.y) {
            m_viewportSize = curVpSize;
            OnViewportSizeChanged();
        }

        // Create SRV and render to ImGui window
        ComPtr<ID3D11Resource> resource;
        m_rttRenderTargetView->GetResource(resource.ReleaseAndGetAddressOf());
        ComPtr<ID3D11ShaderResourceView> srv;
        m_d3dDevice->CreateShaderResourceView(resource.Get(), nullptr, srv.GetAddressOf());
        ImGui::Image(srv.Get(), m_viewportSize);
    ImGui::End();
    ImGui::PopStyleVar();

    // Create frames per second window
    ImGui::Begin("FPS", (bool*)0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("%.3fms (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::End();

    ImGui::Begin("Camera Controls", (bool*)0, ImGuiWindowFlags_AlwaysAutoResize);
        m_camera->RenderGUIControls();
        m_gameObject->RenderGUIControls(m_d3dDevice.Get());
    ImGui::End();

    // Render ImGui
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    // Update and Render additional Platform Windows
    if (m_ioImGui->ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }

    Present();

}

// Helper method to clear the back buffers.
void Game::SetRenderTargetAndClear(ID3D11RenderTargetView* rtv, ID3D11DepthStencilView* dsv)
{
    // Clear the views.
    XMFLOAT3 camBgCol = m_camera->GetBackgroundColour();
    XMFLOAT4 clearColour = XMFLOAT4(camBgCol.x, camBgCol.y, camBgCol.z, 1.0f);
    m_d3dContext->ClearRenderTargetView(rtv, &clearColour.x);
    m_d3dContext->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    m_d3dContext->OMSetRenderTargets(1, &rtv, dsv);

    // Set the viewport.
    CD3D11_VIEWPORT viewport(0.0f, 0.0f, static_cast<float>(m_outputWidth), static_cast<float>(m_outputHeight));
    m_d3dContext->RSSetViewports(1, &viewport);
}

// Presents the back buffer contents to the screen.
void Game::Present()
{
    // The first argument instructs DXGI to block until VSync, putting the application
    // to sleep until the next VSync. This ensures we don't waste any cycles rendering
    // frames that will never be displayed to the screen.
    HRESULT hr = m_swapChain->Present(0, 0);

    // If the device was reset we must completely reinitialize the renderer.
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
    {
        OnDeviceLost();
    }
    else
    {
        DX::ThrowIfFailed(hr);
    }
}

// Message handlers
void Game::OnActivated()
{
    // TODO: Game is becoming active window.
}

void Game::OnDeactivated()
{
    // TODO: Game is becoming background window.
}

void Game::OnSuspending()
{
    // TODO: Game is being power-suspended (or minimized).
}

void Game::OnResuming()
{
    m_timer.ResetElapsedTime();

    // TODO: Game is being power-resumed (or returning from minimize).
}

void Game::OnWindowSizeChanged(int width, int height)
{
    m_outputWidth = std::max(width, 1);
    m_outputHeight = std::max(height, 1);

    CreateResources();

    // TODO: Game window is being resized.
}

void Game::OnViewportSizeChanged()
{
    if (m_camera)
        m_camera->SetAspectRatio(m_viewportSize.x / (float)m_viewportSize.y);

    CreateResources();
}

// Properties
void Game::GetDefaultSize(int& width, int& height) const noexcept
{
    // TODO: Change to desired default window size (note minimum size is 320x200).
    width = 1280;
    height = 720;
}

// These are the resources that depend on the device.
void Game::CreateDevice()
{
    UINT creationFlags = 0;

#ifdef _DEBUG
    creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    static const D3D_FEATURE_LEVEL featureLevels [] =
    {
        // TODO: Modify for supported Direct3D feature levels
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3,
        D3D_FEATURE_LEVEL_9_2,
        D3D_FEATURE_LEVEL_9_1,
    };

    // Create the DX11 API device object, and get a corresponding context.
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    DX::ThrowIfFailed(D3D11CreateDevice(
        nullptr,                            // specify nullptr to use the default adapter
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        creationFlags,
        featureLevels,
        static_cast<UINT>(std::size(featureLevels)),
        D3D11_SDK_VERSION,
        device.ReleaseAndGetAddressOf(),    // returns the Direct3D device created
        &m_featureLevel,                    // returns feature level of device created
        context.ReleaseAndGetAddressOf()    // returns the device immediate context
        ));

#ifndef NDEBUG
    ComPtr<ID3D11Debug> d3dDebug;
    if (SUCCEEDED(device.As(&d3dDebug)))
    {
        ComPtr<ID3D11InfoQueue> d3dInfoQueue;
        if (SUCCEEDED(d3dDebug.As(&d3dInfoQueue)))
        {
#ifdef _DEBUG
            d3dInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, true);
            d3dInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, true);
#endif
            D3D11_MESSAGE_ID hide [] =
            {
                D3D11_MESSAGE_ID_SETPRIVATEDATA_CHANGINGPARAMS,
                // TODO: Add more message IDs here as needed.
            };
            D3D11_INFO_QUEUE_FILTER filter = {};
            filter.DenyList.NumIDs = static_cast<UINT>(std::size(hide));
            filter.DenyList.pIDList = hide;
            d3dInfoQueue->AddStorageFilterEntries(&filter);
        }
    }
#endif

    DX::ThrowIfFailed(device.As(&m_d3dDevice));
    DX::ThrowIfFailed(context.As(&m_d3dContext));

    // TODO: Initialize device dependent objects here (independent of window size).
}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateResources()
{
    // Clear the previous window size specific context.
    ID3D11RenderTargetView* nullViews [] = { nullptr };
    m_d3dContext->OMSetRenderTargets(static_cast<UINT>(std::size(nullViews)), nullViews, nullptr);
    m_renderTargetView.Reset();
    m_depthStencilView.Reset();
    m_d3dContext->Flush();

    const UINT backBufferWidth = static_cast<UINT>(m_outputWidth);
    const UINT backBufferHeight = static_cast<UINT>(m_outputHeight);
    const DXGI_FORMAT backBufferFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
    const DXGI_FORMAT depthBufferFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    constexpr UINT backBufferCount = 2;

    // If the swap chain already exists, resize it, otherwise create one.
    if (m_swapChain)
    {
        HRESULT hr = m_swapChain->ResizeBuffers(backBufferCount, backBufferWidth, backBufferHeight, backBufferFormat, 0);

        if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
        {
            // If the device was removed for any reason, a new device and swap chain will need to be created.
            OnDeviceLost();

            // Everything is set up now. Do not continue execution of this method. OnDeviceLost will reenter this method 
            // and correctly set up the new device.
            return;
        }
        else
        {
            DX::ThrowIfFailed(hr);
        }
    }
    else
    {
        // First, retrieve the underlying DXGI Device from the D3D Device.
        ComPtr<IDXGIDevice1> dxgiDevice;
        DX::ThrowIfFailed(m_d3dDevice.As(&dxgiDevice));

        // Identify the physical adapter (GPU or card) this device is running on.
        ComPtr<IDXGIAdapter> dxgiAdapter;
        DX::ThrowIfFailed(dxgiDevice->GetAdapter(dxgiAdapter.GetAddressOf()));

        // And obtain the factory object that created it.
        ComPtr<IDXGIFactory2> dxgiFactory;
        DX::ThrowIfFailed(dxgiAdapter->GetParent(IID_PPV_ARGS(dxgiFactory.GetAddressOf())));

        // Create a descriptor for the swap chain.
        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
        swapChainDesc.Width = backBufferWidth;
        swapChainDesc.Height = backBufferHeight;
        swapChainDesc.Format = backBufferFormat;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.SampleDesc.Quality = 0;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.BufferCount = backBufferCount;

        DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsSwapChainDesc = {};
        fsSwapChainDesc.Windowed = TRUE;

        // Create a SwapChain from a Win32 window.
        DX::ThrowIfFailed(dxgiFactory->CreateSwapChainForHwnd(
            m_d3dDevice.Get(),
            m_window,
            &swapChainDesc,
            &fsSwapChainDesc,
            nullptr,
            m_swapChain.ReleaseAndGetAddressOf()
            ));

        // This template does not support exclusive fullscreen mode and prevents DXGI from responding to the ALT+ENTER shortcut.
        DX::ThrowIfFailed(dxgiFactory->MakeWindowAssociation(m_window, DXGI_MWA_NO_ALT_ENTER));
    }

    // Obtain the backbuffer for this window which will be the final 3D rendertarget.
    ComPtr<ID3D11Texture2D> m_backBuffer;
    DX::ThrowIfFailed(m_swapChain->GetBuffer(0, IID_PPV_ARGS(m_backBuffer.GetAddressOf())));

    // Create a view interface on the rendertarget to use on bind.
    DX::ThrowIfFailed(m_d3dDevice->CreateRenderTargetView(m_backBuffer.Get(), nullptr, m_renderTargetView.ReleaseAndGetAddressOf()));

    // Allocate a 2-D surface as the depth/stencil buffer and
    // create a DepthStencil view on this surface to use on bind.
    CD3D11_TEXTURE2D_DESC depthStencilDesc(depthBufferFormat, backBufferWidth, backBufferHeight, 1, 1, D3D11_BIND_DEPTH_STENCIL);

    ComPtr<ID3D11Texture2D> depthStencil;
    DX::ThrowIfFailed(m_d3dDevice->CreateTexture2D(&depthStencilDesc, nullptr, depthStencil.GetAddressOf()));

    CD3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc(D3D11_DSV_DIMENSION_TEXTURE2D);
    DX::ThrowIfFailed(m_d3dDevice->CreateDepthStencilView(depthStencil.Get(), &depthStencilViewDesc, m_depthStencilView.ReleaseAndGetAddressOf()));

    // Create intermediate render texture and depth stencil
    Microsoft::WRL::ComPtr<ID3D11Texture2D> rttTex;
    D3D11_TEXTURE2D_DESC rttDesc = {};
    rttDesc.Width = backBufferWidth;
    rttDesc.Height = backBufferHeight;
    rttDesc.MipLevels = 1;
    rttDesc.ArraySize = 1;
    rttDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    rttDesc.SampleDesc.Count = 1;
    rttDesc.Usage = D3D11_USAGE_DEFAULT;
    rttDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    rttDesc.CPUAccessFlags = 0;
    rttDesc.MiscFlags = 0;
    DX::ThrowIfFailed(m_d3dDevice->CreateTexture2D(&rttDesc, nullptr, rttTex.GetAddressOf()));
    DX::ThrowIfFailed(m_d3dDevice->CreateRenderTargetView(rttTex.Get(), nullptr, m_rttRenderTargetView.ReleaseAndGetAddressOf()));

    ComPtr<ID3D11Texture2D> rttDepthStencil;
    DX::ThrowIfFailed(m_d3dDevice->CreateTexture2D(&depthStencilDesc, nullptr, rttDepthStencil.GetAddressOf()));
    DX::ThrowIfFailed(m_d3dDevice->CreateDepthStencilView(rttDepthStencil.Get(), &depthStencilViewDesc, m_rttDepthStencilView.ReleaseAndGetAddressOf()));

    // TODO: Initialize windows-size dependent objects here.
}

void Game::InitialiseImGui(HWND hwnd)
{
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    m_ioImGui = &ImGui::GetIO(); (void)m_ioImGui;
    m_ioImGui->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
    m_ioImGui->ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
    m_ioImGui->ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows

    ImGui::StyleColorsDark();

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();
    if (m_ioImGui->ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(m_d3dDevice.Get(), m_d3dContext.Get());
}

void Game::CreateConstantBuffers() {
    // Create the constant buffer
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth = sizeof(ConstantBuffer);
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    HRESULT hr = m_d3dDevice->CreateBuffer(&bd, nullptr, m_constantBuffer.GetAddressOf());
    if (FAILED(hr))
        return;
}

void Game::CreateCameras(int width, int height) {
    // Create camera
    m_camera = std::make_shared<Camera>(XMFLOAT4(0.0f, 1.0f, -10.0f, 1.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 0.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f),
                                        Camera::CAMERA_TYPE::PERSPECTIVE,
                                        width / (float)height,
                                        XMConvertToRadians(65.0f),
                                        0.01f, 100.0f);
}

void Game::CreateGameObjects() {
    // Create and initialise GameObject
    m_gameObject = std::make_shared<GameObject>();
    m_gameObject->InitMesh(m_d3dDevice.Get(), m_d3dContext.Get());
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT , D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    UINT numElements = ARRAYSIZE(layout);
    m_gameObject->InitShader(m_d3dDevice.Get(), L"VertexShader", L"PixelShader", layout, numElements);
}

void Game::OnDeviceLost()
{
    // TODO: Add Direct3D resource cleanup here.

    m_depthStencilView.Reset();
    m_renderTargetView.Reset();
    m_swapChain.Reset();
    m_d3dContext.Reset();
    m_d3dDevice.Reset();

    CreateDevice();

    CreateResources();
}