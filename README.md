# Mirror's Edge Catalyst Trainer

An all in one trainer for **Mirror's Edge Catalyst**


## How to Use

### Installation
1. Download the latest release containing `MEC_Injector.exe` and `MEC_Trainer.dll`.
2. Place both files in the same folder. The injector only loads the `MEC_Trainer.dll` that sits next to it.

### Injection
1. Run `MEC_Injector.exe`. It does **not** need to be run as administrator.
2. Launch **Mirror's Edge Catalyst**.
3. The injector will automatically detect the game, bypass the EA App splash screens, and wait for the main game window to become responsive before safely injecting the trainer.

If injection fails, the injector keeps its window open so you can read the error. A log of what the trainer does is written to `trainer_log.txt` next to the DLL.

### In-Game Usage
* Press **`F8`** to toggle the trainer menu on and off.
* The menu can be used with the mouse, the keyboard (arrow keys, Space/Enter, Tab) or a gamepad.
* All hotkeys, including the noclip movement keys, can be rebound inside the Settings tab of the menu.
* Keybinds and a few preferences (noclip speed, UI scale, overlay options, no-stumble and wall options) are saved to `trainer_config.ini` next to `MEC_Trainer.dll`. The file can also be edited with Notepad.
* The UI scales automatically with your resolution and Windows display scaling. You can set a fixed scale in Settings.
* Use **Unload / Eject Trainer** in Settings to put every changed game value back and remove the trainer without restarting the game.

#### Default Hotkeys:
* `F8` - Toggle Menu
* `1` - God Mode
* `2` - Noclip
* `3` - No Stumble
* `4` - Save Teleport Location
* `5` - Goto Teleport Location
* `F` - Bunnyhop
* `E` / `R` (hold) - Decrease / Increase Noclip Speed

#### Noclip Controls (defaults):
* `W` `A` `S` `D` - Move
* `Space` / `C` - Up / Down
* `Shift` (hold) - Faster, `Alt` (hold) - Slower

Please don't submit leaderboard or time trial results while using the trainer.

---

## Known Issues


* Exclusive fullscreen used to crash when the game recreated its swap chain (alt-tab, resolution change). This should now be fixed; if it still crashes, use borderless window and please report it with `trainer_log.txt`.
* The memory offsets are for one specific build of the game. The Settings tab shows the detected build; on a build known not to match, the trainer disables all of its memory writes.

---

## How To Compile

This project uses CMake for easy compilation.

### Prerequisites
* **CMake** (v3.20 or higher)
* **Visual Studio** (2019 or 2022) with the **"Desktop development with C++"** workload installed.

### Build Instructions
1. **Clone or download** the repository.
2. Open a terminal inside the root directory of the project.
3. Generate the project files by running:
   ```cmd
   cmake -B build -A x64
   ```
4. Compile the project in Release mode by running:
   ```cmd
   cmake --build build --config Release
   ```
5. Your compiled binaries will be output to `build\Release\MEC_Injector.exe` and `build\Release\MEC_Trainer.dll`.
6. Optionally run the unit tests:
   ```cmd
   ctest --test-dir build -C Release
   ```

Every push and pull request is also built by GitHub Actions (`.github/workflows/build.yml`).

### Supporting a game build
`include/Game/GameOffsets.hpp` contains `BuildInfo::kSupportedTimeDateStamp` and `kSupportedSizeOfImage`. Set them to the values shown in the trainer's Settings tab on the build the offsets were made for. Once set, the trainer refuses to write memory on any other build.

---

## Third-Party Software

This project uses Dear ImGui, kiero and MinHook. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
