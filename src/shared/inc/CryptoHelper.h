#pragma once

#include "env.h"
constexpr const char CURVE_NAME_WITH_ID[] = "prime256v1:0xF0000100";
constexpr const char KEY_REF[] = "nxp:0xF0000100";
constexpr const char PROVIDER_SO_PATH[] = "/opt/MainApp/App/nxp_pnt/lib/libsssProvider.so";

namespace CryptoHelper {
bool GenerateCertificate(bool& use_nxp);
bool HasNXP();
void CleanUpProvider();
}  // namespace CryptoHelper