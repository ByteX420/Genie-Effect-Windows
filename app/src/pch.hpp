#pragma once

// The  fork enables FreeType in imconfig.h. This application keeps its
// existing embedded-font atlas and therefore selects ImGui's built-in stb
// builder without modifying the imported vendor configuration file.
#include "imconfig.h"
#define IMGUI_ENABLE_FREETYPE

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <d3d11.h>
#include <dcomp.h>
#include <dwmapi.h>
#include <dxgi1_2.h>
#include <fstream>
#include <iostream>
#include <memory>
#include <objbase.h>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <windows.h>
#include <wrl/client.h>
