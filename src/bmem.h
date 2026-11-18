/*----------------------------------------*/
// Project	: CarplayScreenMirror
// File		: bmem.h
/*-----------------------------------------*/

#pragma once

#include <windows.h>
#include <cstdint>
#include <string>
#include <sstream>
#include <vector>
#include <map>

#define MINHOOK_ENABLED 1

#if MINHOOK_ENABLED
#include <MinHook/MinHook.h>
#pragma comment(lib, "minhook.x64.lib")
#endif

namespace bmem
{
	struct pattern_byte
	{
		uint8_t byte{};
		bool ignore{};
	};

	inline uintptr_t moduleBase;
	inline uint64_t moduleSize;
	inline std::string appliedModule = "";

	inline bool setModule(std::string module)
	{
		if (appliedModule == module && moduleBase != 0)
			return true;

		if (module == "current")
			moduleBase = (uintptr_t)GetModuleHandleA(nullptr);
		else
			moduleBase = (uintptr_t)GetModuleHandleA(module.c_str());

		if (!moduleBase)
			return false;

		const auto* header = (IMAGE_DOS_HEADER*)moduleBase;
		const auto* nt_header = (IMAGE_NT_HEADERS64*)((uint8_t*)header + header->e_lfanew);
		moduleSize = nt_header->OptionalHeader.SizeOfImage;

		appliedModule = module;
		return true;
	}

	inline uintptr_t patternScan(const char* patternSTR, std::string moduleToSet = "current")
	{
		if (!setModule(moduleToSet))
			return 0;

		std::vector<pattern_byte> pattern;

		std::istringstream stream(patternSTR);
		std::string token;
		while (stream >> token) {
			if (token.length() > 2 || token.length() < 1)
				return 0;

			pattern_byte pbyte;
			if (token == "??" || token == "?") {
				pbyte.ignore = true;
			}
			else
			{
				unsigned int byte;
				std::istringstream(token) >> std::hex >> byte;
				pbyte.byte = (uint8_t)byte;
			}

			pattern.push_back(pbyte);
		}

		if (pattern.empty() || pattern[0].ignore || moduleSize < pattern.size()) {
			return 0;
		}

		uintptr_t patternStart = 0;
		for (uint64_t i = 0; i <= moduleSize - pattern.size(); i++)
		{
			bool matched = true;
			for (size_t j = 0; j < pattern.size(); j++)
			{
				if (pattern[j].ignore)
					continue;

				uint8_t currentByte = *(uint8_t*)(moduleBase + i + j);
				if (currentByte != pattern[j].byte)
				{
					matched = false;
					break;
				}
			}

			if (matched)
			{
				patternStart = moduleBase + i;
				break;
			}
		}

		return patternStart;
	}

	inline bool isAddressValid(uintptr_t address)
	{
		if (address < moduleBase || address > moduleBase + moduleSize)
			return false;

		return true;
	}

	inline uintptr_t relativeToAbsolute(uintptr_t instructionAddress, int offsetToAbsolute, int instructionSize)
	{
		int32_t relativeOffset = *reinterpret_cast<int32_t*>(instructionAddress + offsetToAbsolute);
		uintptr_t absoluteAddress = instructionAddress + instructionSize + relativeOffset;
		return absoluteAddress;
	}
}
