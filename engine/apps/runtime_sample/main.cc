#include "core/app/fabrica.h"
#include "core/logging/logger.h"

#include <string>

namespace {

constexpr int kKeyEscape = 256;
constexpr int kKeyQUppercase = 81;

/**
 * Holds movement tuning values for the sample player entity.
 */
struct PlayerController {
  float move_speed = 0.0f;
};

/**
 * Demonstrates the end-to-end runtime view lifecycle.
 *
 * The sample registers one component, spawns one entity, binds quit actions,
 * and logs per-frame FPS as a reference integration for new projects.
 */
class RuntimeSampleView final : public Fabrica::Engine::BaseView {
 public:
  /**
   * Configure sample window dimensions and title.
   */
  Status ConfigureWindow(WindowConfig& config) override {
    config.width = 1280;
    config.height = 720;
    config.title = "FabricaEngine Runtime View Sample";
    return Status::Ok();
  }

  /**
   * Register sample ECS state and input bindings.
   */
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

  /// Log sample startup marker.
  Status Start() override {
    FABRICA_LOG(Game, Info) << "[Game] Runtime sample view started";
    return Status::Ok();
  }

  /**
   * Log current frame rate every update tick.
   */
  Status Update(const FrameContext& frame_context) override {
    FABRICA_LOG(Game, Info)
        << "[Game] Current FPS is: " << GetCurrentFPS(frame_context.delta_seconds);
    return Status::Ok();
  }

  /**
   * Stop runtime when mapped quit action starts.
   */
  void OnInputAction(const InputActionEvent& event) override {
    if (event.action == "quit" && event.phase == InputActionPhase::kStarted) {
      FABRICA_LOG(Game, Info) << "[Game] Quit action received";
      RequestStop();
    }
  }

  /// Log sample shutdown marker.
  void Shutdown() override {
    FABRICA_LOG(Game, Info) << "[Game] Runtime sample view shutdown";
  }

 private:
  EntityHandle player_entity_;

  /**
   * Convert delta time to frames-per-second with zero guard.
   */
  float GetCurrentFPS(double delta_time) const {
    if (delta_time > 0.0) {
      return static_cast<float>(1.0 / delta_time);
    }
    return 0.0f;
  }
};

}  // namespace

FABRICA_DEFINE_VIEW_MAIN(RuntimeSampleView)
