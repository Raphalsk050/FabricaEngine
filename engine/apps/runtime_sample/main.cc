#include "core/app/fabrica.h"
#include "core/logging/logger.h"
#include <string>

namespace {

constexpr int kKeyEscape = 256;
constexpr int kKeyQUppercase = 81;

struct PlayerController {
  float move_speed = 0.0f;
};

class RuntimeSampleView final : public Fabrica::Engine::BaseView {
 public:
  Status ConfigureWindow(WindowConfig& config) override {
    config.width = 1280;
    config.height = 720;
    config.title = "FabricaEngine Runtime View Sample";
    return Status::Ok();
  }

  Status Awake() override {
    FABRICA_LOG(Game, Info) << "[Game] Runtime sample view awake";

    const Status register_status = RegisterComponent<PlayerController>();
    if (!register_status.ok()) {
      return register_status;
    }

    player_entity_ = CreateEntity();
    const Status add_component_status =
        player_entity_.AddComponent<PlayerController>(6.5f);
    if (!add_component_status.ok()) {
      return add_component_status;
    }

    const Status bind_escape_status = BindInputAction("quit", kKeyEscape);
    if (!bind_escape_status.ok()) {
      return bind_escape_status;
    }

    const Status bind_q_status = BindInputAction("quit", kKeyQUppercase);
    if (!bind_q_status.ok()) {
      return bind_q_status;
    }

    return Status::Ok();
  }

  Status Start() override {
    FABRICA_LOG(Game, Info) << "[Game] Runtime sample view started";
    return Status::Ok();
  }

  Status Update(const FrameContext& frame_context) override {
    FABRICA_LOG(Game, Info) << "[Game] Current FPS is: " << GetCurrentFPS(frame_context.delta_seconds);
    return Status::Ok(); }

  void OnInputAction(const InputActionEvent& event) override {
    if (event.action == "quit" && event.phase == InputActionPhase::kStarted) {
      FABRICA_LOG(Game, Info) << "[Game] Quit action received";
      RequestStop();
    }
  }

  void Shutdown() override {
    FABRICA_LOG(Game, Info) << "[Game] Runtime sample view shutdown";
  }

 private:
  EntityHandle player_entity_;

  float GetCurrentFPS(double delta_time) const {
    if (delta_time > 0.0) {
      return 1.0 / delta_time;
    }
    return 0.0;
  }
};

}  // namespace

FABRICA_DEFINE_VIEW_MAIN(RuntimeSampleView)
