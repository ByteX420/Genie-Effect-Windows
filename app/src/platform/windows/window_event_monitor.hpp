#pragma once

#include <functional>
#include <windows.h>

namespace minimize::platform {

class WindowEventMonitor {
public:
  using WindowCallback = std::function<void(HWND window)>;
  using WindowSeenCallback = std::function<void(HWND window, DWORD event)>;

  WindowEventMonitor() = default;
  ~WindowEventMonitor();

  WindowEventMonitor(const WindowEventMonitor&) = delete;
  WindowEventMonitor& operator=(const WindowEventMonitor&) = delete;

  bool Start(WindowCallback minimize_start_callback, WindowCallback restore_start_callback,
             WindowSeenCallback window_seen_callback);
  void Stop();

private:
  static void CALLBACK HandleWinEvent(HWINEVENTHOOK hook, DWORD event, HWND window, LONG object_id,
                                      LONG child_id, DWORD event_thread, DWORD event_time);

  static LRESULT CALLBACK MessageWindowProc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param);

  void OnWinEvent(DWORD event, HWND window, LONG object_id, LONG child_id);
  LRESULT OnShellMessage(UINT msg, WPARAM w_param, LPARAM l_param);
  void HandleMinimizeStart(HWND window);
  void HandleRestoreStart(HWND window);

  HWINEVENTHOOK minimize_hook_ = nullptr;
  HWINEVENTHOOK restore_hook_ = nullptr;
  HWINEVENTHOOK show_hook_ = nullptr;
  HWINEVENTHOOK foreground_hook_ = nullptr;
  HWINEVENTHOOK state_change_hook_ = nullptr;
  HWND message_window_ = nullptr;
  UINT shell_hook_message_ = 0;
  WindowCallback minimize_start_callback_;
  WindowCallback restore_start_callback_;
  WindowSeenCallback window_seen_callback_;

  static WindowEventMonitor* active_monitor_;
};

}  // namespace minimize::platform
