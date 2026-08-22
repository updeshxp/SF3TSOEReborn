// sf3tsoereborn - ReXGlue Recompiled Project
//
// Game-curated settings: the game's own defaults for SDK cvars, and a small
// player-facing settings overlay (Fullscreen/Resolution, plus a collapsed
// Advanced section) that replaces the SDK's developer settings panel on F4
// when `settings_manager_enabled = true`. See rex::cvar::SetDefaultValue,
// rex::cvar::SaveConfigSubset, and rex::ui::DrawCvarWidget in the SDK for the
// generic mechanism this builds on.
//
// Ported from NocturneRecomp's src/settings.{h,cpp} (same ReXGlue SDK
// generation), adapted for this game: Nocturne's graphics_style_enhanced /
// frame pacer / rando / fast-forward cvars have no equivalent here, and the
// seeded defaults reflect SF3:TSOE's shipped sf3tsoereborn.toml (xenos GPU
// plugin, fighting-game keyboard layout).

#pragma once

#include <filesystem>
#include <memory>
#include <string_view>
#include <vector>

#include <rex/ui/imgui_dialog.h>

namespace rex::ui {
class ImGuiDrawer;
class Window;
}  // namespace rex::ui

namespace rex::input {
class InputSystem;
}  // namespace rex::input

namespace rex::system {
class ModRegistry;
}  // namespace rex::system

namespace sf3tsoereborn {

// Perceptual mapping between the `audio_volume` cvar's linear amplitude
// (0.0-1.0) and a displayed 0-100 percentage, evenly spaced in dB rather than
// in amplitude (-40dB at 0%, 0dB at 100%). Exported so a future in-game
// (guest-driven) options screen can share the same curve with this overlay.
double VolumeAmplitudeFromPercent(int percent);
int VolumePercentFromAmplitude(double amplitude);

// Overrides the SDK's built-in cvar defaults with the game's own. Call once,
// before rex::ReXApp::SetupEnvironment() (i.e. before any config file is
// loaded), so a saved config or CLI/env override still takes precedence.
void ApplySettingDefaults();

// Subscribes to the "settings.language_option" mod-registry event so a mod
// can add its own entry to the Language dropdown without patching this file.
// A publishing mod calls, from its own IModPlugin::OnCreateDialogs:
//
//   rex::system::ModRegistry::EventPayload payload;
//   payload.u64 = 9;  // XLanguage id, e.g. Portuguese
//   const char* label = "Portuguese";
//   payload.bytes = {reinterpret_cast<const uint8_t*>(label), strlen(label)};
//   runtime->mod_registry()->Publish("settings.language_option", payload);
//
// Must be called after Runtime exists but before any mod's OnCreateDialogs
// runs -- true for the call site in sf3tsoereborn_app.h's own
// OnPostLoadXexImage, which the SDK always calls right before it loads mod
// plugins and dispatches their OnCreateDialogs (see rex_app.cpp's
// ConstructRuntime). Duplicate ids (already built in, or already registered
// by an earlier mod) are ignored with a warning, same first-wins rule as
// RegisterAddress.
void RegisterLanguageOptionsListener(rex::system::ModRegistry* registry);

// One entry in the Language dropdown: `id` is the stringified XLanguage
// value stored by the `user_language` cvar, `label` its display text.
struct LanguageOption {
  const char* id;
  const char* label;
};

// The built-in six languages, followed by anything mods registered via
// RegisterLanguageOptionsListener's event, in registration order. Rebuilt
// fresh on every call (cheap), so it's always current regardless of when
// mods finish registering relative to the caller.
std::vector<LanguageOption> GetLanguageOptions();

// Subscribes to the "settings.native_string" mod-registry event, letting a
// mod translate a native in-game options screen's own row text into a
// language it added via RegisterLanguageOptionsListener. Only for text a
// host-side options screen synthesizes itself; no such screen exists in
// this project yet, but the listener is registered anyway so mods can
// publish translations ahead of one being added. A publishing mod calls,
// from its own IModPlugin::OnCreateDialogs, once per string:
//
//   rex::system::ModRegistry::EventPayload payload;
//   payload.u64 = 9;  // XLanguage id this translation applies to
//   const char* kv = "resolution_label=Resolução";  // "key=value", UTF-8
//   payload.bytes = {reinterpret_cast<const uint8_t*>(kv), std::strlen(kv)};
//   runtime->mod_registry()->Publish("settings.native_string", payload);
//
// Same call-timing contract as RegisterLanguageOptionsListener. A malformed
// payload (no '=', or an empty key/value) is ignored with a warning; a
// duplicate (language id, key) pair keeps the first registration.
void RegisterNativeStringListener(rex::system::ModRegistry* registry);

// Looks up a mod-published translation (see RegisterNativeStringListener)
// for `key` in XLanguage `language_id`. Returns nullptr if none was
// registered. The returned string is UTF-16, converted from the published
// UTF-8 value; it is owned by the registry and valid for the process
// lifetime.
const char16_t* FindNativeStringTranslation(uint32_t language_id, std::string_view key);

// Runs the GPU plugin / Vulkan device enumeration that CreateSettingsDialog's
// dropdowns need, once, and caches the results for every dialog instance
// created afterwards. Both enumerations are expensive (EnumerateDevices
// creates and tears down a whole VkInstance to query physical devices) --
// without this, that cost lands on the first frame after the player presses
// F4, since the SDK destroys and recreates the dialog on every close/open
// (see rex_app.cpp's user_settings_overlay_ handling) rather than keeping one
// alive. Call once after graphics setup, while the game's own render loop is
// still idle/loading, so the hitch happens there instead of on the player's
// first F4 press.
void PrewarmSettingsDialogCaches();

// Creates the curated settings overlay. `user_settings_path` is where the
// friendly settings (Fullscreen, Resolution) are persisted;
// `app_config_path` is the SDK's normal cvar config file. `window` is used
// by the "Restart Now" button on the pending-restart banner: it relaunches
// the process (rex::platform::process::Relaunch) then requests `window`
// close so the new instance picks up the just-changed cvars. `input_system`
// is forwarded to the SDK's own rex::ui::SettingsDialog, opened on demand
// via the "All Settings..." button, so its gamepad rebind capture works the
// same as it does from F4; may be null.
std::unique_ptr<rex::ui::ImGuiDialog> CreateSettingsDialog(
    rex::ui::ImGuiDrawer* drawer, rex::ui::Window* window,
    std::filesystem::path user_settings_path, std::filesystem::path app_config_path,
    rex::input::InputSystem* input_system = nullptr);

}  // namespace sf3tsoereborn


