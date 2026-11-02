#pragma once
#include <string>

namespace atmos::core {
std::string makeUuid();          // RFC 4122 version 4, lowercase
std::string utcTimestampNow();   // ISO 8601, e.g. 2026-11-02T10:15:30Z
}
