#pragma once
#include "present.h"
#include "CreateTexture2D.h"

namespace dx11 {
	inline bool init() {
		if (!present::init())
			return false;

		if (!create_texture_2d::init())
			return false;

		return true;
	}

	inline void shutdown() {
		present::shutdown();
	}
}
