#pragma once
#include <vector>
#include <mutex>
#include <string>
#include <set>
#include <atomic>

#include "sources/content_source.h"

struct ID3D11Texture2D;
struct ID3D11DeviceContext;

enum class screen_type_t : uint8_t {
	GPS = 1,
	DASHBOARD,
	CUSTOM
};

enum class split_alignment_t : uint8_t {
	FULL = 0,        // Stretch to fill (ignores aspect ratio)
	FIT,             // Fit inside, keep aspect ratio (black bars)
	FILL,            // Fill entire texture, keep aspect ratio (crop overflow)
	LEFT_HALF,
	RIGHT_HALF
};

struct screen_t
{
	screen_type_t type{};
	split_alignment_t splitAlignment = split_alignment_t::FULL;

	std::string original_texture; // /vehicle/truck/share/gps.tobj
	std::string override_texture; // /home/CarplayScreenMirror/gps.tobj

	uint32_t override_texture_size_w{}; // 64
	uint32_t override_texture_size_h{}; // 2048

	std::string source_application_name;
	std::string source_application_display_name;
	std::unique_ptr<IContentSource> source;
	std::vector<uint8_t> frameScratch;
	bool flipVertical = true;
	bool firstFrameLogged = false;

	// Source viewport controls (pan and zoom)
	float sourceOffsetX = 0.0f;  // -1.0 to 1.0, horizontal pan (0 = centered)
	float sourceOffsetY = 0.0f;  // -1.0 to 1.0, vertical pan (0 = centered)
	float sourceZoom = 1.0f;     // 1.0 = full source, 2.0 = 2x zoom in, etc.

	ID3D11Texture2D* liveTexture{};
	ID3D11DeviceContext* immediateContext{};

	// Target live texture size
	uint32_t targetLiveTextureWidth = 1920;
	uint32_t targetLiveTextureHeight = 1080;

	// Actual texture size of created live texture
	uint32_t liveTextureWidth{};
	uint32_t liveTextureHeight{};
};

inline std::atomic<bool> g_screen_source_creation_in_progress{};
inline std::mutex g_screens_mutex;
inline std::vector<screen_t> g_screens;

extern std::set<std::string> g_known_texture_paths;
extern std::mutex g_known_textures_mutex;

void update_screen_patches();
