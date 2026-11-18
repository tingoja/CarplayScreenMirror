#include <Windows.h>
#include <scs_sdk/scssdk_telemetry.h>
#include "scs_logging.h"

#include "version.h"
#include "updater.h"

#include "sources/wgc_dispatcher.h"
#include <MinHook/MinHook.h>
#include "bmem.h"
#include "screens.h"
#include "win32/win32.h"
#include "dx11/dx11.h"
#include "dinput8/dinput8.h"
#include "prism/prism.h"
#include "menu/menu.h"

using namespace scs_logging;
#pragma comment(lib, "minhook.x64.lib")

#ifdef _DEBUG
#pragma comment(lib, "ImGuiD.lib")
#else
#pragma comment(lib, "ImGuiR.lib")
#endif

void update_screen_patches()
{
    static bool gps_patched{};
    static bool dash_patched{};
    static bool custom_patched{};

    bool has_gps{};
    bool has_dash{};
    bool has_custom{};

    {
        std::lock_guard<std::mutex> lock(g_screens_mutex);
        for (auto& screen : g_screens)
        {
            if (screen.type == screen_type_t::GPS)
                has_gps = true;
            else if (screen.type == screen_type_t::DASHBOARD)
                has_dash = true;
            else if (screen.type == screen_type_t::CUSTOM)
                has_custom = true;
        }
    }

    if (has_gps != gps_patched)
    {
        static uint64_t patch_addr{};
        static bool scan_attempted{};
        if (!patch_addr && !scan_attempted) {
            scan_attempted = true;
            uint64_t scan_result = bmem::patternScan("49 8B 96 ?? ?? ?? ?? 48 85 D2 0F 84 ?? ?? ?? ?? 4D 8B 86");
            if (scan_result) {
                patch_addr = scan_result + 10;
            } else {
                scs_log(1, "[dllmain] GPS patch pattern not found on this game version.");
            }
        }

        if (patch_addr) {
            DWORD oldProtect;
            if (VirtualProtect((void*)patch_addr, 2, PAGE_EXECUTE_READWRITE, &oldProtect)) {
                if (has_gps) {
                    *(reinterpret_cast<uint8_t*>(patch_addr + 0)) = 0x90;
                    *(reinterpret_cast<uint8_t*>(patch_addr + 1)) = 0xE9;
                } else {
                    *(reinterpret_cast<uint8_t*>(patch_addr + 0)) = 0x0F;
                    *(reinterpret_cast<uint8_t*>(patch_addr + 1)) = 0x84;
                }
                VirtualProtect((void*)patch_addr, 2, oldProtect, &oldProtect);
                gps_patched = has_gps;
                scs_log(0, "[dllmain] GPS screen patch %s", has_gps ? "applied" : "restored");
            }
        }
    }

    if (has_dash != dash_patched)
    {
        static uint64_t patch_addr{};
        static bool scan_attempted{};
        if (!patch_addr && !scan_attempted) {
            scan_attempted = true;
            uint64_t scan_result = bmem::patternScan("4C 8D 3D ?? ?? ?? ?? 48 85 D2 0F 84");
            if (scan_result) {
                patch_addr = scan_result + 10;
            } else {
                scs_log(1, "[dllmain] Dashboard patch pattern not found on this game version.");
            }
        }

        if (patch_addr) {
            DWORD oldProtect;
            if (VirtualProtect((void*)patch_addr, 2, PAGE_EXECUTE_READWRITE, &oldProtect)) {
                if (has_dash) {
                    *(reinterpret_cast<uint8_t*>(patch_addr + 0)) = 0x90;
                    *(reinterpret_cast<uint8_t*>(patch_addr + 1)) = 0xE9;
                } else {
                    *(reinterpret_cast<uint8_t*>(patch_addr + 0)) = 0x0F;
                    *(reinterpret_cast<uint8_t*>(patch_addr + 1)) = 0x84;
                }
                VirtualProtect((void*)patch_addr, 2, oldProtect, &oldProtect);
                dash_patched = has_dash;
                scs_log(0, "[dllmain] Dashboard screen patch %s", has_dash ? "applied" : "restored");
            }
        }
    }

    if (has_custom != custom_patched)
    {
        static uint64_t patch_addr{};
        static bool is_short_jump{};
        static bool scan_attempted{};
        if (!patch_addr && !scan_attempted) {
            scan_attempted = true;
            // ETS2 1.62+ short jump (74 rel8 -> je short)
            uint64_t short_scan = bmem::patternScan("49 3B F6 74 ?? 45 84 E4 4D 0F 45 FD");
            if (short_scan) {
                patch_addr = short_scan + 3;
                is_short_jump = true;
            } else {
                // ETS2 1.60 / 1.61 near jump (0F 84 rel32 -> je near)
                uint64_t near_scan = bmem::patternScan("0F 84 ?? ?? ?? ?? 45 84 E4 4D 0F 45 FD");
                if (near_scan) {
                    patch_addr = near_scan;
                    is_short_jump = false;
                } else {
                    scs_log(1, "[dllmain] Custom mirror patch pattern not found on this game version.");
                }
            }
        }

        if (patch_addr) {
            DWORD oldProtect;
            size_t patch_len = is_short_jump ? 1 : 2;
            if (VirtualProtect((void*)patch_addr, patch_len, PAGE_EXECUTE_READWRITE, &oldProtect)) {
                if (is_short_jump) {
                    *(reinterpret_cast<uint8_t*>(patch_addr)) = has_custom ? 0xEB : 0x74;
                } else {
                    if (has_custom) {
                        *(reinterpret_cast<uint8_t*>(patch_addr + 0)) = 0x90;
                        *(reinterpret_cast<uint8_t*>(patch_addr + 1)) = 0xE9;
                    } else {
                        *(reinterpret_cast<uint8_t*>(patch_addr + 0)) = 0x0F;
                        *(reinterpret_cast<uint8_t*>(patch_addr + 1)) = 0x84;
                    }
                }
                VirtualProtect((void*)patch_addr, patch_len, oldProtect, &oldProtect);
                custom_patched = has_custom;
                scs_log(0, "[dllmain] Custom mirror patch %s", has_custom ? "applied" : "restored");
            }
        }
    }
}

SCSAPI_VOID telemetry_tick(const scs_event_t event, const void* const event_info, scs_context_t context)
{
    update_screen_patches();
}

#pragma comment( linker, "/export:scs_telemetry_init=scs_telemetry_init" )
SCSAPI_RESULT scs_telemetry_init(const scs_u32_t version, const scs_telemetry_init_params_t* const params)
{
    scs_logging::init(params, "Carplay Screen Mirror v" + std::string(g_version));
    scs_log(0, "Starting Carplay Screen Mirror | By: erdsn");

    updater::check_for_updates_async();

    const scs_telemetry_init_params_v101_t* version_params = reinterpret_cast<const scs_telemetry_init_params_v101_t*>(params);
    version_params->register_for_event(SCS_TELEMETRY_EVENT_frame_start, telemetry_tick, nullptr);

    if (MH_Initialize() != MH_OK) {
        scs_log(2, "Failed to initialize MinHook!");
        return SCS_RESULT_generic_error;
    }

    scs_log(0, "Starting Direct Input 8 hooks...");
    dinput8::init();

    scs_log(0, "Starting DX11 hooks...");
    dx11::init();

    scs_log(0, "Starting Prism3D hooks...");
    prism::init();

    scs_log(0, "Starting Menu GUI...");
    Gui::init();

    scs_log(0, "Plugin Started");
    return SCS_RESULT_ok;
}

#pragma comment( linker, "/export:scs_telemetry_shutdown=scs_telemetry_shutdown" )
SCSAPI_VOID scs_telemetry_shutdown()
{
    {
        std::lock_guard<std::mutex> lock(g_screens_mutex);
        for (auto& screen : g_screens)
        {
            if (screen.liveTexture) { screen.liveTexture->Release(); screen.liveTexture = nullptr; }
            if (screen.immediateContext) { screen.immediateContext->Release(); screen.immediateContext = nullptr; }
        }
        g_screens.clear();
    }

    sources::WgcDispatcher::Instance().Stop();

    dx11::shutdown();
    win32::shutdown();
    dinput8::shutdown();

    MH_DisableHook(MH_ALL_HOOKS);
    MH_RemoveHook(MH_ALL_HOOKS);
    MH_Uninitialize();

    scs_log(0, "Plugin Shutdown");
}
