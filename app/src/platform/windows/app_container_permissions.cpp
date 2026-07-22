#include "pch.hpp"

#include "platform/windows/app_container_permissions.hpp"

#include <aclapi.h>
#include <sddl.h>

namespace minimize::platform {

bool GrantAppContainerPermissions(const std::wstring& path) {
  std::wstring normalized_path = path;
  if (normalized_path.size() > 1 &&
      (normalized_path.back() == L'\\' || normalized_path.back() == L'/')) {
    normalized_path.pop_back();
  }

  PACL old_dacl = nullptr;
  PSECURITY_DESCRIPTOR descriptor = nullptr;
  DWORD error =
      GetNamedSecurityInfoW(normalized_path.data(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
                            nullptr, nullptr, &old_dacl, nullptr, &descriptor);
  if (error != ERROR_SUCCESS) return false;

  PSID package_sid = nullptr;
  PSID restricted_package_sid = nullptr;
  bool success = false;
  if (ConvertStringSidToSidW(L"S-1-15-2-1", &package_sid) &&
      ConvertStringSidToSidW(L"S-1-15-2-2", &restricted_package_sid)) {
    EXPLICIT_ACCESSW entries[2]{};
    PSID sids[2] = {package_sid, restricted_package_sid};
    for (std::size_t index = 0; index < 2; ++index) {
      entries[index].grfAccessPermissions = GENERIC_READ | GENERIC_EXECUTE;
      entries[index].grfAccessMode = GRANT_ACCESS;
      entries[index].grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
      entries[index].Trustee.TrusteeForm = TRUSTEE_IS_SID;
      entries[index].Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
      entries[index].Trustee.ptstrName = reinterpret_cast<LPWSTR>(sids[index]);
    }
    PACL new_dacl = nullptr;
    error = SetEntriesInAclW(2, entries, old_dacl, &new_dacl);
    if (error == ERROR_SUCCESS) {
      error = SetNamedSecurityInfoW(normalized_path.data(), SE_FILE_OBJECT,
                                    DACL_SECURITY_INFORMATION, nullptr, nullptr, new_dacl, nullptr);
      success = error == ERROR_SUCCESS;
      LocalFree(new_dacl);
    }
  }
  if (package_sid != nullptr) LocalFree(package_sid);
  if (restricted_package_sid != nullptr) LocalFree(restricted_package_sid);
  LocalFree(descriptor);
  return success;
}

}  // namespace minimize::platform
