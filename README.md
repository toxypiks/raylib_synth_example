# Raylib Synth & Looper

A simple real-time software synthesizer with looper functionality written in C using [raylib](https://www.raylib.com/) and [`stb_ds.h`](https://github.com/nothings/stb).

## Features

* **Sine-Wave Synthesizer:** Real-time polyphonic sound generation.
* **MIDI-Style Looper:** Records Note-On and Note-Off events quantized to musical grids and plays them back seamlessly in a loop.
* **Visual Feedback:** Bar timeline, metronome indicator, and state status colors (Replay, Waiting, Record).

## Requirements

Before building the project, make sure you have the following tools and dependencies installed on your system:

* **C Compiler:** `gcc` or `clang` (C99 support or newer)
* **Build System:** `CMake` (version 3.11 or higher) and `make` (or `ninja`)
* **Raylib Dependencies (Arch Linux):**
  Install `base-devel`, `cmake`, and the required X11, OpenGL, and ALSA/PulseAudio development packages:
  ```bash
  sudo pacman -S base-devel cmake raylib libx11 libxft libxinerama libxcursor libxi alsa-lib
  ```

---

## Quick Start

### 1. Clone the repository

```bash
git clone [https://github.com/toxypiks/raylib_synth_example.git](https://github.com/toxypiks/raylib_synth_example.git)
cd raylib_synth_example
```

### 2. Build the project

```bash
mkdir build
cd build
cmake ..
make
```

### 3. Run the application

```bash
./main
```

---

## Controls

* **Keyboard (Notes):** `Z`, `S`, `X`, `D`, `C`, `V`, `G`, `B`, `H`, `N`, `J`, `M`, `,` (Semitones from C to C')
* **`Spacebar`:**
  * In **Replay Mode:** Switches to **Waiting until end of bar** (clears old recordings and waits for the next measure to start recording).
  * In **Record Mode:** Stops recording and switches back to **Replay Mode** (loops the recorded pattern).
