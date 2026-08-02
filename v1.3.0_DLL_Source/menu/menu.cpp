#include "menu.h"
#include <d3d11.h>
#include <map>
#include <algorithm>

#include "../version.h"

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
            friendly += "Custom: " + path.substr(lastSlash + 1);
        } else {
            friendly += "Unknown Mirror";
        }
    }

    return friendly;
}

static bool menu_visible{};
void on_frame()
{
	static bool wasPressed = false;

	bool ctrlDown = GetAsyncKeyState(VK_CONTROL) & 0x8000;
	bool f8Down = GetAsyncKeyState(VK_F8) & 0x8000;
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


	if (menu_visible) {
		// Carplay Custom Apple-like Dark Mode with Green/Blue Accent
		ImGuiStyle* style = &ImGui::GetStyle();
		style->Colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.06f, 0.94f);
		style->Colors[ImGuiCol_Header] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
		style->Colors[ImGuiCol_HeaderHovered] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
		style->Colors[ImGuiCol_HeaderActive] = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
		style->Colors[ImGuiCol_Button] = ImVec4(0.00f, 0.44f, 0.87f, 1.00f); // Apple Blue
		style->Colors[ImGuiCol_ButtonHovered] = ImVec4(0.00f, 0.53f, 1.00f, 1.00f);
		style->Colors[ImGuiCol_ButtonActive] = ImVec4(0.00f, 0.35f, 0.68f, 1.00f);
		style->Colors[ImGuiCol_FrameBg] = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
		style->Colors[ImGuiCol_TitleBg] = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
		style->Colors[ImGuiCol_TitleBgActive] = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
		style->WindowRounding = 8.0f;
		style->FrameRounding = 4.0f;
		style->GrabRounding = 4.0f;

		ImGui::SetNextWindowSizeConstraints(ImVec2(584, 208), ImVec2(FLT_MAX, FLT_MAX));
		ImGui::Begin(("Carplay Screen Mirror v" + std::string(g_version)).c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize);

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
		}
		ImGui::EndDisabled();

		ImGui::SameLine();
		ImGui::BeginDisabled(hasCustom1 && hasCustom2);
		if (ImGui::Button("Add Custom Screen"))
		{
			unsavedChanges = true;

			screen_t screen;
			screen.type = screen_type_t::CUSTOM;
			screen.original_texture = "/vehicle/truck/share/left_mirror.tobj"; // Set a safe default
			
			int h = !hasCustom1 ? 1024 : 1025;
			std::string id = !hasCustom1 ? "1" : "2";
			
			screen.override_texture = "/home/CarplayScreenMirror/custom" + id + ".tobj";
			screen.override_texture_size_h = h;
			screen.override_texture_size_w = 128;

			{
				std::lock_guard<std::mutex> lock(g_screens_mutex);
				g_screens.push_back(std::move(screen));
			}
		}
		ImGui::EndDisabled();

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
		if (ImGui::Button("Apply Unsaved Changes"))
		{
			prism::string cmd("game");
			prism::execute_command::call(&cmd, -1);

			justSaved = true;
			unsavedChanges = false;
		}
		ImGui::EndDisabled();

		if (unsavedChanges || justSaved) ImGui::PopStyleColor(3);




		std::map<std::string, std::string> applications; // window title, application name
		EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
			auto* applications = reinterpret_cast<std::map<std::string, std::string>*>(lParam);

			if (!IsWindowVisible(hwnd))
				return TRUE;

			LONG exStyle = GetWindowLongA(hwnd, GWL_EXSTYLE);
			if (exStyle & WS_EX_TOOLWINDOW)
				return TRUE;

			std::string windowTitle;
			bool hasTitle{};

			int titleLen = GetWindowTextLengthA(hwnd);
			if (titleLen != 0)
			{
				hasTitle = true;

				windowTitle = std::string(titleLen + 1, '\0');
				GetWindowTextA(hwnd, windowTitle.data(), titleLen + 1);
				windowTitle.resize(titleLen);
			}



			std::string applicationName;

			DWORD pid = 0;
			GetWindowThreadProcessId(hwnd, &pid);

			HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
			if (!hProc) return TRUE; // Continue, dont add if no process name

			char path[MAX_PATH]{};
			DWORD size = MAX_PATH;
			QueryFullProcessImageNameA(hProc, 0, path, &size);
			CloseHandle(hProc);

			applicationName = std::string(path);

			auto pos = applicationName.rfind('\\');
			applicationName = pos != std::string::npos ? applicationName.substr(pos + 1) : applicationName;

			if (!hasTitle || windowTitle.empty() || windowTitle == "")
				windowTitle = applicationName;

			(*applications)[windowTitle] = applicationName;

			return TRUE;
		}, reinterpret_cast<LPARAM>(&applications));


		{
			std::lock_guard<std::mutex> lock(g_screens_mutex);
			std::vector<int> to_remove{}; // indexes to remove

			int i = 0;
			hasGps = false;
			for (screen_t& screen : g_screens) {
				bool isGps = screen.type == screen_type_t::GPS;

				if (isGps)
					hasGps = true;

				std::string name = "Carplay Screen";
				if (isGps) name = "Carplay GPS Screen";
				else if (screen.type == screen_type_t::DASHBOARD) name = "Carplay Dashboard Screen";
				else if (screen.type == screen_type_t::CUSTOM) name = "Carplay Custom Screen";
				name += "##";
				if (ImGui::CollapsingHeader((name + std::to_string(i)).c_str(), ImGuiTreeNodeFlags_DefaultOpen))
				{
					ImGui::PushID(i);

					if (screen.type == screen_type_t::CUSTOM) {
						if (ImGui::BeginTable("screen_table", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit))
						{
							ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 170.0f);
							ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

							ImGui::TableNextRow();
							ImGui::TableSetColumnIndex(0);
							ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.55f, 1.0f), "Game Texture Path");
							if (ImGui::IsItemHovered())
							{
								ImGui::BeginTooltip();
								ImGui::Text("Enter the game's texture path to override (e.g. /vehicle/truck/share/mirror.tobj)");
								ImGui::EndTooltip();
							}
							ImGui::TableSetColumnIndex(1);
							ImGui::SetNextItemWidth(-FLT_MIN);
							
							std::vector<std::string> options;
							{
								std::lock_guard<std::mutex> lock(g_known_textures_mutex);
								for (const auto& path : g_known_texture_paths) {
									options.push_back(path);
								}
							}
							
							std::sort(options.begin(), options.end(), [](const std::string& a, const std::string& b) {
								bool a_is_digital = (a.find("cam_mirrors") != std::string::npos || a.find("cam_left") != std::string::npos || a.find("cam_right") != std::string::npos);
								bool b_is_digital = (b.find("cam_mirrors") != std::string::npos || b.find("cam_left") != std::string::npos || b.find("cam_right") != std::string::npos);
								if (a_is_digital && !b_is_digital) return true;
								if (!a_is_digital && b_is_digital) return false;
								return a < b;
							});

							std::string current_preview = "Select Auto-Detected Mirror...";
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
							
							ImGui::SetNextItemWidth(-FLT_MIN);
							if (ImGui::InputText("##original_texture", &screen.original_texture)) unsavedChanges = true;

							ImGui::EndTable();
						}
					}

					ImGui::Text("Resolution");
					ImGui::SameLine();
					ImGui::SetNextItemWidth(80.0f);
					if (ImGui::InputScalar("##res_w", ImGuiDataType_U32, &screen.targetLiveTextureWidth)) unsavedChanges = true;
					ImGui::SameLine();
					ImGui::Text("x");
					ImGui::SameLine();
					ImGui::SetNextItemWidth(80.0f);
					if (ImGui::InputScalar("##res_h", ImGuiDataType_U32, &screen.targetLiveTextureHeight)) unsavedChanges = true;

					if (screen.type == screen_type_t::CUSTOM) {
						ImGui::Text("Mirror Fit");
						ImGui::SameLine();
						ImGui::SetNextItemWidth(160.0f);
						const char* fit_options[] = { "Full (Stretched)", "Fit to Left Mirror", "Fit to Right Mirror" };
						int current_fit = static_cast<int>(screen.splitAlignment);
						if (ImGui::Combo("##split_alignment", &current_fit, fit_options, IM_ARRAYSIZE(fit_options))) {
							screen.splitAlignment = static_cast<split_alignment_t>(current_fit);
							unsavedChanges = true;
						}
					}

					ImGui::BeginDisabled(!screen.legacyCapture);
					ImGui::Text("Framerate");
					ImGui::SameLine();
					ImGui::SetNextItemWidth(160.0f);
					uint8_t fps_min = 1, fps_max = 255;
					if (ImGui::SliderScalar("##target_fps", ImGuiDataType_U8, &screen.framerate, &fps_min, &fps_max) && screen.source.get()) {
						screen.source->SetFramerate(screen.framerate);
					}
					ImGui::EndDisabled();

					ImGui::Checkbox("Legacy Capture", &screen.legacyCapture);
					if (ImGui::IsItemHovered())
					{
						ImGui::BeginTooltip();
						ImGui::Text("Not Recommended | Only use if capture fails");
						ImGui::EndTooltip();
					}

					ImGui::Checkbox("Hide Capture Border (Yellow Line)", &screen.hideBorder);
					if (ImGui::IsItemHovered())
					{
						ImGui::BeginTooltip();
						ImGui::Text("Hides the yellow Windows capture border around the source window");
						ImGui::EndTooltip();
					}

					// Nothing can be selected without a source (handles for application closing)
					if (!screen.source.get()) {
						screen.source_application_display_name.clear();
						screen.source_application_name.clear();
					}

					const char* preview = screen.source_application_name.empty() ? "Select Source..." : screen.source_application_name.c_str();

					bool changed = false;
					if (ImGui::BeginCombo("##appcombo", preview))
					{
						int i = 0;
						for (const auto& [title, application] : applications) {
							bool selected = (title == screen.source_application_display_name) ? true : ((application == screen.source_application_name) ? true : false);

							if (ImGui::Selectable(("[" + application + "] " + title + "##" + std::to_string(i)).c_str(), selected)) {
								screen.source_application_name = application;
								screen.source_application_display_name = title;
								changed = true;
							}

							if (selected)
								ImGui::SetItemDefaultFocus();

							i++;
						}
						ImGui::EndCombo();
					}

					if (changed) {
						unsavedChanges = true;
						g_screen_source_creation_in_progress = true;
						screen.source.reset(); // Destroy before construct

						if (screen.legacyCapture) {
							screen.source = sources::CreateWindowSource(screen.source_application_name.c_str(), screen.source_application_display_name.c_str());
						}
						else {
							screen.source = sources::CreateWgcWindowSource(screen.source_application_name.c_str(), screen.source_application_display_name.c_str(), screen.hideBorder);
						}

						if (!screen.source.get()) {
							screen.source_application_display_name.clear();
							screen.source_application_name.clear();
							ImGui::OpenPopup("Source Error");
						}

						g_screen_source_creation_in_progress = false;
					}

					if (ImGui::BeginPopupModal("Source Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
						ImGui::TextWrapped("Failed to use source, check the console for more details");
						if (ImGui::Button("OK", ImVec2(120, 0))) {
							ImGui::CloseCurrentPopup();
						}
						ImGui::EndPopup();
					}


					if (ImGui::Button("Remove")) {

						if (screen.liveTexture) screen.liveTexture->Release();
						if (screen.immediateContext) screen.immediateContext->Release();

						to_remove.push_back(i);
					}
					ImGui::SameLine();
					if (ImGui::Button("Flip Screen")) {
						screen.flipVertical = !screen.flipVertical;
					}

					ImGui::PopID();
				}

				i++;
			}

			for (auto it = to_remove.rbegin(); it != to_remove.rend(); ++it) {
				g_screens.erase(g_screens.begin() + *it);
			}
		}




		ImGui::End();
	}
}

namespace Gui {
	bool init()
	{
		dx11::present::on_frame(on_frame);
		return true;
	}
}
