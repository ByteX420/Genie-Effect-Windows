#include "pch.hpp"

#include "core/embedded_resource.hpp"

namespace genie::core {

std::string LoadEmbeddedText(int resource_id) {
  const HINSTANCE instance = GetModuleHandleW(nullptr);
  HRSRC resource = FindResourceW(instance, MAKEINTRESOURCEW(resource_id), MAKEINTRESOURCEW(10));
  if (resource == nullptr) return {};
  HGLOBAL loaded = LoadResource(instance, resource);
  if (loaded == nullptr) return {};
  const void* data = LockResource(loaded);
  const DWORD size = SizeofResource(instance, resource);
  if (data == nullptr || size == 0) return {};
  return std::string(static_cast<const char*>(data), static_cast<std::size_t>(size));
}

}  // namespace genie::core
