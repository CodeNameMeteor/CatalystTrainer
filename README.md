# Mirror's Edge Catalyst Trainer

An all in one trainer for **Mirror's Edge Catalyst**


## How to Use

### Installation
1. Download the latest release containing `MEC_Injector.exe` and `MEC_Trainer.dll`.
2. Place both files in the same folder.

### Injection
1. Run `MEC_Injector.exe` (Run as Administrator if necessary).
2. Launch **Mirror's Edge Catalyst**.
3. The injector will automatically detect the game, bypass the EA App splash screens, and wait for the main game window to become responsive before safely injecting the trainer.

### In-Game Usage
* Press **`M`** to toggle the trainer menu on and off.
* All hotkeys can be rebound inside the Settings tab of the menu. Your settings will automatically save to a `config.ini` file in the game's directory.

#### Default Hotkeys:
* `M` - Toggle Menu
* `1` - God Mode
* `2` - Noclip
* `3` - No Stumble
* `4` - Save Teleport Location
* `5` - Goto Teleport Location
* `F` - Bunnyhop
* `E` - Decrease Noclip Speed
* `R` - Increase Noclip Speed

---

## Known Issues


* Crashes after injecting when using full screen(Fix implemented, if it crashes try borderless window)

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
   cmake -B build
   ```
4. Compile the project in Release mode by running:
   ```cmd
   cmake --build build --config Release
   ```
5. Your compiled binaries will be output to `build\Release\MEC_Injector.exe` and `build\Release\MEC_Trainer.dll`.
