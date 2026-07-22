#include "pch.hpp"

#include "platform/windows/process_runtime.hpp"

#include <mutex>

namespace minimize::platform::windows {
namespace {

std::mutex g_handler_mutex;
std::function<void()> g_shutdown_handler;

BOOL WINAPI ConsoleHandler(DWORD signal) {
  if (signal != CTRL_CLOSE_EVENT && signal != CTRL_C_EVENT && signal != CTRL_BREAK_EVENT &&
      signal != CTRL_LOGOFF_EVENT && signal != CTRL_SHUTDOWN_EVENT)
    return FALSE;
  std::function<void()> handler;
  {
    std::scoped_lock lock(g_handler_mutex);
    handler = g_shutdown_handler;
  }
  if (handler) handler();
  return TRUE;
}

}  // namespace

ProcessRuntime::~ProcessRuntime() { Shutdown(); }

bool ProcessRuntime::Initialize() {
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  error_ = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  if (FAILED(error_)) return false;
  com_initialized_ = true;
  if (SetConsoleCtrlHandler(ConsoleHandler, TRUE)) {
    console_handler_installed_ = true;
  }
  return true;
}

void ProcessRuntime::SetShutdownHandler(std::function<void()> handler) {
  std::scoped_lock lock(g_handler_mutex);
  g_shutdown_handler = std::move(handler);
}

void ProcessRuntime::Shutdown() {
  SetShutdownHandler({});
  if (console_handler_installed_) {
    SetConsoleCtrlHandler(ConsoleHandler, FALSE);
    console_handler_installed_ = false;
  }
  if (com_initialized_) {
    CoUninitialize();
    com_initialized_ = false;
  }
}

}  // namespace minimize::platform::windows
