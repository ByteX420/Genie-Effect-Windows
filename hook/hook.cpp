#include <string>
#include <windows.h>

#include "../app/src/core/logger.hpp"
#include "../app/src/platform/windows/window_properties.hpp"

namespace {

constexpr wchar_t kOverlayMessageName[] = L"GenieMinimizeAttempt";
constexpr wchar_t kRestoreMessageName[] = L"GenieRestoreAttempt";
constexpr wchar_t kOverlayClassName[] = L"MinimizeEffectOverlayWindow";

bool IsMinimizeCommand(LPARAM l_param) {
  if (l_param == SW_MINIMIZE || l_param == SW_SHOWMINIMIZED || l_param == SW_FORCEMINIMIZE) {
    return true;
  }

  if (l_param == SW_SHOWMINNOACTIVE) {
    return true;
  }

  return false;
}

bool IsRestoreCommand(LPARAM l_param) {
  return l_param == SW_RESTORE || l_param == SW_SHOWNORMAL || l_param == SW_SHOW ||
         l_param == SW_SHOWDEFAULT || l_param == SW_SHOWNA || l_param == SW_SHOWNOACTIVATE ||
         l_param == SW_SHOWMAXIMIZED || l_param == SW_MAXIMIZE;
}

}  // namespace

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
  (void)hModule;
  (void)lpReserved;
  (void)ul_reason_for_call;
  return TRUE;
}

extern "C" __declspec(dllexport) LRESULT CALLBACK CBTProc(int code, WPARAM w_param,
                                                          LPARAM l_param) {
  const int show_cmd = LOWORD(l_param);
  HWND target_window = reinterpret_cast<HWND>(w_param);

  if (code == HCBT_MINMAX) {
    if (genie::core::IsTraceLoggingEnabled()) {
      wchar_t class_name[256]{};
      GetClassNameW(target_window, class_name, 256);
      wchar_t title[256]{};
      GetWindowTextW(target_window, title, 256);

      std::wstring cmd_name = L"UNKNOWN";
      if (IsMinimizeCommand(show_cmd))
        cmd_name = L"MINIMIZE";
      else if (IsRestoreCommand(show_cmd))
        cmd_name = L"RESTORE";

      genie::core::LogTrace(L"HookDLL",
                            L"CBT HCBT_MINMAX: hwnd=0x" +
                                std::to_wstring(reinterpret_cast<std::uintptr_t>(target_window)) +
                                L" cmd=" + cmd_name + L" show_cmd=" + std::to_wstring(show_cmd) +
                                L" class=\"" + class_name + L"\" title=\"" + title + L"\"");
    }

    if (IsMinimizeCommand(show_cmd)) {
      if (GetPropW(target_window, genie::platform::windows::properties::kExcludedApplication) !=
          nullptr) {
        genie::core::LogTrace(L"HookDLL", L"Minimize allowed natively for excluded application");
      } else if (GetPropW(target_window, genie::platform::windows::properties::kAllowMinimize) ==
                 nullptr) {
        HWND overlay_window = FindWindowW(kOverlayClassName, nullptr);
        const UINT message = RegisterWindowMessageW(kOverlayMessageName);
        genie::core::LogTrace(
            L"HookDLL", L"Attempting to intercept minimize for hwnd=0x" +
                            std::to_wstring(reinterpret_cast<std::uintptr_t>(target_window)) +
                            L" overlay=0x" +
                            std::to_wstring(reinterpret_cast<std::uintptr_t>(overlay_window)) +
                            L" message=" + std::to_wstring(message));
        if (overlay_window != nullptr && message != 0) {
          if (PostMessageW(overlay_window, message, reinterpret_cast<WPARAM>(target_window),
                           l_param)) {
            genie::core::LogTrace(L"HookDLL",
                                  L"PostMessage(GenieMinimizeAttempt) succeeded; blocking native "
                                  L"minimize");
            return 1;
          }
          genie::core::LogDebug(L"HookDLL", L"PostMessage(GenieMinimizeAttempt) failed error=" +
                                                std::to_wstring(GetLastError()));
        }
      } else {
        genie::core::LogTrace(L"HookDLL",
                              L"Minimize allowed by property GenieAllowMinimize for hwnd=0x" +
                                  std::to_wstring(reinterpret_cast<std::uintptr_t>(target_window)));
      }
    }

    if (IsRestoreCommand(show_cmd)) {
      if (GetPropW(target_window, genie::platform::windows::properties::kExcludedApplication) !=
          nullptr) {
        genie::core::LogTrace(L"HookDLL", L"Restore allowed natively for excluded application");
      } else if (GetPropW(target_window, genie::platform::windows::properties::kAllowRestore) ==
                 nullptr) {
        HWND overlay_window = FindWindowW(kOverlayClassName, nullptr);
        const UINT message = RegisterWindowMessageW(kRestoreMessageName);
        genie::core::LogTrace(
            L"HookDLL", L"Attempting to intercept restore for hwnd=0x" +
                            std::to_wstring(reinterpret_cast<std::uintptr_t>(target_window)) +
                            L" overlay=0x" +
                            std::to_wstring(reinterpret_cast<std::uintptr_t>(overlay_window)) +
                            L" message=" + std::to_wstring(message));
        if (overlay_window != nullptr && message != 0) {
          DWORD_PTR handled = 0;
          constexpr UINT kRestoreMessageTimeoutMs = 75;
          const LRESULT send_result =
              SendMessageTimeoutW(overlay_window, message, reinterpret_cast<WPARAM>(target_window),
                                  l_param, SMTO_ABORTIFHUNG, kRestoreMessageTimeoutMs, &handled);
          if (send_result == 0) {
            genie::core::LogDebug(L"HookDLL",
                                  L"SendMessageTimeout(GenieRestoreAttempt) failed error=" +
                                      std::to_wstring(GetLastError()));
          }
          genie::core::LogTrace(L"HookDLL", L"SendMessageTimeout(GenieRestoreAttempt) returned " +
                                                std::to_wstring(handled));
          if (handled != 0) {
            return 1;
          }
        }
      } else {
        genie::core::LogTrace(L"HookDLL",
                              L"Restore allowed by property GenieAllowRestore for hwnd=0x" +
                                  std::to_wstring(reinterpret_cast<std::uintptr_t>(target_window)));
      }
    }
  }

  return CallNextHookEx(nullptr, code, w_param, l_param);
}
