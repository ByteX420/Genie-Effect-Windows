#include "pch.hpp"

#include "app/application_runtime.hpp"

#include <atomic>
#include <iostream>
#include <string>
#include <string_view>

#include "animation/geometry.hpp"
#include "core/environment.hpp"
#include "core/logger.hpp"
#include "platform/windows/app_container_permissions.hpp"
#include "platform/windows/display_info.hpp"
#include "platform/windows/power_status.hpp"
#include "platform/windows/process_info.hpp"
#include "platform/windows/startup_manager.hpp"
#include "platform/windows/window_diagnostics.hpp"
#include "platform/windows/window_properties.hpp"
#include "platform/windows/window_state.hpp"
#include "settings/exclusion_rules.hpp"
#include "settings/settings_service.hpp"

namespace genie::app {

ApplicationRuntime::~ApplicationRuntime() { CleanupAndRestoreAll(); }

bool ApplicationRuntime::Initialize(HINSTANCE instance) {
  genie::core::CleanupDebugLogs();
  instance_ = instance;
  main_thread_id_ = GetCurrentThreadId();
#ifdef _DEBUG
  device_recovery_test_pending_ = core::EnvironmentFlagEnabled("GENIE_TEST_DEVICE_RECOVERY");
#endif
  (void)settings_service_.Load();
  effect_policy_.Configure(settings_service_.Get());
  if (settings_service_.Get().run_at_startup && !platform::windows::ConfigureRunAtStartup(true)) {
    genie::core::LogDebug(L"Startup",
                          L"Could not repair the per-user startup entry; disabling the option");
    auto repaired_settings = settings_service_.Get();
    repaired_settings.run_at_startup = false;
    if (!settings_service_.Update(std::move(repaired_settings))) {
      genie::core::LogDebug(L"Startup", L"Could not persist the repaired startup state");
    }
  }
#ifdef _DEBUG
  // Touch log file and grant permissions so AppContainers can write to it
  {
    const std::wstring& log_path = genie::core::DebugLogPath();
    HANDLE file =
        CreateFileW(log_path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                    OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
      CloseHandle(file);
      (void)platform::GrantAppContainerPermissions(log_path);
    }
  }
#endif

  genie::core::LogDebug(L"App", L"ApplicationRuntime::Initialize started");
  genie::core::LogTrace(L"App", L"ApplicationRuntime::Initialize started");

  frame_scheduler_.Initialize();

  HealLeftoverWindows();

  if (!platform::IsCurrentProcessElevated()) {
    std::wcerr << L"WARNING: Not running as Administrator. Elevated windows (like Task Manager, "
                  L"cmd as Admin, etc.)\n"
               << L"         will NOT be hooked due to Windows UIPI security restrictions.\n"
               << L"         To hook all windows, please run GenieEffect.exe as Administrator.\n\n";
    genie::core::LogDebug(L"App", L"Warning: Not running as Administrator");
  } else {
    genie::core::LogDebug(L"App", L"Running as Administrator");
  }

  if (!CreateAnimationRenderer()) return false;

  if (!settings_window_.Initialize(instance, *this)) {
    return false;
  }
  settings_window_.UpdateState(settings_service_.Get());
  hotkey_controller_.SetWindow(settings_window_.hwnd());
  RegisterConfiguredHotkeys();
  settings_window_.UpdatePauseState(false, false);
  settings_window_.Show(!settings_service_.Get().start_minimized ||
                        settings_service_.Get().close_behavior != "tray");
  UpdateFullscreenSuppression(true);
  UpdatePowerState(true);
  RefreshEffectRuntimeState();

  if (!effect_controller_.Start([this](HWND window) { return OnMinimizeStart(window); },
                                [this](HWND window) { return OnRestoreAttempt(window); },
                                [this](HWND window, DWORD event) {
                                  effect_controller_.HandleWindowSeen(
                                      window, event, GetOverlayWindow(),
                                      renderer_recovery_.pending(), native_animation_blocker_,
                                      desktop_capture_.get(),
                                      [this](HWND target) { return OnRestoreAttempt(target); });
                                })) {
    return false;
  }

  std::wcout << L"Genie minimize monitor is running.\n";
  genie::core::LogTrace(L"App", L"ApplicationRuntime::Initialize completed");
  std::wcout << L"Set GENIE_TASKBAR_RECT=left,top,right,bottom to aim at a "
                L"custom taskbar rectangle.\n";
  std::wcout << L"Close this console window to restore the previous Windows "
                L"animation setting.\n";
  return true;
}

int ApplicationRuntime::Run() {
  return message_loop_.Run(MessageLoopCallbacks{
      .should_stop = [this] { return shutting_down_.load(std::memory_order_acquire); },
      .update = [this] { UpdateRuntime(); },
      .display_changed = [this] { HandleDisplayChange(); },
      .render_settings = [this] { settings_window_.Render(); },
      .tick_runtime = [this] { return TickRuntime(); },
      .wait_for_animation = [this] { WaitForAnimationFrameOrMessage(); },
  });
}

void ApplicationRuntime::RequestShutdown() {
  shutting_down_.store(true, std::memory_order_release);
  frame_scheduler_.Wake();
  // PostQuitMessage works when called from the main thread (settings UI click path).
  // PostThreadMessage covers the same queue if we are already on it, and is the
  // wake path for console-control / other-thread shutdown requests.
  PostQuitMessage(0);
  if (main_thread_id_ != 0) {
    PostThreadMessageW(main_thread_id_, WM_QUIT, 0, 0);
  }
}

}  // namespace genie::app
