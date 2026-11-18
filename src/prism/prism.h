#pragma once
#include <new>
#include <cstdint>
#include <cstring>

#include "execute_command.h"
#include "memserver_texture_queue.h"

#pragma pack(push, 1)
namespace prism {
    inline bool init() {
        if (!execute_command::get())
            return false;

        if (!memserver_texture_queue::init())
            return false;

        return true;
    }

	class string_dyn_t // Size: 0x0020
	{
	public:
		char*	 m_string;	  //0x0008 (0x08)
		uint32_t m_size;	  //0x0010 (0x04)
		uint32_t m_capacity;  //0x0014 (0x04)
		void*	 m_allocator; //0x0018 (0x08)

		virtual string_dyn_t* destructor(bool free);
		virtual void Function1();
		virtual const uint64_t allocate(uint64_t target);
		virtual void clear();
		virtual void Function4();
		virtual void Function5();
		virtual void Function6();
		virtual void Function7();
		virtual void Function8();
		virtual void Function9();
		virtual void Function10();
		virtual void Function11();
		virtual void Function12();
		virtual void Function13();
		virtual void Function14();
		virtual void Function15();
		virtual void Function16();
		virtual void Function17();
		virtual void Function18();
		virtual void Function19();
	};
	static_assert(sizeof(string_dyn_t) == 0x20);

    class string
    {
    public:
        char* m_value{};

        ~string() {
            delete[] m_value;
        }

        string(const char* str)
        {
            size_t len = strlen(str) + 1;
            m_value = new char[len];
            strcpy_s(m_value, len, str);
        }

        string(const string& other) : string(other.m_value) {}

        string(string&& other) noexcept
        {
            m_value = other.m_value;
            other.m_value = nullptr;
        }

        string& operator=(const string& other) {
            if (this == &other)
                return *this;

            this->~string();
            new (this) string(other.m_value);
            return *this;
        }

        string& operator=(string&& other) noexcept
        {
            if (this == &other)
                return *this;

            this->~string();

            m_value = other.m_value;
            other.m_value = nullptr;

            return *this;
        }
    };

    class mem_tobj_t // Size: 0x00A8
    {
    public:
        char pad_0000[56];        //0x0000 (0x38)
        string_dyn_t m_file_path; //0x0038 (0x20)
        char pad_0058[80];        //0x0058 (0x50)
    };
    static_assert(sizeof(mem_tobj_t) == 0xA8);

    template <typename T = void>
    class list_node_t // Size: 0x0018
    {
    public:
        list_node_t* m_next;     //0x0000 (0x08)
        list_node_t* m_previous; //0x0008 (0x08)
        T* m_item;               //0x0010 (0x08)
    };
    static_assert(sizeof(list_node_t<>) == 0x18);
}
#pragma pack(pop)
