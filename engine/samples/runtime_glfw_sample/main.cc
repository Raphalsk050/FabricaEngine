#include "core/app/fabrica.h"
#include "core/logging/logger.h"

namespace {

constexpr int kKeyEscape = 256;
constexpr int kKeyQUppercase = 81;

/**
 * Stores movement tuning values for the sample player entity.
 */
struct PlayerController {
  float move_speed = 0.0f;
};

/**
 * Demonstrate end-to-end runtime flow using the GLFW window backend.
 *
 * This sample is the closest blueprint for game teams: configure the window,
 * register ECS components, map input actions, and run update/render callbacks
 * through the high-level `BaseView` API.
 *
 * @code
 * class MyView final : public Fabrica::Engine::BaseView {
 *  public:
 *   Status Awake() override { return BindInputAction("quit", 256); }
 * };
 * @endcode
 */
class RuntimeGlfwSampleView final : public Fabrica::Engine::BaseView {
 public:
  /**
   * Configure window dimensions and title before runtime initialization.
   */
  Status ConfigureWindow(WindowConfig& config) override {
    config.width = 1280;
    config.height = 720;
    config.title = "FabricaEngine Runtime GLFW Sample";
    return Status::Ok();
  }

  /**
   * Register ECS components and input mappings used by this sample.
   */
  Status Awake() override {
    FABRICA_LOG(Game, Info) << "[Game] Runtime GLFW sample awake";

    Status status = RegisterComponent<PlayerController>();
    if (!status.ok()) {
      return status;
    }

    player_entity_ = CreateEntity();
    status = player_entity_.AddComponent<PlayerController>(6.5f);
    if (!status.ok()) {
      return status;
    }

    status = BindInputAction("quit", kKeyEscape);
    if (!status.ok()) {
      return status;
    }

    return BindInputAction("quit", kKeyQUppercase);
  }

  Status Start() override {
    FABRICA_LOG(Game, Info) << "[Game] Runtime GLFW sample started";
    return Status::Ok();
  }

  /**
   * Print frame pacing information every update tick.
   */
  Status Update(const FrameContext& frame_context) override {
    FABRICA_LOG(Game, Info)
        << "[Game] Current FPS: " << GetCurrentFps(frame_context.delta_seconds);
    return Status::Ok();
  }

  /**
   * Stop runtime when the mapped quit action starts.
   */
  void OnInputAction(const InputActionEvent& event) override {
    if (event.action == "quit" && event.phase == InputActionPhase::kStarted) {
      FABRICA_LOG(Game, Info) << "[Game] Quit action received";
      RequestStop();
    }
  }

  void Shutdown() override {
    FABRICA_LOG(Game, Info) << "[Game] Runtime GLFW sample shutdown";
  }

 private:
  /**
   * Convert delta time to frames-per-second with a zero guard.
   */
  float GetCurrentFps(double delta_time) const {
    if (delta_time <= 0.0) {
      return 0.0f;
    }
    return static_cast<float>(1.0 / delta_time);
  }

  EntityHandle player_entity_;
};

}  // namespace

FABRICA_DEFINE_VIEW_MAIN(RuntimeGlfwSampleView)
