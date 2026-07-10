#include "pch.hpp"

#include "app/application.hpp"

#ifndef _DEBUG
#pragma comment(linker, "/subsystem:windows /ENTRY:wmainCRTStartup")
#endif

static genie::app::Application* g_application = nullptr;

BOOL WINAPI ConsoleHandler(DWORD signal) {
  if (signal == CTRL_CLOSE_EVENT || signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT ||
      signal == CTRL_LOGOFF_EVENT || signal == CTRL_SHUTDOWN_EVENT) {
    if (g_application != nullptr) {
      g_application->RequestShutdown();
    }
    return TRUE;
  }
  return FALSE;
}

int wmain() {
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

  const HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  if (FAILED(hr)) {
    std::wcerr << L"CoInitializeEx failed: 0x" << std::hex << hr << L"\n";
    return 1;
  }

  genie::app::Application application;
  g_application = &application;
  SetConsoleCtrlHandler(ConsoleHandler, TRUE);

  if (!application.Initialize(GetModuleHandleW(nullptr))) {
    g_application = nullptr;
    SetConsoleCtrlHandler(ConsoleHandler, FALSE);
    CoUninitialize();
    return 1;
  }

  const int result = application.Run();
  g_application = nullptr;
  SetConsoleCtrlHandler(ConsoleHandler, FALSE);
  CoUninitialize();

  return result;
}
