#include "menu.h"
#include <d3d11.h>
#include <map>
#include <algorithm>
#include <chrono>

#include "../version.h"
#include "../updater.h"

#include "../scs_logging.h"
using namespace scs_logging;

#include "../prism/prism.h"
#include "../dx11/present.h"
#include "../dinput8/dinput8.h"

#include <ImGui/imgui.h>
#include "../misc/imgui_stdlib.h"
#include <ImGui/imgui_impl_dx11.h>
#include <ImGui/imgui_impl_win32.h>

#include "../screens.h"
#include "../sources/window.h"
#include "../sources/wgc_window.h"

std::set<std::string> g_known_texture_paths;
std::mutex g_known_textures_mutex;

static bool IsIgnoredProcess(const std::string& exeName)
{
    static const char* ignored[] = {
        "eurotrucks2.exe",
        "amtrucks.exe",
        "steamwebhelper.exe",
        "TextInputHost.exe",
        "ApplicationFrameHost.exe",
        "SystemSettings.exe",
        "SearchHost.exe",
        "ShellExperienceHost.exe",
        "LockApp.exe",
        "StartMenuExperienceHost.exe"
    };
    for (const char* item : ignored) {
        if (_stricmp(exeName.c_str(), item) == 0)
            return true;
    }
    return false;
}

std::string GetFriendlyMirrorName(const std::string& path) {
    std::string friendly = "";

    if (path.find("/vehicle/truck/") != std::string::npos) {
        size_t start = path.find("/vehicle/truck/") + 15;
        size_t end = path.find("/", start);
        if (end != std::string::npos) {
            std::string truckName = path.substr(start, end - start);
            for (char& c : truckName) {
                if (c == '_') c = ' ';
            }
            if (!truckName.empty()) truckName[0] = toupper(truckName[0]);
            if (truckName != "share" && truckName != "Share") {
                friendly += "[" + truckName + "] ";
            }
        }
    }

    if (path.find("cam_mirrors") != std::string::npos || path.find("cam_left") != std::string::npos || path.find("cam_right") != std::string::npos) {
        friendly += "Digital Camera Mirror";
    } else if (path.find("side_mirror") != std::string::npos) {
        friendly += "Main Side Mirror (Glass)";
    } else if (path.find("close_mirror") != std::string::npos || path.find("close_s_mirror") != std::string::npos) {
        friendly += "Close Side Mirror (Glass)";
    } else if (path.find("front_mirror") != std::string::npos) {
        friendly += "Front Mirror (Glass)";
    } else if (path.find("far_mirror") != std::string::npos || path.find("far_s_mirror") != std::string::npos) {
        friendly += "Far Side Mirror (Glass)";
    } else if (path.find("hood_mirror") != std::string::npos) {
        friendly += "Hood Mirror (Glass)";
    } else if (path.find("left_mirror") != std::string::npos) {
        friendly += "Left Mirror";
    } else if (path.find("right_mirror") != std::string::npos) {
        friendly += "Right Mirror";
    } else {
        size_t lastSlash = path.find_last_of('/');
        if (lastSlash != std::string::npos) {
            friendly += "Mirror: " + path.substr(lastSlash + 1);
        } else {
            friendly += "Unknown Mirror";
        }
    }

    return friendly;
}

static bool menu_visible{};

namespace Gui {
	bool init()
	{
		return true;
	}

	bool is_visible()
	{
		return menu_visible;
	}

	void check_toggle()
	{
		static bool wasPressed = false;

		bool ctrlDown = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
		bool f8Down = (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
		bool isPressed = ctrlDown && f8Down;

		if (isPressed && !wasPressed) {
			menu_visible = !menu_visible;
			dinput8::set_mouse(menu_visible);

			if (ImGui::GetCurrentContext()) {
				ImGuiIO* io = &ImGui::GetIO();
				if (io) io->MouseDrawCursor = menu_visible;
			}
		}
		wasPressed = isPressed;
	}

	void render()
	{
		if (!menu_visible) return;

		ImGuiStyle* style = &ImGui::GetStyle();
		style->Colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.06f, 0.94f);
		style->Colors[ImGuiCol_Header] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
		style->Colors[ImGuiCol_HeaderHovered] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
		style->Colors[ImGuiCol_HeaderActive] = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
		style->Colors[ImGuiCol_Button] = ImVec4(0.00f, 0.44f, 0.87f, 1.00f);
		style->Colors[ImGuiCol_ButtonHovered] = ImVec4(0.00f, 0.53f, 1.00f, 1.00f);
		style->Colors[ImGuiCol_ButtonActive] = ImVec4(0.00f, 0.35f, 0.68f, 1.00f);
		style->Colors[ImGuiCol_FrameBg] = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
		style->Colors[ImGuiCol_TitleBg] = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
		style->Colors[ImGuiCol_TitleBgActive] = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
		style->WindowRounding = 8.0f;
		style->FrameRounding = 4.0f;
		style->GrabRounding = 4.0f;

		ImGui::SetNextWindowSizeConstraints(ImVec2(520, 180), ImVec2(FLT_MAX, FLT_MAX));
		ImGui::Begin(("Carplay Screen Mirror v" + std::string(g_version)).c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize);

		if (updater::g_isOutdated)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.35f, 0.35f, 1.0f));
			if (updater::g_isTurkish)
			{
				ImGui::Text("UYARI: Yeni DLL Surumu Mevcut! (Mevcut: v%s | Guncel: %s)", g_version, updater::g_latestVersion.c_str());
				ImGui::Text("Lutfen GitHub uzerinden guncelleyiniz:");
			}
			else
			{
				ImGui::Text("WARNING: New DLL Version Available! (Installed: v%s | Latest: %s)", g_version, updater::g_latestVersion.c_str());
				ImGui::Text("Please update your DLL from GitHub:");
			}
			ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "https://github.com/tingoja/CarplayScreenMirror/releases/latest");
			ImGui::PopStyleColor();
			ImGui::Separator();
		}

		static bool unsavedChanges = false;
		bool hasGps = false;
		bool hasDashboard = false;
		bool hasCustom1 = false;
		bool hasCustom2 = false;
		{
			std::lock_guard<std::mutex> lock(g_screens_mutex);
			for (const auto& s : g_screens) {
				if (s.type == screen_type_t::GPS) hasGps = true;
				if (s.type == screen_type_t::DASHBOARD) hasDashboard = true;
				if (s.type == screen_type_t::CUSTOM) {
					if (s.override_texture_size_h == 1024) hasCustom1 = true;
					if (s.override_texture_size_h == 1025) hasCustom2 = true;
				}
			}
		}

		ImGui::BeginDisabled(hasGps);
		if (ImGui::Button("Add GPS Screen"))
		{
			unsavedChanges = true;

			screen_t screen;
			screen.type = screen_type_t::GPS;
			screen.original_texture = "/vehicle/truck/share/gps.tobj";
			screen.override_texture = "/home/CarplayScreenMirror/gps.tobj";
			screen.override_texture_size_h = 2048;
			screen.override_texture_size_w = 64;

			{
				std::lock_guard<std::mutex> lock(g_screens_mutex);
				g_screens.push_back(std::move(screen));
			}
			update_screen_patches();
		}
		ImGui::EndDisabled();

		ImGui::SameLine();
		ImGui::BeginDisabled(hasDashboard);
		if (ImGui::Button("Add Dashboard Screen"))
		{
			unsavedChanges = true;

			screen_t screen;
			screen.type = screen_type_t::DASHBOARD;
			screen.original_texture = "/vehicle/truck/share/dashboard.tobj";
			screen.override_texture = "/home/CarplayScreenMirror/dashboard.tobj";
			screen.override_texture_size_h = 64;
			screen.override_texture_size_w = 2048;

			{
				std::lock_guard<std::mutex> lock(g_screens_mutex);
				g_screens.push_back(std::move(screen));
			}
			update_screen_patches();
		}
		ImGui::EndDisabled();

		ImGui::SameLine();
		ImGui::BeginDisabled(hasCustom1 && hasCustom2);
		if (ImGui::Button("Add Mirror Screen"))
		{
			unsavedChanges = true;

			screen_t screen;
			screen.type = screen_type_t::CUSTOM;
			screen.original_texture = "/vehicle/truck/share/left_mirror.tobj";

			int h = !hasCustom1 ? 1024 : 1025;
			std::string id = !hasCustom1 ? "1" : "2";

			screen.override_texture = "/home/CarplayScreenMirror/custom" + id + ".tobj";
			screen.override_texture_size_h = h;
			screen.override_texture_size_w = 128;

			{
				std::lock_guard<std::mutex> lock(g_screens_mutex);
				g_screens.push_back(std::move(screen));
			}
			update_screen_patches();
		}
		ImGui::EndDisabled();

		ImGui::SameLine();
		if (unsavedChanges) {
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.25f, 0.10f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.95f, 0.35f, 0.15f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.70f, 0.18f, 0.08f, 1.0f));
		}

		bool canApply = unsavedChanges;
		if (canApply) {
			std::lock_guard<std::mutex> lock(g_screens_mutex);
			for (const auto& s : g_screens) {
				if (!s.source.get()) {
					canApply = false;
					break;
				}
			}
		}
		ImGui::BeginDisabled(!canApply);
		bool justSaved = false;
		if (ImGui::Button("Apply Changes"))
		{
			update_screen_patches();
			prism::memserver_texture_queue::reload_active_screens();

			prism::string cmd("game");
			prism::execute_command::call(&cmd, -1);

			justSaved = true;
			unsavedChanges = false;
		}
		ImGui::EndDisabled();

		if (unsavedChanges || justSaved) ImGui::PopStyleColor(3);

		// Cache EnumWindows results - refresh every 2 seconds
		static std::map<std::string, std::string> cached_applications;
		static auto lastEnumTime = std::chrono::steady_clock::time_point{};
		auto now = std::chrono::steady_clock::now();
		if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastEnumTime).count() > 2000) {
			cached_applications.clear();
			EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
				auto* applications = reinterpret_cast<std::map<std::string, std::string>*>(lParam);

				if (!IsWindowVisible(hwnd))
					return TRUE;

				LONG exStyle = GetWindowLongA(hwnd, GWL_EXSTYLE);
				if (exStyle & WS_EX_TOOLWINDOW)
					return TRUE;

				int titleLen = GetWindowTextLengthA(hwnd);
				if (titleLen == 0)
					return TRUE;

				std::string windowTitle(titleLen + 1, '\0');
				GetWindowTextA(hwnd, windowTitle.data(), titleLen + 1);
				windowTitle.resize(titleLen);

				DWORD pid = 0;
				GetWindowThreadProcessId(hwnd, &pid);

				HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
				if (!hProc) return TRUE;

				char path[MAX_PATH]{};
				DWORD size = MAX_PATH;
				if (!QueryFullProcessImageNameA(hProc, 0, path, &size)) {
					CloseHandle(hProc);
					return TRUE;
				}
				CloseHandle(hProc);

				std::string applicationName(path);
				auto pos = applicationName.rfind('\\');
				applicationName = pos != std::string::npos ? applicationName.substr(pos + 1) : applicationName;

				if (IsIgnoredProcess(applicationName))
					return TRUE;

				(*applications)[windowTitle] = applicationName;
				return TRUE;
			}, reinterpret_cast<LPARAM>(&cached_applications));
			lastEnumTime = now;
		}
		auto& applications = cached_applications;

		{
			std::lock_guard<std::mutex> lock(g_screens_mutex);
			std::vector<int> to_remove{};

			int i = 0;
			for (screen_t& screen : g_screens) {
				std::string name = "Carplay Screen";
				if (screen.type == screen_type_t::GPS) name = "GPS Screen";
				else if (screen.type == screen_type_t::DASHBOARD) name = "Dashboard Screen";
				else if (screen.type == screen_type_t::CUSTOM) name = "Mirror Screen";
				name += "##";

				if (ImGui::CollapsingHeader((name + std::to_string(i)).c_str(), ImGuiTreeNodeFlags_DefaultOpen))
				{
					ImGui::PushID(i);

					if (screen.type == screen_type_t::CUSTOM) {
						ImGui::Text("Mirror Location");
						ImGui::SameLine(130.0f);
						ImGui::SetNextItemWidth(360.0f);

						std::vector<std::string> options;
						{
							std::lock_guard<std::mutex> texLock(g_known_textures_mutex);
							for (const auto& path : g_known_texture_paths) {
								if (path.find("gps") == std::string::npos && path.find("dashboard") == std::string::npos) {
									options.push_back(path);
								}
							}
						}

						std::sort(options.begin(), options.end(), [](const std::string& a, const std::string& b) {
							bool a_is_digital = (a.find("cam_mirrors") != std::string::npos || a.find("cam_left") != std::string::npos || a.find("cam_right") != std::string::npos);
							bool b_is_digital = (b.find("cam_mirrors") != std::string::npos || b.find("cam_left") != std::string::npos || b.find("cam_right") != std::string::npos);
							if (a_is_digital && !b_is_digital) return true;
							if (!a_is_digital && b_is_digital) return false;
							return a < b;
						});

						std::string current_preview = "Select Mirror...";
						if (!screen.original_texture.empty()) {
							current_preview = GetFriendlyMirrorName(screen.original_texture);
						}

						if (ImGui::BeginCombo("##preset_combo", current_preview.c_str())) {
							for (size_t n = 0; n < options.size(); n++) {
								bool is_selected = (screen.original_texture == options[n]);
								std::string friendly_name = GetFriendlyMirrorName(options[n]) + "##" + options[n];
								if (ImGui::Selectable(friendly_name.c_str(), is_selected)) {
									screen.original_texture = options[n];
									unsavedChanges = true;
								}
								if (is_selected) ImGui::SetItemDefaultFocus();
							}
							ImGui::EndCombo();
						}
					}

					if (!screen.source.get()) {
						screen.source_application_display_name.clear();
						screen.source_application_name.clear();
					}

					std::string sourcePreview = "Select Window / Application...";
					if (!screen.source_application_name.empty()) {
						sourcePreview = "[" + screen.source_application_name + "] " + screen.source_application_display_name;
					}

					ImGui::Text("Window Source");
					ImGui::SameLine(130.0f);
					ImGui::SetNextItemWidth(360.0f);

					bool changed = false;
					if (ImGui::BeginCombo("##appcombo", sourcePreview.c_str()))
					{
						int idx = 0;
						for (const auto& [title, application] : applications) {
							bool selected = (title == screen.source_application_display_name) && (application == screen.source_application_name);

							if (ImGui::Selectable(("[" + application + "] " + title + "##" + std::to_string(idx)).c_str(), selected)) {
								screen.source_application_name = application;
								screen.source_application_display_name = title;
								changed = true;
							}

							if (selected)
								ImGui::SetItemDefaultFocus();

							idx++;
						}
						ImGui::EndCombo();
					}

					if (changed) {
						if (!screen.liveTexture) {
							unsavedChanges = true;
						}
						g_screen_source_creation_in_progress = true;
						screen.source.reset();
						screen.firstFrameLogged = false;

						screen.source = sources::CreateWgcWindowSource(
							screen.source_application_name.c_str(),
							screen.source_application_display_name.c_str(),
							true
						);

						if (!screen.source.get()) {
							screen.source_application_display_name.clear();
							screen.source_application_name.clear();
							ImGui::OpenPopup("Source Error");
						}

						g_screen_source_creation_in_progress = false;
					}

					if (ImGui::BeginPopupModal("Source Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
						ImGui::TextWrapped("Could not capture the selected window. Make sure the window is open.");
						if (ImGui::Button("OK", ImVec2(120, 0))) {
							ImGui::CloseCurrentPopup();
						}
						ImGui::EndPopup();
					}

					ImGui::Text("Screen Fit");
					ImGui::SameLine(130.0f);
					ImGui::SetNextItemWidth(220.0f);
					const char* fit_options[] = { "Stretch (Full)", "Fit (Keep Aspect)", "Fill (Crop)", "Left Half", "Right Half" };
					int current_fit = static_cast<int>(screen.splitAlignment);
					if (ImGui::Combo("##split_alignment", &current_fit, fit_options, IM_ARRAYSIZE(fit_options))) {
						screen.splitAlignment = static_cast<split_alignment_t>(current_fit);
					}

					ImGui::Text("Zoom");
					ImGui::SameLine(130.0f);
					ImGui::SetNextItemWidth(220.0f);
					ImGui::SliderFloat("##source_zoom", &screen.sourceZoom, 1.0f, 5.0f, "%.1fx");

					if (screen.sourceZoom > 1.01f) {
						ImGui::Text("Position X / Y");
						ImGui::SameLine(130.0f);
						ImGui::SetNextItemWidth(106.0f);
						ImGui::SliderFloat("##source_offset_x", &screen.sourceOffsetX, -1.0f, 1.0f, "X: %.2f");
						ImGui::SameLine();
						ImGui::SetNextItemWidth(106.0f);
						ImGui::SliderFloat("##source_offset_y", &screen.sourceOffsetY, -1.0f, 1.0f, "Y: %.2f");
						ImGui::SameLine();
						if (ImGui::Button("Reset##viewport")) {
							screen.sourceZoom = 1.0f;
							screen.sourceOffsetX = 0.0f;
							screen.sourceOffsetY = 0.0f;
						}
					}

					ImGui::Spacing();
					if (ImGui::Button("Flip Screen")) {
						screen.flipVertical = !screen.flipVertical;
					}
					ImGui::SameLine();
					if (ImGui::Button("Remove Screen")) {
						if (screen.liveTexture) { screen.liveTexture->Release(); screen.liveTexture = nullptr; }
						if (screen.immediateContext) { screen.immediateContext->Release(); screen.immediateContext = nullptr; }

						to_remove.push_back(i);
					}

					ImGui::PopID();
				}

				i++;
			}

			if (!to_remove.empty()) {
				for (auto it = to_remove.rbegin(); it != to_remove.rend(); ++it) {
					g_screens.erase(g_screens.begin() + *it);
				}
			}
		}

		ImGui::End();
	}
}
