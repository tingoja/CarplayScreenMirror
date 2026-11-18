#include "prism.h"
#include "../bmem.h"
#include "../scs_logging.h"
using namespace scs_logging;
#include "../screens.h"
#include <intrin.h>
#include <map>

typedef char(__fastcall* memserver_texture_queue_processor_t)(uint8_t* memserver);
static memserver_texture_queue_processor_t original_memserver_texture_queue_processor{};

typedef prism::mem_tobj_t* (__fastcall* mem_tobj_ctor_t)(
    prism::mem_tobj_t* self,
    const prism::string* file_path,
    uint64_t hash,
    uint32_t flags
);
static mem_tobj_ctor_t original_mem_tobj_ctor{};

typedef uint16_t* (__fastcall* find_or_create_tobj_handle_t)(
    uint8_t* resource_manager,
    uint16_t* out_handle,
    const prism::string* path,
    uint32_t flags
);
static find_or_create_tobj_handle_t original_find_or_create_tobj_handle{};

typedef void(__fastcall* enqueue_tobj_handle_t)(uint8_t* resource_manager, uint16_t handle);
static enqueue_tobj_handle_t enqueue_tobj_handle_fn{};

static uint8_t* g_resource_manager{};
static std::map<std::string, uint16_t> g_known_tobj_handles;
static std::mutex g_handles_mutex;

static void record_known_texture_path(const std::string& path_str)
{
    if (path_str.find("mirror") != std::string::npos ||
        path_str.find("cam") != std::string::npos ||
        path_str.find("dashboard") != std::string::npos ||
        path_str.find("gps") != std::string::npos)
    {
        std::lock_guard<std::mutex> lock(g_known_textures_mutex);
        g_known_texture_paths.insert(path_str);
    }
}

static void process_tobj(prism::mem_tobj_t* tobj)
{
    if (!tobj || !tobj->m_file_path.m_string)
        return;

    std::string path_str(tobj->m_file_path.m_string);
    record_known_texture_path(path_str);

    std::lock_guard<std::mutex> lock(g_screens_mutex);
    for (auto& screen : g_screens)
    {
        if (!screen.source.get()) continue;

        if (screen.original_texture == std::string_view(tobj->m_file_path.m_string))
        {
            tobj->m_file_path.allocate(screen.override_texture.size() + 1);
            memcpy(tobj->m_file_path.m_string, screen.override_texture.data(), screen.override_texture.size());
            tobj->m_file_path.m_string[screen.override_texture.size()] = '\0';
            tobj->m_file_path.m_size = static_cast<uint32_t>(screen.override_texture.size());

            scs_log(0, "[prism::memserver_texture_queue] Replaced '%s' with '%s'", screen.original_texture.c_str(), screen.override_texture.c_str());
        }
    }
}

prism::mem_tobj_t* __fastcall hooked_mem_tobj_ctor(
    prism::mem_tobj_t* self,
    const prism::string* file_path,
    uint64_t hash,
    uint32_t flags)
{
    prism::mem_tobj_t* result = original_mem_tobj_ctor(self, file_path, hash, flags);
    process_tobj(result);
    return result;
}

static void force_reload_tobj_handle_if_cached(uint8_t* resource_manager, uint16_t handle)
{
    if (!resource_manager || !enqueue_tobj_handle_fn)
        return;

    uint16_t idx = handle & 0x3FFF;
    if (idx == 0x3FFF)
        return;

    const uint8_t* fn_bytes = reinterpret_cast<const uint8_t*>(enqueue_tobj_handle_fn);
    // Read dynamic struct offsets from enqueue_tobj_handle_fn instructions:
    // +0x30: 48 81 C1 <disp32> (add rcx, tobj_array_offset)
    // +0x60: 41 0F B7 96 <disp32> (movzx edx, word ptr [r14 + placeholder_img_offset])
    uint32_t arr_offset = 0x1a75e78;
    uint32_t placeholder_offset = 0x362394e;
    if (fn_bytes[0x30] == 0x48 && fn_bytes[0x31] == 0x81 && fn_bytes[0x32] == 0xC1) {
        arr_offset = *reinterpret_cast<const uint32_t*>(fn_bytes + 0x33);
    }
    if (fn_bytes[0x60] == 0x41 && fn_bytes[0x61] == 0x0F && fn_bytes[0x62] == 0xB7 && fn_bytes[0x63] == 0x96) {
        placeholder_offset = *reinterpret_cast<const uint32_t*>(fn_bytes + 0x64);
    }

    uint64_t array_count = *reinterpret_cast<uint64_t*>(resource_manager + arr_offset + 0x10);
    uint8_t* array_base = *reinterpret_cast<uint8_t**>(resource_manager + arr_offset + 0x08);
    if (idx < array_count && array_base)
    {
        uint8_t* tobj_entry = array_base + static_cast<size_t>(idx) * 128;
        uint16_t current_img = *reinterpret_cast<uint16_t*>(tobj_entry + 0x7C);
        uint16_t placeholder_img = *reinterpret_cast<uint16_t*>(resource_manager + placeholder_offset);
        if (current_img != placeholder_img)
        {
            enqueue_tobj_handle_fn(resource_manager, handle);
            scs_log(0, "[prism::memserver_texture_queue] Forced reload of cached tobj handle %u", idx);
        }
    }
}

uint16_t* __fastcall hooked_find_or_create_tobj_handle(
    uint8_t* resource_manager,
    uint16_t* out_handle,
    const prism::string* path,
    uint32_t flags)
{
    uint16_t* result = original_find_or_create_tobj_handle(resource_manager, out_handle, path, flags);

    if (resource_manager && path && path->m_value && out_handle && (*out_handle & 0x3FFF) != 0x3FFF)
    {
        std::string path_str(path->m_value);
        record_known_texture_path(path_str);

        {
            std::lock_guard<std::mutex> lock(g_handles_mutex);
            g_resource_manager = resource_manager;
            g_known_tobj_handles[path_str] = *out_handle;
        }

        bool should_reload = false;
        {
            std::lock_guard<std::mutex> lock(g_screens_mutex);
            for (const auto& screen : g_screens)
            {
                if (screen.source.get() && screen.original_texture == path_str)
                {
                    should_reload = true;
                    break;
                }
            }
        }

        if (should_reload)
        {
            force_reload_tobj_handle_if_cached(resource_manager, *out_handle);
        }
    }

    return result;
}

char memserver_texture_queue_processor(uint8_t* memserver)
{
    if (memserver)
    {
        volatile char* queue_spinlock = reinterpret_cast<volatile char*>(memserver + 0x190);
        while (_InterlockedExchange8(queue_spinlock, 1) != 0)
        {
            while (*queue_spinlock != 0)
            {
                YieldProcessor();
            }
        }

        prism::list_node_t<prism::mem_tobj_t>* first_node = *(prism::list_node_t<prism::mem_tobj_t>**)(memserver + 0x170);
        void* fake_node = (void*)(memserver + 0x180);

        if (first_node && first_node != fake_node)
        {
            prism::list_node_t<prism::mem_tobj_t>* target = first_node;
            int safety_counter = 0;

            while (target && target != fake_node && safety_counter < 4096)
            {
                ++safety_counter;
                process_tobj(target->m_item);
                target = target->m_next;
            }
        }

        _InterlockedExchange8(queue_spinlock, 0);
    }

    return original_memserver_texture_queue_processor(memserver);
}

namespace prism::memserver_texture_queue {
	void reload_active_screens()
	{
		std::vector<std::string> targets;
		{
			std::lock_guard<std::mutex> lock(g_screens_mutex);
			for (const auto& screen : g_screens)
			{
				if (screen.source.get() && !screen.original_texture.empty())
					targets.push_back(screen.original_texture);
			}
		}

		std::lock_guard<std::mutex> lock(g_handles_mutex);
		if (!g_resource_manager || !enqueue_tobj_handle_fn)
			return;

		for (const auto& tex_path : targets)
		{
			auto it = g_known_tobj_handles.find(tex_path);
			if (it != g_known_tobj_handles.end())
			{
				force_reload_tobj_handle_if_cached(g_resource_manager, it->second);
			}
		}
	}

	bool init() {
		uint64_t queue_scan = bmem::patternScan("48 8D 68 ?? 48 81 EC ?? ?? ?? ?? 48 8B F9 4C");
		if (!queue_scan) {
			scs_log(2, "[prism::memserver_texture_queue] Queue processor pattern scan failed!");
			return false;
		}
		uint64_t queue_processor_addr = queue_scan - 19;
		MH_CreateHook(
			(LPVOID)queue_processor_addr,
			&memserver_texture_queue_processor,
			reinterpret_cast<void**>(&original_memserver_texture_queue_processor)
		);
		MH_EnableHook((LPVOID)queue_processor_addr);

		uint64_t ctor_scan = bmem::patternScan("48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 33 F6 44 89 49 30 48 89 31");
		if (ctor_scan) {
			MH_CreateHook(
				(LPVOID)ctor_scan,
				&hooked_mem_tobj_ctor,
				reinterpret_cast<void**>(&original_mem_tobj_ctor)
			);
			MH_EnableHook((LPVOID)ctor_scan);
		} else {
			scs_log(1, "[prism::memserver_texture_queue] mem_tobj_ctor pattern not found.");
		}

		uint64_t enqueue_handle_scan = bmem::patternScan("48 89 5C 24 18 57 41 56 41 57 48 83 EC 40 41 BF FF 3F 00 00 0F B7 C2");
		if (enqueue_handle_scan) {
			enqueue_tobj_handle_fn = reinterpret_cast<enqueue_tobj_handle_t>(enqueue_handle_scan);
		}

		uint64_t find_handle_scan = bmem::patternScan("44 89 4C 24 20 55 56 41 56 41 57 48 83 EC 38 4C 8B F9 4D 8B F0 49 8B 08");
		if (find_handle_scan) {
			MH_CreateHook(
				(LPVOID)find_handle_scan,
				&hooked_find_or_create_tobj_handle,
				reinterpret_cast<void**>(&original_find_or_create_tobj_handle)
			);
			MH_EnableHook((LPVOID)find_handle_scan);
		} else {
			scs_log(1, "[prism::memserver_texture_queue] find_or_create_tobj_handle pattern not found.");
		}

		return true;
	}
}
