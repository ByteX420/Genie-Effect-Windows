#pragma once

#include <functional>

namespace genie::platform::windows {

class ProcessRuntime final {
public:
  ProcessRuntime() = default;
  ~ProcessRuntime();
  ProcessRuntime(const ProcessRuntime&) = delete;
  ProcessRuntime& operator=(const ProcessRuntime&) = delete;

  [[nodiscard]] bool Initialize();
  void SetShutdownHandler(std::function<void()> handler);
  void Shutdown();
  [[nodiscard]] HRESULT error() const { return error_; }

private:
  bool com_initialized_ = false;
  bool console_handler_installed_ = false;
  HRESULT error_ = S_OK;
};

}  // namespace genie::platform::windows
