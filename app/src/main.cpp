#include "pch.hpp"

#include "app/application.hpp"
#include "platform/windows/process_runtime.hpp"
#include "platform/windows/single_instance_guard.hpp"

#ifndef _DEBUG
#pragma comment(linker, "/subsystem:windows /ENTRY:wmainCRTStartup")
#endif

int wmain() {
  minimize::platform::windows::SingleInstanceGuard instance_guard;
  const auto instance_result = instance_guard.Acquire();
  if (instance_result == minimize::platform::windows::SingleInstanceResult::kAlreadyRunning) {
    (void)minimize::platform::windows::SingleInstanceGuard::ActivateExistingInstance(5000);
    return 0;
  }
  if (instance_result == minimize::platform::windows::SingleInstanceResult::kError) {
    std::wcerr << L"Could not create the single-instance guard: " << instance_guard.error()
               << L"\n";
    return 1;
  }

  minimize::platform::windows::ProcessRuntime process_runtime;
  if (!process_runtime.Initialize()) {
    std::wcerr << L"Process initialization failed: 0x" << std::hex << process_runtime.error()
               << L"\n";
    return 1;
  }

  minimize::app::Application application;
  process_runtime.SetShutdownHandler([&application] { application.RequestShutdown(); });

  if (!application.Initialize(GetModuleHandleW(nullptr))) {
    return 1;
  }

  return application.Run();
}
