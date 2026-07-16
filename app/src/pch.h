#pragma once

// Compatibility include required by the  beta ImGui fork. Keeping this
// shim outside third_party leaves every imported vendor file byte-identical.
#include "pch.hpp"
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "imgui_internal.h"
