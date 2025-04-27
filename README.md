Great! Here's the improved and more "professional" version of the `README.md`, now including nice badges at the top and a cooler style while keeping the rigor:

---

# 🎮 ZX Spectrum Emulator (Work in Progress)

![License: MIT](https://img.shields.io/badge/license-MIT-green.svg)
![Status: Early Development](https://img.shields.io/badge/status-WIP-yellow.svg)
![Language: C](https://img.shields.io/badge/language-C-blue.svg)

---

This repository contains the early stages of a **ZX Spectrum emulator**, developed in C.

The emulator uses a **proven Z80 CPU core** created by **Nicolas Allemand** (licensed under MIT License).  
The **ZX Spectrum-specific emulation logic** — including memory mapping, basic I/O framework, and system setup — was written by me in the `main.c` file.

> ⚡️ **Goal**: Build a reliable, lightweight ZX Spectrum emulator, starting from a trusted CPU core to focus on accurate hardware emulation.

---

## 🛠️ Project Structure

- `Z80.c` / `Z80.h` — Z80 CPU emulator (Copyright © 2019 Nicolas Allemand)
- `main.c` — ZX Spectrum 48K system emulation (my code)

---

## ✨ Current Features

- ✅ Integration with Z80 CPU core
- ✅ Basic memory mapping (48KB RAM + 16KB ROM)
- ✅ ROM loading and execution
- ✅ Basic I/O framework (for future peripherals)

---

## 🚀 Next Steps

- 🔜 Full keyboard input emulation
- 🔜 Video output (ULA graphics emulation)
- 🔜 Sound support
- 🔜 Loading programs (TAP/TZX file support)

---

## ❓ Why Start with an Existing Z80 Core?

Although I have previously written my own Z80 emulator in C, for this project I decided to use **a proven, stable core** to **focus on system emulation first**.  
Later, I may replace it with my own Z80 implementation once the ZX Spectrum-specific parts are mature.

This approach allows for faster development and avoids CPU-related debugging during the initial phases.

---

## 📄 License

This project is licensed under the [MIT License](LICENSE), respecting the terms of the original Z80 core by Nicolas Allemand.

> **Note**:  
> The Z80 CPU core is © 2019 Nicolas Allemand, provided under the MIT License.  
> The ZX Spectrum system code (`main.c`) was developed independently for this project.

