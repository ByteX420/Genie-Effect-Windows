#include <iterator>
#include <string_view>
#include <windows.h>

#include "../app/src/core/logger.hpp"
#include "../app/src/platform/windows/window_properties.hpp"

namespace {

constexpr wchar_t kOverlayMessageName[] = L"MinimizeMinimizeAttempt";
constexpr wchar_t kRestoreMessageName[] = L"MinimizeRestoreAttempt";
constexpr wchar_t kOverlayClassName[] = L"MinimizeEffectOverlayWindow";

[[nodiscard]] constexpr bool IsMinimizeCommand(int show_cmd) noexcept {
  return show_cmd == SW_MINIMIZE || show_cmd == SW_SHOWMINIMIZED || show_cmd == SW_FORCEMINIMIZE ||
         show_cmd == SW_SHOWMINNOACTIVE;
}

[[nodiscard]] constexpr bool IsRestoreCommand(int show_cmd) noexcept {
  return show_cmd == SW_RESTORE || show_cmd == SW_SHOWNORMAL || show_cmd == SW_SHOW ||
         show_cmd == SW_SHOWDEFAULT || show_cmd == SW_SHOWNA || show_cmd == SW_SHOWNOACTIVATE ||
         show_cmd == SW_SHOWMAXIMIZED || show_cmd == SW_MAXIMIZE;
}

[[nodiscard]] UINT GetMinimizeMessage() noexcept {
  static const UINT msg = RegisterWindowMessageW(kOverlayMessageName);
  return msg;
}

[[nodiscard]] UINT GetRestoreMessage() noexcept {
  static const UINT msg = RegisterWindowMessageW(kRestoreMessageName);
  return msg;
}

}  // namespace

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) noexcept {
  (void)lpReserved;
  if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
    DisableThreadLibraryCalls(hModule);
  }
  return TRUE;
}

extern "C" __declspec(dllexport) LRESULT CALLBACK CBTProc(int code, WPARAM w_param,
                                                          LPARAM l_param) noexcept {
  if (code < 0) {
    return CallNextHookEx(nullptr, code, w_param, l_param);
  }

  try {
    if (code == HCBT_MINMAX) {
      const auto show_cmd = static_cast<int>(l_param & 0xFFFF);
      const auto target_window = reinterpret_cast<HWND>(w_param);

      if (minimize::core::IsTraceLoggingEnabled()) {
        wchar_t class_name[256]{};
        GetClassNameW(target_window, class_name, static_cast<int>(std::size(class_name)));
        wchar_t title[256]{};
        GetWindowTextW(target_window, title, static_cast<int>(std::size(title)));

        std::wstring_view cmd_name = L"UNKNOWN";
        if (IsMinimizeCommand(show_cmd))
          cmd_name = L"MINIMIZE";
        else if (IsRestoreCommand(show_cmd))
          cmd_name = L"RESTORE";

        wchar_t log_buf[512]{};
        _snwprintf_s(log_buf, std::size(log_buf), _TRUNCATE,
                     L"CBT HCBT_MINMAX: hwnd=0x%p cmd=%.*ls show_cmd=%d class=\"%ls\" "
                     L"title=\"%ls\"",
                     static_cast<void*>(target_window), static_cast<int>(cmd_name.size()),
                     cmd_name.data(), show_cmd, class_name, title);
        minimize::core::LogTrace(L"HookDLL", log_buf);
      }

      if (IsMinimizeCommand(show_cmd)) {
        if (GetPropW(target_window,
                     minimize::platform::windows::properties::kExcludedApplication) != nullptr) {
          minimize::core::LogTrace(L"HookDLL",
                                   L"Minimize allowed natively for excluded application");
        } else if (GetPropW(target_window,
                            minimize::platform::windows::properties::kAllowMinimize) == nullptr) {
          const HWND overlay_window = FindWindowW(kOverlayClassName, nullptr);
          const UINT message = GetMinimizeMessage();
          if (overlay_window != nullptr && message != 0) {
            if (PostMessageW(overlay_window, message, reinterpret_cast<WPARAM>(target_window),
                             l_param)) {
              minimize::core::LogTrace(L"HookDLL",
                                       L"PostMessage(MinimizeMinimizeAttempt) succeeded; blocking "
                                       L"native minimize");
              return 1;
            }
            const DWORD error = GetLastError();
            if (minimize::core::IsTraceLoggingEnabled()) {
              wchar_t err_buf[128]{};
              swprintf_s(err_buf, std::size(err_buf),
                         L"PostMessage(MinimizeMinimizeAttempt) failed error=%lu", error);
              minimize::core::LogDebug(L"HookDLL", err_buf);
            }
          }
        } else {
          minimize::core::LogTrace(L"HookDLL",
                                   L"Minimize allowed by property MinimizeAllowMinimize");
        }
      }

      if (IsRestoreCommand(show_cmd)) {
        if (GetPropW(target_window,
                     minimize::platform::windows::properties::kExcludedApplication) != nullptr) {
          minimize::core::LogTrace(L"HookDLL",
                                   L"Restore allowed natively for excluded "
                                   L"application");
        } else if (GetPropW(target_window,
                            minimize::platform::windows::properties::kAllowRestore) == nullptr) {
          const HWND overlay_window = FindWindowW(kOverlayClassName, nullptr);
          const UINT message = GetRestoreMessage();
          if (overlay_window != nullptr && message != 0) {
            DWORD_PTR handled = 0;
            constexpr UINT kRestoreMessageTimeoutMs = 75;
            const LRESULT send_result = SendMessageTimeoutW(
                overlay_window, message, reinterpret_cast<WPARAM>(target_window), l_param,
                SMTO_ABORTIFHUNG, kRestoreMessageTimeoutMs, &handled);
            if (send_result == 0) {
              const DWORD error = GetLastError();
              if (minimize::core::IsTraceLoggingEnabled()) {
                wchar_t err_buf[128]{};
                swprintf_s(err_buf, std::size(err_buf),
                           L"SendMessageTimeout(MinimizeRestoreAttempt) failed error=%lu", error);
                minimize::core::LogDebug(L"HookDLL", err_buf);
              }
            }
            if (handled != 0) {
              return 1;
            }
          }
        } else {
          minimize::core::LogTrace(L"HookDLL", L"Restore allowed by property MinimizeAllowRestore");
        }
      }
    }
  } catch (...) {
    // Prevent any C++ exception from crossing exported extern "C" DLL boundary
  }

  return CallNextHookEx(nullptr, code, w_param, l_param);
}
