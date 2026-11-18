#pragma once
#include <Windows.h>
#include <wininet.h>
#include <thread>
#include <string>
#include <cstdio>
#include "scs_logging.h"
#include "version.h"

#pragma comment(lib, "wininet.lib")

namespace updater
{
    inline bool g_isOutdated = false;
    inline std::string g_latestVersion = "";
    inline bool g_isTurkish = false;

    inline bool is_newer_version(const std::string& latestStr, const std::string& currentStr)
    {
        int majLatest = 0, minLatest = 0, patchLatest = 0;
        int majCurrent = 0, minCurrent = 0, patchCurrent = 0;

        std::string sLatest = latestStr;
        if (!sLatest.empty() && (sLatest[0] == 'v' || sLatest[0] == 'V')) sLatest = sLatest.substr(1);

        std::string sCurrent = currentStr;
        if (!sCurrent.empty() && (sCurrent[0] == 'v' || sCurrent[0] == 'V')) sCurrent = sCurrent.substr(1);

        sscanf_s(sLatest.c_str(), "%d.%d.%d", &majLatest, &minLatest, &patchLatest);
        sscanf_s(sCurrent.c_str(), "%d.%d.%d", &majCurrent, &minCurrent, &patchCurrent);

        if (majLatest > majCurrent) return true;
        if (majLatest < majCurrent) return false;

        if (minLatest > minCurrent) return true;
        if (minLatest < minCurrent) return false;

        if (patchLatest > patchCurrent) return true;
        return false;
    }

    inline void check_for_updates_async()
    {
        LANGID langId = GetUserDefaultUILanguage();
        g_isTurkish = (PRIMARYLANGID(langId) == LANG_TURKISH);

        std::thread([]() {
            HINTERNET hInternet = InternetOpenA("CarplayScreenMirror-Updater", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
            if (!hInternet) return;

            DWORD timeoutMs = 4000;
            InternetSetOptionA(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
            InternetSetOptionA(hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeoutMs, sizeof(timeoutMs));

            HINTERNET hConnect = InternetOpenUrlA(
                hInternet,
                "https://api.github.com/repos/tingoja/CarplayScreenMirror/releases/latest",
                "User-Agent: CarplayScreenMirror-Mod\r\n",
                -1,
                INTERNET_FLAG_RELOAD | INTERNET_FLAG_SECURE,
                0
            );

            if (hConnect) {
                char buffer[4096] = { 0 };
                DWORD bytesRead = 0;
                if (InternetReadFile(hConnect, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
                    std::string response(buffer, bytesRead);

                    size_t tagPos = response.find("\"tag_name\":");
                    if (tagPos != std::string::npos) {
                        size_t start = response.find("\"", tagPos + 11);
                        size_t end = response.find("\"", start + 1);
                        if (start != std::string::npos && end != std::string::npos) {
                            std::string latestVer = response.substr(start + 1, end - start - 1);
                            g_latestVersion = latestVer;

                            if (is_newer_version(latestVer, g_version)) {
                                g_isOutdated = true;

                                scs_logging::scs_log(1, "==========================================================================");
                                if (g_isTurkish) {
                                    scs_logging::scs_log(1, "UYARI: DLL Surumunuz Eski! (Yuklu: %s | Guncel: %s)", g_version, latestVer.c_str());
                                    scs_logging::scs_log(1, "Lutfen GitHub uzerinden guncelleyin: https://github.com/tingoja/CarplayScreenMirror/releases/latest");
                                }
                                else {
                                    scs_logging::scs_log(1, "WARNING: Outdated DLL version! (Installed: %s | Latest: %s)", g_version, latestVer.c_str());
                                    scs_logging::scs_log(1, "Please update your DLL from GitHub: https://github.com/tingoja/CarplayScreenMirror/releases/latest");
                                }
                                scs_logging::scs_log(1, "==========================================================================");
                            }
                            else {
                                scs_logging::scs_log(0, "DLL is up to date (v%s).", g_version);
                            }
                        }
                    }
                }
                InternetCloseHandle(hConnect);
            }
            InternetCloseHandle(hInternet);
        }).detach();
    }
}
