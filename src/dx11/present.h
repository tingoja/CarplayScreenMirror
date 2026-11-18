#pragma once
#include <d3d11.h>
#include <functional>

#pragma comment(lib, "d3d11.lib")

namespace dx11::present {
	bool init();
	void shutdown();
	void on_frame(std::function<void()> callback);
}
