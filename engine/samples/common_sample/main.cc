#include "core/common/assert.h"
#include "core/common/status.h"
#include "core/common/typed_id.h"

#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {

using Fabrica::Core::Status;
using Fabrica::Core::StatusOr;

struct SceneIdTag {};
using SceneId = Fabrica::Core::TypedId<SceneIdTag, std::uint32_t>;

/**
 * Parse a decimal scene identifier into a strongly typed id.
 *
 * The sample keeps parsing at the boundary and returns `StatusOr` so callers
 * can propagate diagnostics without relying on exceptions.
 *
 * @param text Decimal representation of the scene identifier.
 * @return Parsed `SceneId` when valid and greater than zero.
 */
StatusOr<SceneId> ParseSceneId(std::string_view text) {
  if (text.empty()) {
    return Status::InvalidArgument("Scene id cannot be empty");
  }

  std::uint32_t raw_value = 0;
  const char* begin = text.data();
  const char* end = begin + text.size();
  const auto [parsed_end, error] = std::from_chars(begin, end, raw_value);
  if (error != std::errc() || parsed_end != end || raw_value == 0) {
    return Status::InvalidArgument("Scene id must be a non-zero integer");
  }

  return SceneId(raw_value);
}

/**
 * Validate a typed scene id before using it in gameplay code.
 *
 * @param scene_id Candidate identifier.
 * @return `Status::Ok()` when id is valid.
 */
Status ValidateSceneId(SceneId scene_id) {
  return FABRICA_VERIFY(scene_id.IsValid(), "Scene id must be valid");
}

}  // namespace

int main() {
  const StatusOr<SceneId> parsed_scene = ParseSceneId("42");
  if (!parsed_scene.ok()) {
    std::cerr << "[common_sample] Parse failed: "
              << parsed_scene.status().ToString() << '\n';
    return EXIT_FAILURE;
  }

  const Status validate_status = ValidateSceneId(parsed_scene.value());
  if (!validate_status.ok()) {
    std::cerr << "[common_sample] Validation failed: "
              << validate_status.ToString() << '\n';
    return EXIT_FAILURE;
  }

  const StatusOr<SceneId> invalid_scene = ParseSceneId("0");
  if (invalid_scene.ok()) {
    std::cerr << "[common_sample] Expected invalid parse for zero id\n";
    return EXIT_FAILURE;
  }

  std::cout << "[common_sample] Parsed scene id=" << parsed_scene.value().Value()
            << " and rejected invalid input as expected\n";
  return EXIT_SUCCESS;
}
