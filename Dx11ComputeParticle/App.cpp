#include "pch.h"

using namespace winrt;

using namespace Windows::ApplicationModel::Core;
using namespace Windows::UI::Core;

struct App : implements<App, IFrameworkViewSource, IFrameworkView> {
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
};

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
  CoreApplication::Run(App());
}
