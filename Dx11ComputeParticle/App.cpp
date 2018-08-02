#include "pch.h"

using namespace winrt;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::ApplicationModel;
using namespace Windows::Foundation::Numerics;
using namespace Windows::UI::Core;

class App : public implements<App, IFrameworkViewSource, IFrameworkView> {
public:
  IFrameworkView CreateView() { return *this; }

  void Initialize(const CoreApplicationView &) {}

  void SetWindow(const CoreWindow &) {}

  void Load(const hstring &) {
    const D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    check_hresult(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                    D3D11_CREATE_DEVICE_DEBUG, &featureLevel, 1,
                                    D3D11_SDK_VERSION, device.put(), nullptr,
                                    context.put()));

    com_ptr<IDXGIFactory2> dxgiFactory{};
    check_hresult(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG,
                                     IID_PPV_ARGS(dxgiFactory.put())));

    const auto window = CoreWindow::GetForCurrentThread();
    const auto windowBounds = window.Bounds();
    DXGI_SWAP_CHAIN_DESC1 swapchainDesc{};
    swapchainDesc.Width = static_cast<UINT>(windowBounds.Width);
    swapchainDesc.Height = static_cast<UINT>(windowBounds.Height);
    swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapchainDesc.SampleDesc.Count = 1;
    swapchainDesc.SampleDesc.Quality = 0;
    swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapchainDesc.BufferCount = 2;
    swapchainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;

    check_hresult(dxgiFactory->CreateSwapChainForCoreWindow(
        device.get(), window.as<IUnknown>().get(), &swapchainDesc, nullptr,
        swapchain.put()));

    com_ptr<ID3D11Texture2D> backBuffer{};
    check_hresult(swapchain->GetBuffer(0, IID_PPV_ARGS(backBuffer.put())));

    D3D11_RENDER_TARGET_VIEW_DESC rtvDesc{};
    rtvDesc.Format = swapchainDesc.Format;
    rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    check_hresult(
        device->CreateRenderTargetView(backBuffer.get(), &rtvDesc, rtv.put()));

    D3D11_TEXTURE2D_DESC depthBufferDesc{};
    depthBufferDesc.Width = swapchainDesc.Width;
    depthBufferDesc.Height = swapchainDesc.Height;
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

    const auto vsData = ReadFile(L"VS.cso");
    const auto psData = ReadFile(L"PS.cso");

    check_hresult(device->CreateVertexShader(vsData.data(), vsData.size(),
                                             nullptr, vertexShader.put()));

    check_hresult(device->CreatePixelShader(psData.data(), psData.size(),
                                            nullptr, pixelShader.put()));

    D3D11_INPUT_ELEMENT_DESC layout{};
    layout.SemanticName = "Position";
    layout.SemanticIndex = 0;
    layout.Format = DXGI_FORMAT_R32G32B32_FLOAT;
    layout.AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
    check_hresult(device->CreateInputLayout(&layout, 1, vsData.data(),
                                            vsData.size(), inputLayout.put()));

    D3D11_BUFFER_DESC vertexBufferDesc{};
    vertexBufferDesc.ByteWidth = sizeof(float3) * 3;
    vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vertexBufferData{};
    std::array<float3, 3> vertices{
        {{-0.5, -0.5, 0}, {0, 0.5, 0}, {0.5, -0.5, 0}}};
    vertexBufferData.pSysMem = vertices.data();
    device->CreateBuffer(&vertexBufferDesc, &vertexBufferData,
                         vertexBuffer.put());
  }

  void Uninitialize() {}

  void Run() {
    CoreWindow::GetForCurrentThread().Activate();
    while (true) {
      CoreWindow::GetForCurrentThread().Dispatcher().ProcessEvents(
          CoreProcessEventsOption::ProcessAllIfPresent);
      Render();
    }
  }

  void Render() {
    const auto &windowBounds = CoreWindow::GetForCurrentThread().Bounds();
    D3D11_VIEWPORT viewport{};
    viewport.Width = static_cast<FLOAT>(windowBounds.Width);
    viewport.Height = static_cast<FLOAT>(windowBounds.Height);
    viewport.MaxDepth = 1;
    context->RSSetViewports(1, &viewport);

    const std::array<ID3D11RenderTargetView *, 1> rtvs{rtv.get()};
    context->OMSetRenderTargets(static_cast<UINT>(rtvs.size()), rtvs.data(),
                                dsv.get());

    const std::array<float, 4> clear_color{};
    context->ClearRenderTargetView(rtv.get(), clear_color.data());
    context->ClearDepthStencilView(
        dsv.get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);

    ID3D11Buffer *pVertexBuffer = vertexBuffer.get();
    const UINT vertexBufferStride{sizeof(float3)};
    const UINT vertexBufferOffset{0};
    context->IASetVertexBuffers(0, 1, &pVertexBuffer, &vertexBufferStride,
                                &vertexBufferOffset);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->IASetInputLayout(inputLayout.get());

    context->VSSetShader(vertexShader.get(), nullptr, 0);
    context->PSSetShader(pixelShader.get(), nullptr, 0);

    context->Draw(3, 0);

    check_hresult(swapchain->Present(1, 0));
  }

private:
  std::vector<char> ReadFile(const std::filesystem::path &name) {
    const auto root = std::filesystem::path{
        Package::Current().InstalledLocation().Path().c_str()};
    std::ifstream file{root / name, std::ios::binary};
    return std::vector<char>{std::istreambuf_iterator<char>{file},
                             std::istreambuf_iterator<char>{}};
  }

  com_ptr<ID3D11Device> device{};
  com_ptr<ID3D11DeviceContext> context{};
  com_ptr<IDXGISwapChain1> swapchain{};
  com_ptr<ID3D11RenderTargetView> rtv{};
  com_ptr<ID3D11DepthStencilView> dsv{};
  com_ptr<ID3D11VertexShader> vertexShader{};
  com_ptr<ID3D11PixelShader> pixelShader{};
  com_ptr<ID3D11InputLayout> inputLayout{};
  com_ptr<ID3D11Buffer> vertexBuffer{};
};

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
  CoreApplication::Run(App());
}
