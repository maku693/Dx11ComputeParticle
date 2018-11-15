#include "pch.h"

using namespace winrt;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::ApplicationModel;
using namespace Windows::Foundation::Numerics;
using namespace Windows::UI::Core;
using namespace Windows::UI::ViewManagement;

std::vector<char> readFile(const std::filesystem::path &name) {
  const auto root = std::filesystem::path{
      Package::Current().InstalledLocation().Path().c_str()};
  std::ifstream file{root / name, std::ios::binary};
  return std::vector<char>{std::istreambuf_iterator<char>{file},
                           std::istreambuf_iterator<char>{}};
}

struct Vertex {
  float3 Position;
  float4 Color;
};

struct ParticleSettings {
  unsigned int particleCount;
  unsigned int lifetime;
  double unused__;
};

struct Particle {
  unsigned int age;
  float3 position;
  float3 velocity;
};

class App : public implements<App, IFrameworkViewSource, IFrameworkView> {
public:
  IFrameworkView CreateView() { return *this; }

  void Initialize(const CoreApplicationView &) {}

  void SetWindow(const CoreWindow &) {}

  void Load(const hstring &) {
    const auto windowBounds = CoreWindow::GetForCurrentThread().Bounds();
    windowWidth = static_cast<UINT>(windowBounds.Width);
    windowHeight = static_cast<UINT>(windowBounds.Height);

    // Setup particles
    std::mt19937 mt{};
    std::uniform_real_distribution<float> dist{-1, 1};
    const auto rnd = [&mt, &dist]() { return dist(mt); };
    for (auto &particle : particles) {
      particle.velocity.x = rnd();
      particle.velocity.y = rnd();
      particle.velocity.z = rnd();
      particle.velocity = normalize(particle.velocity);
      particle.velocity *= 0.05f;
    }

    SetupDevice();

    SetupCSUAV();
    SetupComputeShader();
    SetupCSConstantBuffer();

    SetupSwapchain();
    SetupRTV();
    SetupDSV();
    SetupRenderingShaders();
    SetupVertexBuffers();
  }

  void Uninitialize() {}

  void Run() {
    CoreWindow::GetForCurrentThread().Activate();
    while (true) {
      CoreWindow::GetForCurrentThread().Dispatcher().ProcessEvents(
          CoreProcessEventsOption::ProcessAllIfPresent);

      Dispatch();
      CopyResouces();
      Render();
    }
  }

private:
  static constexpr DXGI_FORMAT swapchainFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
  static constexpr int particleCount = 1024;
  static constexpr int threadGroupCountX = particleCount / 64;

  UINT windowWidth{};
  UINT windowHeight{};

  std::array<Particle, particleCount> particles{};

  com_ptr<ID3D11Device> device{};
  com_ptr<ID3D11DeviceContext> context{};

  com_ptr<ID3D11ComputeShader> computeShader{};
  com_ptr<ID3D11Buffer> csStructuredBuffer{};
  com_ptr<ID3D11UnorderedAccessView> csUAV{};
  com_ptr<ID3D11Buffer> csConstantBuffer{};

  com_ptr<IDXGISwapChain1> swapchain{};
  com_ptr<ID3D11RenderTargetView> rtv{};
  com_ptr<ID3D11DepthStencilView> dsv{};
  com_ptr<ID3D11VertexShader> vertexShader{};
  com_ptr<ID3D11PixelShader> pixelShader{};
  com_ptr<ID3D11InputLayout> inputLayout{};
  com_ptr<ID3D11Buffer> vertexBuffer{};
  com_ptr<ID3D11Buffer> instanceBuffer{};
  std::array<ID3D11Buffer *, 2> vertexBuffers;
  std::array<UINT, 2> vertexBufferStrides;
  std::array<UINT, 2> vertexBufferOffsets;

  void SetupDevice() {
    const D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    check_hresult(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                    0, &featureLevel, 1, D3D11_SDK_VERSION,
                                    device.put(), nullptr, context.put()));
  }

  void SetupCSUAV() {
    D3D11_BUFFER_DESC csStructuredBufferDesc{};
    csStructuredBufferDesc.ByteWidth = sizeof(Particle) * particleCount;
    csStructuredBufferDesc.StructureByteStride = sizeof(Particle);
    csStructuredBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    csStructuredBufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    csStructuredBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    D3D11_SUBRESOURCE_DATA csStructuredBufferData{};
    csStructuredBufferData.pSysMem = particles.data();
    check_hresult(device->CreateBuffer(&csStructuredBufferDesc,
                                       &csStructuredBufferData,
                                       csStructuredBuffer.put()));
    check_hresult(device->CreateUnorderedAccessView(csStructuredBuffer.get(),
                                                    nullptr, csUAV.put()));
  }

  void SetupComputeShader() {
    const auto csData = readFile(L"CS.cso");
    check_hresult(device->CreateComputeShader(csData.data(), csData.size(),
                                              nullptr, computeShader.put()));
  }

  void SetupCSConstantBuffer() {
    D3D11_BUFFER_DESC csConstantBufferDesc{};
    csConstantBufferDesc.ByteWidth = sizeof(ParticleSettings);
    csConstantBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    csConstantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    D3D11_SUBRESOURCE_DATA csConstantBufferData{};
    ParticleSettings particleSettings{particleCount, 60};
    csConstantBufferData.pSysMem = &particleSettings;
    check_hresult(device->CreateBuffer(
        &csConstantBufferDesc, &csConstantBufferData, csConstantBuffer.put()));
  }

  void SetupSwapchain() {
    com_ptr<IDXGIFactory2> dxgiFactory{};
    check_hresult(CreateDXGIFactory2(0, IID_PPV_ARGS(dxgiFactory.put())));

    const auto window = CoreWindow::GetForCurrentThread();

    DXGI_SWAP_CHAIN_DESC1 swapchainDesc{};
    swapchainDesc.Width = windowWidth;
    swapchainDesc.Height = windowHeight;
    swapchainDesc.Format = swapchainFormat;
    swapchainDesc.SampleDesc.Count = 1;
    swapchainDesc.SampleDesc.Quality = 0;
    swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapchainDesc.BufferCount = 2;
    swapchainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;

    check_hresult(dxgiFactory->CreateSwapChainForCoreWindow(
        device.get(), window.as<IUnknown>().get(), &swapchainDesc, nullptr,
        swapchain.put()));
  }

  void SetupRTV() {
    com_ptr<ID3D11Texture2D> backBuffer{};
    check_hresult(swapchain->GetBuffer(0, IID_PPV_ARGS(backBuffer.put())));

    D3D11_RENDER_TARGET_VIEW_DESC rtvDesc{};
    rtvDesc.Format = swapchainFormat;
    rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    check_hresult(
        device->CreateRenderTargetView(backBuffer.get(), &rtvDesc, rtv.put()));
  }

  void SetupDSV() {
    D3D11_TEXTURE2D_DESC depthBufferDesc{};
    depthBufferDesc.Width = windowWidth;
    depthBufferDesc.Height = windowHeight;
    depthBufferDesc.MipLevels = 1;
    depthBufferDesc.ArraySize = 1;
    depthBufferDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
    depthBufferDesc.SampleDesc.Count = 1;
    depthBufferDesc.SampleDesc.Quality = 0;
    depthBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    depthBufferDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    com_ptr<ID3D11Texture2D> depthBuffer{};
    check_hresult(
        device->CreateTexture2D(&depthBufferDesc, nullptr, depthBuffer.put()));

    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    check_hresult(
        device->CreateDepthStencilView(depthBuffer.get(), &dsvDesc, dsv.put()));
  }

  void SetupRenderingShaders() {
    const auto vsData = readFile(L"VS.cso");
    const auto psData = readFile(L"PS.cso");
    check_hresult(device->CreateVertexShader(vsData.data(), vsData.size(),
                                             nullptr, vertexShader.put()));
    check_hresult(device->CreatePixelShader(psData.data(), psData.size(),
                                            nullptr, pixelShader.put()));

    std::array<D3D11_INPUT_ELEMENT_DESC, 3> elementDescs{{
        {"Center", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
         D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"Color", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
         D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"Position", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1,
         D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1},
    }};
    check_hresult(device->CreateInputLayout(
        elementDescs.data(), static_cast<UINT>(elementDescs.size()),
        vsData.data(), vsData.size(), inputLayout.put()));
  }

  void SetupVertexBuffers() {
    D3D11_BUFFER_DESC vertexBufferDesc{};
    vertexBufferDesc.ByteWidth = sizeof(Vertex) * 3;
    vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vertexBufferData{};
    std::array<Vertex, 3> vertices{{{{-0.1f, -0.1f, 0}, {1, 0, 0, 1}},
                                    {{0, 0.1f, 0}, {0, 1, 0, 1}},
                                    {{0.1f, -0.1f, 0}, {0, 0, 1, 1}}}};
    vertexBufferData.pSysMem = vertices.data();
    check_hresult(device->CreateBuffer(&vertexBufferDesc, &vertexBufferData,
                                       vertexBuffer.put()));

    D3D11_BUFFER_DESC instanceBufferDesc{};
    instanceBufferDesc.ByteWidth = sizeof(Particle) * particleCount;
    instanceBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    instanceBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    check_hresult(device->CreateBuffer(&instanceBufferDesc, nullptr,
                                       instanceBuffer.put()));
    vertexBuffers = {
        vertexBuffer.get(),
        instanceBuffer.get(),
    };
    vertexBufferStrides = {
        sizeof(Vertex),
        sizeof(Particle),
    };
    vertexBufferOffsets = {
        0,
        sizeof(unsigned int), // Skip `age`
    };
  }

  void Dispatch() {
    auto *pCSConstantBuffer = csConstantBuffer.get();
    context->CSSetConstantBuffers(0, 1, &pCSConstantBuffer);
    auto *pCSUAV = csUAV.get();
    context->CSSetUnorderedAccessViews(0, 1, &pCSUAV, nullptr);
    context->CSSetShader(computeShader.get(), nullptr, 0);
    context->Dispatch(threadGroupCountX, 1, 1);
  }

  void CopyResouces() {
    context->CopyResource(instanceBuffer.get(), csStructuredBuffer.get());
  }

  void Render() {
    D3D11_VIEWPORT viewport{};
    viewport.Width = windowWidth;
    viewport.Height = windowHeight;
    viewport.MaxDepth = 1;
    context->RSSetViewports(1, &viewport);

    const std::array<ID3D11RenderTargetView *, 1> rtvs{rtv.get()};
    context->OMSetRenderTargets(static_cast<UINT>(rtvs.size()), rtvs.data(),
                                dsv.get());

    const std::array<float, 4> clear_color{};
    context->ClearRenderTargetView(rtv.get(), clear_color.data());
    context->ClearDepthStencilView(
        dsv.get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);

    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->IASetInputLayout(inputLayout.get());
    context->IASetVertexBuffers(
        0, static_cast<UINT>(vertexBuffers.size()), vertexBuffers.data(),
        vertexBufferStrides.data(), vertexBufferOffsets.data());
    context->VSSetShader(vertexShader.get(), nullptr, 0);
    context->PSSetShader(pixelShader.get(), nullptr, 0);

    context->DrawInstanced(3, particleCount, 0, 0);

    check_hresult(swapchain->Present(1, 0));
  }
};

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
  ApplicationView::PreferredLaunchViewSize({640, 480});
  ApplicationView::PreferredLaunchWindowingMode(
      ApplicationViewWindowingMode::PreferredLaunchViewSize);
  CoreApplication::Run(App());
}
