# SF3TSOEReborn

Static recompilation of **Street Fighter III: 3rd Strike Online Edition** (Xbox Live Arcade) for Windows
and Linux, built on the [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk).

This project converts the Xbox 360 PowerPC `default.xex` into native x86_64 and arm64 executable at build time, then wraps it with a small host runtime (logging,
overlays, hooks) while visible rendering currently remains authoritative in the vendored RexGlue/Xenia graphics backend.

**You must own the game.** This project does **not** ship any copyrighted code, data, or assets. You provide your own legally dumped game.

## What You Need

- Python 3.10 or newer for the extraction helper.
- A Street Fighter III: 3rd Strike Online Edition Xbox 360 STFS package. The expected title id
  is `58410B16`, and XBLA packages normally live under a path like:

```text
...\58410B16\000D0000\<long package file name>
```
Do this:

1. Extract the release you just downloaded
2. Run `python scripts/extract_game_xbla.py` from the release directory and extract game assets in assets/ folder.

Finally, run the game executable to play the game.

## Building from scratch (development)

### 0. Install dependencies

#### Linux (Arch/CachyOS)
```bash
paru -S clang20 cmake ninja vulkan-headers
```

#### Windows
```powershell
scoop install llvm cmake ninja
```

### 1. Clone

```bash
git clone https://github.com/updeshxp/SF3TSOEReborn
cd SF3TSOEReborn
```

### 2. Download the ReXGlue SDK

```bash
python scripts/download-sdk.py --pinned
```

### 3. Provide your game

Place your legally dumped XBLA package into the current directory, then extract it into `assets/`:

```bash
python scripts/extract_game_xbla.py
```

### 4. Build

Use this script:

```bash
# Vanilla
python scripts/build.py --release

# Title Update
python scripts/build.py --release --tu /path/to/TU_*
```

### 5. Run

```bash
python scripts/run.py
```

This runs the freshly built executable with the correct CLI arguments
(`--game_data_root=assets`, `--gpu_plugin=xenos`, `--license_mask=1`).

Any extra arguments are forwarded to the executable, e.g.:

```bash
python scripts/run.py --vulkan_device 1
```

## Options

Options can be persisted by adding them to `sf3tsoereborn.toml` next to the game executable, for example:

```toml
vulkan_device = 1 # NVIDIA GPU
user_language = 1 # English
```

### Keyboard & mouse

Keyboard and mouse controls are enabled by default. All bindings are overridable in the **F4** menu or `sf3tsoereborn.toml`. For example:

```toml
keybind_a = "J"
keybind_left_trigger = "O"
```

### Logging

The game writes logs into the `logs` directory by default, but you can configure it.

```bash
python scripts/run.py --log_file sf3tsoereborn.log --log_level debug
```

## Adding a hook

1. Find the guest address in `default.xex`.
2. Add to `sf3tsoereborn_config.toml`:

   ```toml
   [functions]
   0x8XXXXXXX = {name = "MyFunction"}
   ```

3. Implement in `src/sf3tsoereborn_hooks.cpp` (create if it doesn't exist, and add it to `CMakeLists.txt`):

   ```cpp
   void MyFunction(PPCContext& ctx, uint8_t* base) {
       // your logic
   }
   ```

4. Re-run codegen and rebuild.

## Adding a midasm hook (inline patch)

```toml
[[midasm_hook]]
address = 0x8XXXXXXX
name = "MyHook"
registers = ["r3"]
return = true
```

Implement in `src/sf3tsoereborn_hooks.cpp`:

```cpp
void MyHook(PPCRegister& r3) {
    r3.u32 = 1;
}
```

## Credits

- [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk)
- [TheDarkness](https://github.com/birabittoh/TheDarkness)
- [NocturneRecomp](https://github.com/birabittoh/NocturneRecomp)

## License

The host-side source in `src/`, build scripts, and CI config are available
under the MIT License.

The recompiled game code produced at build time contains symbols and logic
from sf3tsoerborn and is **not** redistributable. Do not share
`default.xex`, the `generated/` directory, or any built binary that links
against them.
