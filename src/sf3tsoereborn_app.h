// sf3tsoereborn - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <rex/rex_app.h>
#include <rex/ui/window.h>

#ifdef _WIN32
#include <Windows.h>
#include <timeapi.h>
#pragma comment(lib, "winmm.lib")
#endif

#include "icon.generated.h"
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
    //config.game_version = sf3tsoereborn::kVersionString;

    #ifdef _WIN32
        timeBeginPeriod(1);
    #endif
  }

  void OnPostSetup() override {
    window()->SetIcon(sf3tsoereborn::kIconPNG, sf3tsoereborn::kIconPNGSize);
  }

  // Override virtual hooks for customization:
  // void OnPostInitLogging() override {}
  // void OnLoadXexImage(std::string& xex_image) override {}
  // void OnPostLoadXexImage() override {}
  // void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {}
  // std::unique_ptr<rex::ui::ImGuiDialog> CreateAchievementsOverlay() override;
  // std::unique_ptr<rex::ui::AchievementNotificationDialog>
  // CreateAchievementNotificationDialog() override;
  // void OnShutdown() override {}
  // void OnConfigurePaths(rex::PathConfig& paths) override {}
};
