// sf3tsoereborn - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <filesystem>

#include <rex/cvar.h>
#include <rex/rex_app.h>
#include <rex/input/input_system.h>
#include <rex/runtime.h>
#include <rex/system/game_data_selector.h>
#include <rex/version.h>
#include <rex/ui/imgui_drawer.h>
#include <rex/ui/imgui_theme.h>
#include <rex/ui/window.h>
#include <imgui.h>
#include <rex/ui/window.h>

#ifdef _WIN32
#include <Windows.h>
#include <timeapi.h>
#pragma comment(lib, "winmm.lib")
#endif

#include "icon.generated.h"
#include "settings.h"
#include "version.generated.h"

// Single on/off vsync toggle for this renderer, layered on top of the SDK's
// vulkan_allow_present_mode_immediate cvar (vulkan_presenter.cpp): disabling
// it drops "immediate" (tearing) out of swapchain present-mode selection,
// leaving mailbox (tear-free, non-blocking -- see OnPreLaunchModule below
// for why mailbox and not FIFO) as the effective vsync-on mode.
REXCVAR_DEFINE_BOOL(vsync, true, "Video", "Enable vsync");

// Defined in rexglue-sdk/src/ui/vulkan/vulkan_presenter.cpp; the vsync cvar
// above drives this rather than duplicating present-mode selection here.
REXCVAR_DECLARE(bool, vulkan_allow_present_mode_immediate);

class Sf3tsoerebornApp : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<Sf3tsoerebornApp>(new Sf3tsoerebornApp(ctx, "sf3tsoereborn",
        PPCImageConfig));
  }
  
  void OnPreSetup(rex::RuntimeConfig& config) override {
    // Lets a mod.toml pin a minimum build via `game_version = ">= x.y.z"`,
    // validated at Setup() alongside `requires`/`conflicts` -- see
    // docs/making-mods.md. Derived from the nearest git tag at configure
    // time (src/version.generated.h, see CMakeLists.txt).
    config.game_version = sf3tsoereborn::kVersionString;

    // One-shot toast shown top-left as the game starts
    config.startup_hint = "Press F4 to open settings.";

    #ifdef _WIN32
        timeBeginPeriod(1);
    #endif
  }

  bool SetupEnvironment() override {
    // Game defaults for SDK cvars must land before the SDK loads any config
    // file, so a saved/CLI/env override still wins.
    sf3tsoereborn::ApplySettingDefaults();

    if (!rex::ReXApp::SetupEnvironment())
      return false;

    // User-facing settings (Fullscreen, Resolution, ...) live in their own
    // file, separate from the advanced cvars in the app's normal config, and
    // are loaded last so they win over both.
    if (std::filesystem::exists(user_settings_path()))
      rex::cvar::LoadConfig(user_settings_path());

    rex::system::GameDataSelectorSettings settings;
    settings.is_xbla = true;
    settings.default_xex_sha256 = "1eb116026b64737a63695977cc25f44104cbfed5d76f11777f34682c26cfab6e";
#ifdef SF3TSOEREBORN_TU
    settings.title_update_sha256 = "063dd5e9bbc4d62e76ff9a46c036293fb983d4799e979737b97bd562b67b898f";
#endif
    settings.config_path = config_path();

    return rex::system::GameDataSelector::EnsureGameData(settings);
  }

  void OnPostSetup() override {
    // Runs the settings overlay's GPU plugin / Vulkan device enumeration
    // now, rather than paying for it on the player's first F4 press (the
    // SDK destroys and recreates the overlay on every close/open, so without
    // this it re-ran on every single open). See PrewarmSettingsDialogCaches.
    sf3tsoereborn::PrewarmSettingsDialogCaches();

    window()->SetIcon(sf3tsoereborn::kIconPNG, sf3tsoereborn::kIconPNGSize);

    // Feed the F3 debug overlay's "Guest FPS" readout. RegisterTick fires once
    // per guest frame on GPU swap (command-processor thread); the counter is
    // polled from the UI thread each time the overlay redraws to derive an
    // instantaneous FPS, smoothed by only recomputing every ~0.2s.
    runtime()->mod_registry()->RegisterTick(
        [this] { guest_frame_count_.fetch_add(1, std::memory_order_relaxed); });
    SetGuestFrameStats([this]() -> rex::ui::FrameStats {
      rex::ui::FrameStats stats;
      uint64_t count = guest_frame_count_.load(std::memory_order_relaxed);
      stats.frame_count = count;

      auto now = std::chrono::steady_clock::now();
      double elapsed = std::chrono::duration<double>(now - fps_poll_time_).count();
      if (elapsed >= 0.2) {
        fps_smoothed_ = static_cast<double>(count - fps_poll_frame_count_) / elapsed;
        fps_poll_time_ = now;
        fps_poll_frame_count_ = count;
      }
      stats.fps = fps_smoothed_;
      stats.frame_time_ms = fps_smoothed_ > 0.0 ? 1000.0 / fps_smoothed_ : 0.0;
      return stats;
    });
  }

  // Curated player-facing settings dialog on F4 (see settings.h); returning
  // nullptr here would fall back to the SDK's developer settings panel.
  std::unique_ptr<rex::ui::ImGuiDialog> OnCreateUserSettingsOverlay() override {
    return sf3tsoereborn::CreateSettingsDialog(
        imgui_drawer(), window(), user_settings_path(), config_path(),
        static_cast<rex::input::InputSystem*>(runtime()->input_system()));
  }

  // Override virtual hooks for customization:
  // void OnPostInitLogging() override {}
  // void OnPostLoadXexImage() override {}
  // void OnLoadXexImage(std::string& xex_image) override {}
  // void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {}
  // std::unique_ptr<rex::ui::ImGuiDialog> CreateAchievementsOverlay() override;
  // std::unique_ptr<rex::ui::AchievementNotificationDialog>
  // CreateAchievementNotificationDialog() override;
  // void OnShutdown() override {}
  // void OnConfigurePaths(rex::PathConfig& paths) override {}

 private:
  std::atomic<uint64_t> guest_frame_count_{0};
  std::chrono::steady_clock::time_point fps_poll_time_ = std::chrono::steady_clock::now();
  uint64_t fps_poll_frame_count_ = 0;
  double fps_smoothed_ = 0.0;

  std::filesystem::path user_settings_path() const {
    return user_data_root() / "settings.toml";
  }
};
