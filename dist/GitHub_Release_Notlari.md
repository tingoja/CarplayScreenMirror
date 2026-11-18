# GitHub v1.4.0 Release Notes

### Tag Name: `v1.4.0`
### Title: `CarplayScreenMirror Plugin v1.4.0 - ETS2 1.62+ Fix & High-End PC Stability Update`

---

### 🛠️ ETS2 1.62+ Compatibility & Crash Fixes (Güncelleme & Çökme Düzeltmeleri)
- **ETS2 1.62+ Signature & Texture Cache Fix:** Updated internal byte patterns for Custom Mirrors (`je rel8` short-jump optimization in 1.62+), GPS, and Dashboard screens, and added `resource_manager` LRU cache invalidation + `mem_tobj_ctor` hooking so textures replace reliably on every reload.
- **Multi-Core CPU Spinlock Synchronization:** Fixed a race condition in `memserver_texture_queue_processor` by acquiring the engine's internal spinlock (`memserver + 0x190`), preventing random `ACCESS_VIOLATION` crashes on high-core-count CPUs (Ryzen 7/9, Intel i7/i9).
- **D3D11 Startup Crash Fix (`d3d11.dll`):** Replaced `GetForegroundWindow()` dummy swapchain initialization with an isolated hidden window + WARP fallback, resolving startup crashes with ReShade, Steam Overlay, and NVIDIA App/Overlay.
- **Zero Render-Pipeline Overhead When Menu is Closed:** ImGui rendering and `OMSetRenderTargets` state binding now execute **only** when the `CTRL + F8` menu is open, eliminating micro-stutter and HDR/sRGB flicker on high-refresh-rate (144Hz/240Hz) monitors.
- **Multi-GPU / Optimus & Minimized Window Support:** Added automatic hardware adapter fallback, automatic background restore for minimized windows, and fallback capture support.

### ✨ Features Included
- **Clean, Simplified UI:** Streamlined controls with automatic background process filtering and instant live window switching.
- **Zoom & Pan Viewport Controls:** `1.0x - 5.0x` Zoom + `X` / `Y` Position sliders.
- **Screen Fit Modes:** `Stretch (Full)`, `Fit (Keep Aspect)`, `Fill (Crop)`, `Left Half`, `Right Half`.
- **Automatic Update Checker:** Background GitHub release check with timeout protection.

---

### 📦 Installation Guide / Kurulum
1. Download `CarplayScreenMirror.dll` from the release assets below.
2. Copy `CarplayScreenMirror.dll` into your game's plugins folder:
   `C:\Program Files (x86)\Steam\steamapps\common\Euro Truck Simulator 2\bin\win_x64\plugins\`
3. Launch Euro Truck Simulator 2 and press **CTRL + F8** in-game!
