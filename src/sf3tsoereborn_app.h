// sf3tsoereborn - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <filesystem>

#include <rex/rex_app.h>
#include <rex/input/input_system.h>
#include <rex/ui/window.h>

#ifdef _WIN32
#include <Windows.h>
#include <timeapi.h>
#pragma comment(lib, "winmm.lib")
#endif

#include "icon.generated.h"
#include "settings.h"
#include "version.generated.h"

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

    return true;
  }

  void OnPostSetup() override {
    // Runs the settings overlay's GPU plugin / Vulkan device enumeration
    // now, rather than paying for it on the player's first F4 press (the
    // SDK destroys and recreates the overlay on every close/open, so without
    // this it re-ran on every single open). See PrewarmSettingsDialogCaches.
    sf3tsoereborn::PrewarmSettingsDialogCaches();

    window()->SetIcon(sf3tsoereborn::kIconPNG, sf3tsoereborn::kIconPNGSize);
  }

  // Runtime (and therefore mod_registry()) exists now but mod plugins
  // haven't loaded yet -- ConstructRuntime calls OnPostLoadXexImage, then
  // loads mod plugins and dispatches their OnCreateDialogs afterward (see
  // rex_app.cpp). This is the last point to subscribe before a mod's
  // OnCreateDialogs might publish a "settings.language_option" or
  // "settings.native_string" event. See settings.h.
  void OnPostLoadXexImage() override {
    sf3tsoereborn::RegisterLanguageOptionsListener(runtime()->mod_registry());
    sf3tsoereborn::RegisterNativeStringListener(runtime()->mod_registry());
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
  // void OnLoadXexImage(std::string& xex_image) override {}
  // void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {}
  // std::unique_ptr<rex::ui::ImGuiDialog> CreateAchievementsOverlay() override;
  // std::unique_ptr<rex::ui::AchievementNotificationDialog>
  // CreateAchievementNotificationDialog() override;
  // void OnShutdown() override {}
  // void OnConfigurePaths(rex::PathConfig& paths) override {}

 private:
  std::filesystem::path user_settings_path() const {
    return user_data_root() / "settings.toml";
  }
};
