#include "dx11.h"
#include "../win32/win32.h"
#include "../menu/menu.h"
#include <MinHook/MinHook.h>
#include "../scs_logging.h"
using namespace scs_logging;

#include <ImGui/imgui.h>
#include <ImGui/imgui_impl_dx11.h>
#include <ImGui/imgui_impl_win32.h>

#include <vector>
#include <functional>

static std::vector<std::function<void()>> frame_callbacks{};
static std::vector<std::function<void()>> pending_callbacks{};

static ID3D11Device* device{};
static ID3D11DeviceContext* context{};
static ID3D11RenderTargetView* mainRenderTargetView{};

static void* present_function_address{};
static void* resize_buffers_function_address{};

typedef HRESULT(__stdcall* present_t)(IDXGISwapChain*, UINT, UINT);
static present_t original_present{};

HRESULT hooked_present(IDXGISwapChain* SwapChain, UINT SyncInterval, UINT Flags)
{
    static bool init{};
    if (!init) {
        DXGI_SWAP_CHAIN_DESC desc;
        HRESULT result = SwapChain->GetDesc(&desc);
        if (!SUCCEEDED(result)) {
            scs_log(2, "Failed to get swapchain description.");
            return original_present(SwapChain, SyncInterval, Flags);
        }

        result = SwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&device);
        if (!SUCCEEDED(result)) {
            scs_log(2, "Failed to get D3D11 device.");
            return original_present(SwapChain, SyncInterval, Flags);
        }

        device->GetImmediateContext(&context);

        win32::init(desc.OutputWindow);

        ImGuiContext* ctx = ImGui::CreateContext();
        ImGui::SetCurrentContext(ctx);

        ImGui_ImplWin32_Init(desc.OutputWindow);
        ImGui_ImplDX11_Init(device, context);

        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr; // Avoid creating imgui.ini in game directory
        if (GetFileAttributesA("C:\\Windows\\Fonts\\Arial.ttf") != INVALID_FILE_ATTRIBUTES) {
            io.FontDefault = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\Arial.ttf", 15.0f);
        }
        io.Fonts->Build();

        init = true;
    }

    if (!pending_callbacks.empty()) {
        for (auto& cb : pending_callbacks)
            frame_callbacks.push_back(std::move(cb));
        pending_callbacks.clear();
    }

    // 1. Always run non-UI frame callbacks (e.g. live texture updates)
    for (auto& cb : frame_callbacks)
        cb();

    // 2. Check CTRL + F8 menu toggle
    Gui::check_toggle();

    // 3. Only run ImGui and touch RenderTargetViews when the menu is actually visible!
    // This eliminates stutter, HDR/sRGB gamma issues, and overlay conflicts on high-end systems.
    if (Gui::is_visible())
    {
        if (!mainRenderTargetView) {
            ID3D11Texture2D* backBuffer = nullptr;
            if (SUCCEEDED(SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&backBuffer)) && backBuffer) {
                D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
                rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
                if (FAILED(device->CreateRenderTargetView(backBuffer, &rtvDesc, &mainRenderTargetView))) {
                    device->CreateRenderTargetView(backBuffer, NULL, &mainRenderTargetView);
                }
                backBuffer->Release();
            }
        }

        if (mainRenderTargetView) {
            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            Gui::render();

            ImGui::EndFrame();
            ImGui::Render();

            ID3D11RenderTargetView* prevRTV = nullptr;
            ID3D11DepthStencilView* prevDSV = nullptr;
            context->OMGetRenderTargets(1, &prevRTV, &prevDSV);

            context->OMSetRenderTargets(1, &mainRenderTargetView, NULL);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

            context->OMSetRenderTargets(1, &prevRTV, prevDSV);
            if (prevRTV) prevRTV->Release();
            if (prevDSV) prevDSV->Release();
        }
    }

    return original_present(SwapChain, SyncInterval, Flags);
}

typedef HRESULT(__stdcall* resize_buffers_t)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
static resize_buffers_t original_resize_buffers{};

HRESULT hooked_resize_buffers(IDXGISwapChain* SwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
{
    if (mainRenderTargetView) {
        if (context) context->OMSetRenderTargets(0, nullptr, nullptr);
        mainRenderTargetView->Release();
        mainRenderTargetView = nullptr;
    }

    return original_resize_buffers(SwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
}

namespace dx11::present {
	bool init()
	{
        // Create our own hidden dummy window instead of using GetForegroundWindow()
        // to prevent crashes in d3d11.dll / ReShade / Steam Overlay during game startup.
        HWND dummyHwnd = CreateWindowExA(
            0, "STATIC", "CarplayScreenMirrorDummy", WS_POPUP,
            0, 0, 8, 8, nullptr, nullptr, GetModuleHandleA(nullptr), nullptr
        );

        IDXGISwapChain* dummySwapChain{};
        ID3D11Device* dummyDevice{};
        ID3D11DeviceContext* dummyContext{};

        DXGI_SWAP_CHAIN_DESC desc = {};
        desc.BufferCount = 1;
        desc.BufferDesc.Width = 8;
        desc.BufferDesc.Height = 8;
        desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.OutputWindow = dummyHwnd ? dummyHwnd : GetDesktopWindow();
        desc.SampleDesc.Count = 1;
        desc.Windowed = TRUE;

        D3D_FEATURE_LEVEL level;
        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
            nullptr, 0, D3D11_SDK_VERSION,
            &desc, &dummySwapChain, &dummyDevice, &level, &dummyContext
        );

        if (FAILED(hr)) {
            hr = D3D11CreateDeviceAndSwapChain(
                nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
                nullptr, 0, D3D11_SDK_VERSION,
                &desc, &dummySwapChain, &dummyDevice, &level, &dummyContext
            );
        }

        if (FAILED(hr) || !dummySwapChain) {
            if (dummyHwnd) DestroyWindow(dummyHwnd);
            scs_log(2, "[dx11::present] D3D11CreateDeviceAndSwapChain failed (hr=0x%08X).", hr);
            return false;
        }

        void** vtable = *reinterpret_cast<void***>(dummySwapChain);
        present_function_address = vtable[8];
        MH_CreateHook(
            present_function_address,
            &hooked_present,
            reinterpret_cast<void**>(&original_present)
        );
        MH_EnableHook(present_function_address);

        resize_buffers_function_address = vtable[13];
        MH_CreateHook(
            resize_buffers_function_address,
            &hooked_resize_buffers,
            reinterpret_cast<void**>(&original_resize_buffers)
        );
        MH_EnableHook(resize_buffers_function_address);

        dummyContext->Release();
        dummyDevice->Release();
        dummySwapChain->Release();
        if (dummyHwnd) DestroyWindow(dummyHwnd);

        return true;
	}

    void shutdown()
    {
        if (present_function_address) {
            MH_DisableHook(present_function_address);
            MH_RemoveHook(present_function_address);
        }

        if (resize_buffers_function_address) {
            MH_DisableHook(resize_buffers_function_address);
            MH_RemoveHook(resize_buffers_function_address);
        }

        if (mainRenderTargetView) { mainRenderTargetView->Release(); mainRenderTargetView = nullptr; }
        if (context) { context->Release(); context = nullptr; }
        if (device) { device->Release(); device = nullptr; }
    }

    void on_frame(std::function<void()> callback)
    {
        pending_callbacks.push_back(callback);
    }
}
