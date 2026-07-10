#include "pch.hpp"

#include "app/application.hpp"

#ifndef _DEBUG
#pragma comment(linker, "/subsystem:windows /ENTRY:wmainCRTStartup")
#endif

static genie::app::Application* g_application = nullptr;
constexpr wchar_t kSingleInstanceMutexName[] = L"Local\\GenieEffect.Windows.SingleInstance";

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

  HANDLE instance_mutex = CreateMutexW(nullptr, TRUE, kSingleInstanceMutexName);
  const DWORD mutex_error = GetLastError();
  if (instance_mutex == nullptr) {
    if (mutex_error == ERROR_ACCESS_DENIED) {
      (void)genie::app::SettingsWindow::ActivateExistingInstance(5000);
      return 0;
    }
    std::wcerr << L"Could not create the single-instance guard: " << mutex_error << L"\n";
    return 1;
  }
  if (mutex_error == ERROR_ALREADY_EXISTS) {
    CloseHandle(instance_mutex);
    (void)genie::app::SettingsWindow::ActivateExistingInstance(5000);
    return 0;
  }

  const HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  if (FAILED(hr)) {
    std::wcerr << L"CoInitializeEx failed: 0x" << std::hex << hr << L"\n";
    CloseHandle(instance_mutex);
    return 1;
  }

  genie::app::Application application;
  g_application = &application;
  SetConsoleCtrlHandler(ConsoleHandler, TRUE);

  if (!application.Initialize(GetModuleHandleW(nullptr))) {
    g_application = nullptr;
    SetConsoleCtrlHandler(ConsoleHandler, FALSE);
    CoUninitialize();
    CloseHandle(instance_mutex);
    return 1;
  }

  const int result = application.Run();
  g_application = nullptr;
  SetConsoleCtrlHandler(ConsoleHandler, FALSE);
  CoUninitialize();
  CloseHandle(instance_mutex);

  return result;
}
