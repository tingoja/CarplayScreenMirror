#include "win32.h"
#include "../menu/menu.h"

#include <Windows.h>
#include <ImGui/imgui_impl_win32.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static HWND newHwnd{};
static WNDPROC originalWindowProc{};

LRESULT CALLBACK hookedWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (Gui::is_visible()) {
        if (ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam))
            return TRUE;
    }

    return CallWindowProc(originalWindowProc, hwnd, uMsg, wParam, lParam);
}

namespace win32 {
	void init(void* hwnd)
	{
		newHwnd = (HWND)hwnd;
		originalWindowProc = (WNDPROC)SetWindowLongPtr(newHwnd, GWLP_WNDPROC, (LONG_PTR)hookedWindowProc);
	}

	void shutdown()
	{
		if (originalWindowProc && newHwnd)
			SetWindowLongPtr(newHwnd, GWLP_WNDPROC, (LONG_PTR)originalWindowProc);
	}
}
