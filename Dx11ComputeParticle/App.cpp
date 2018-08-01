#include "pch.h"

using namespace winrt;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::ApplicationModel;
using namespace Windows::UI::Core;

class App : public implements<App, IFrameworkViewSource, IFrameworkView> {
public:
  IFrameworkView CreateView() { return *this; }

  void Initialize(const CoreApplicationView &) {}

  void SetWindow(const CoreWindow &) {}

  void Load(const hstring &) {}

  void Uninitialize() {}

  void Run() {
    CoreWindow::GetForCurrentThread().Activate();
    while (true) {
      CoreWindow::GetForCurrentThread().Dispatcher().ProcessEvents(
          CoreProcessEventsOption::ProcessAllIfPresent);
    }
  }

private:
  std::vector<char> ReadFile(const std::filesystem::path& name) {
    const auto root = std::filesystem::path{
        Package::Current().InstalledLocation().Path().c_str()};
    std::ifstream file{root / name};
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
