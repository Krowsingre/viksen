#pragma once
// Single point of inclusion for the JSON library (nlohmann/json, MIT, vendored in
// third_party/nlohmann). Keeping it behind one header makes replacement possible.
#include <nlohmann/json.hpp>

namespace atmos::core {
using Json = nlohmann::json;
}
