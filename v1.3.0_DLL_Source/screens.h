#pragma once
#include <vector>
#include <mutex>
#include <string>
#include <set>

#include "sources/content_source.h"

class ID3D11Texture2D;
class ID3D11DeviceContext;

enum class screen_type_t : uint8_t {
	GPS = 1,
	DASHBOARD,
	CUSTOM
};

enum class split_alignment_t : uint8_t {
	FULL = 0,
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
	bool legacyCapture = false;
	bool hideBorder = true; // Added by AI for WGC border toggle
	bool flipVertical = true;

	ID3D11Texture2D* liveTexture{};
	ID3D11DeviceContext* immediateContext{};

	uint8_t framerate = 30; // Framerate of source, can actually be updated live

	// Target live texture size
	uint32_t targetLiveTextureWidth = 1920;
	uint32_t targetLiveTextureHeight = 1080;

	// Actual texture size of created live texture
	uint32_t liveTextureWidth{};
	uint32_t liveTextureHeight{};
};

inline std::atomic<bool> g_screen_source_creation_in_progress{}; // Mainly for WGC to prevent deadlock on create texture 2d
inline std::mutex g_screens_mutex;
inline std::vector<screen_t> g_screens;

extern std::set<std::string> g_known_texture_paths;
extern std::mutex g_known_textures_mutex;