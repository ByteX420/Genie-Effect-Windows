#include "pch.hpp"

#include "platform/windows/single_instance_guard.hpp"

namespace genie::platform::windows {
namespace {

constexpr wchar_t kMutexName[] = L"Local\\GenieEffect.Windows.SingleInstance";
constexpr wchar_t kSettingsWindowClass[] = L"GenieEffectImGuiSettings";
constexpr UINT kShowSettingsMessage = WM_APP + 101;

}  // namespace

SingleInstanceGuard::~SingleInstanceGuard() { Release(); }

bool SingleInstanceGuard::ActivateExistingInstance(DWORD timeout_ms) {
  const ULONGLONG deadline = GetTickCount64() + timeout_ms;
  do {
    const HWND window = FindWindowW(kSettingsWindowClass, nullptr);
    if (window != nullptr) {
      DWORD_PTR ignored = 0;
      return SendMessageTimeoutW(window, kShowSettingsMessage, 0, 0, SMTO_ABORTIFHUNG | SMTO_BLOCK,
                                 1000, &ignored) != 0;
    }
    Sleep(50);
  } while (GetTickCount64() < deadline);
  return false;
}

SingleInstanceResult SingleInstanceGuard::Acquire() {
  if (mutex_ != nullptr) return SingleInstanceResult::kPrimary;
  SetLastError(ERROR_SUCCESS);
  mutex_ = CreateMutexW(nullptr, TRUE, kMutexName);
  error_ = GetLastError();
  if (mutex_ == nullptr) {
    return error_ == ERROR_ACCESS_DENIED ? SingleInstanceResult::kAlreadyRunning
                                         : SingleInstanceResult::kError;
  }
  if (error_ == ERROR_ALREADY_EXISTS) {
    Release();
    return SingleInstanceResult::kAlreadyRunning;
  }
  owns_mutex_ = true;
  return SingleInstanceResult::kPrimary;
}

void SingleInstanceGuard::Release() {
  if (mutex_ == nullptr) return;
  if (owns_mutex_) ReleaseMutex(mutex_);
  CloseHandle(mutex_);
  mutex_ = nullptr;
  owns_mutex_ = false;
}

}  // namespace genie::platform::windows
