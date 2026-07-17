#pragma once

#include <memory>
#include <windows.h>

namespace genie::app {

class ApplicationRuntime;

class Application final {
public:
  Application();
  ~Application();
  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;

  [[nodiscard]] bool Initialize(HINSTANCE instance);
  [[nodiscard]] int Run();
  void RequestShutdown();

private:
  std::unique_ptr<ApplicationRuntime> runtime_;
};

}  // namespace genie::app
