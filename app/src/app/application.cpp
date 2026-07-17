#include "pch.hpp"

#include "app/application.hpp"

#include "app/application_runtime.hpp"

namespace genie::app {

Application::Application() : runtime_(std::make_unique<ApplicationRuntime>()) {}
Application::~Application() = default;

bool Application::Initialize(HINSTANCE instance) { return runtime_->Initialize(instance); }

int Application::Run() { return runtime_->Run(); }

void Application::RequestShutdown() { runtime_->RequestShutdown(); }

}  // namespace genie::app
