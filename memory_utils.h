#ifndef __MEMORY_UTILS_H
#define __MEMORY_UTILS_H

#include <windows.h>
#include <memoryapi.h>
#include <processthreadsapi.h>

#include <string.h>

#include <vector>

#include "socket_utils.h"
#include "cmd_utils.h"
#include "process_utils.h"
#include "file_utils.h"

struct memory_info{
	MEMORY_BASIC_INFORMATION native;
	bool has_module_info;
	module_info module;
};

static std::vector<memory_info> enumerate_memory_regions(){
	void *offset = 0;
	MEMORY_BASIC_INFORMATION info;
	std::vector<memory_info> regions;
	while(VirtualQueryEx(GetCurrentProcess(), offset, &info, sizeof(info)) != 0){
		struct memory_info item = {0};
		item.native = info;
		regions.push_back(item);
		offset = (void *)((uint64_t)info.BaseAddress + (uint64_t)info.RegionSize);
	}

	auto modules = enumerate_modules();
	auto itr = modules.begin();
	while(itr != modules.end()){
		auto ritr = regions.begin();
		while(ritr != regions.end()){
			if(itr->native.lpBaseOfDll >= ritr->native.BaseAddress && itr->native.lpBaseOfDll < (void *)((size_t)ritr->native.BaseAddress + ritr->native.RegionSize)){
				ritr->module = *itr;
				ritr->has_module_info = true;
				break;
			}
			ritr++;
		}
		itr++;
	}

	return regions;
}

static uint32_t send_memory_regions(int fd, std::vector<memory_info> &regions){
	auto itr = regions.begin();
	int total_sent = 0;

	static const char title_modules[] = "Modules:\n";
	int ret = send_till_done(fd, title_modules, sizeof(title_modules), 0);
	if(ret <= 0){
		return ret;
	}

	total_sent = total_sent + ret;

	while(itr != regions.end()){
		if(!itr->has_module_info){
			itr++;
			continue;
		}

		char send_buf[512];
		int len = sprintf(send_buf, "%s: 0x%p - 0x%p %zu (0x%p - 0x%p %zu)\n", itr->module.name, itr->native.BaseAddress, (size_t)itr->native.BaseAddress + itr->native.RegionSize - 1, itr->native.RegionSize, itr->module.native.lpBaseOfDll, (size_t)itr->module.native.lpBaseOfDll + itr->module.native.SizeOfImage - 1, itr->module.native.SizeOfImage);
		ret = send_till_done(fd, send_buf, len, 0);
		if(ret <= 0){
			return ret;
		}
		total_sent = total_sent + ret;
		itr++;
	}

	static const char title_others[] = "Others:\n";
	ret = send_till_done(fd, title_others, sizeof(title_others), 0);
	if(ret <= 0){
		return ret;
	}

	total_sent = total_sent + ret;

	itr = regions.begin();
	while(itr != regions.end()){
		if(itr->has_module_info){
			itr++;
			continue;
		}

		char send_buf[512];

		const char *type = "unknown";
		switch(itr->native.Type){
			case MEM_MAPPED:
				type = "mapped";
				break;
			case MEM_IMAGE:
				type = "image";
				break;
			case MEM_PRIVATE:
				type = "private";
				break;
		itr++;
		}
		const char *state = "unknown";
		switch(itr->native.State){
			case MEM_COMMIT:
				state = "commit";
				break;
			case MEM_FREE:
				state = "free";
				break;
			case MEM_RESERVE:
				state = "reserve";
				break;
		}
		const char *protect = "unknown";
		switch(itr->native.Protect){
			case PAGE_EXECUTE:
				protect = "execute";
				break;
			case PAGE_EXECUTE_READ:
				protect = "execute read";
				break;
			case PAGE_EXECUTE_READWRITE:
				protect = "execute read write";
				break;
			case PAGE_EXECUTE_WRITECOPY:
				protect = "execute write copy";
				break;
			case PAGE_NOACCESS:
				protect = "no access";
				break;
			case PAGE_READONLY:
				protect = "read only";
				break;
			case PAGE_READWRITE:
				protect = "read write";
				break;
			case PAGE_WRITECOPY:
				protect = "write copy";
				break;
			case PAGE_TARGETS_INVALID:
				protect = "targets invalid/no update";
				break;
		}
		
		int len = sprintf(send_buf, "0x%p - 0x%p %zu type: %s, state: %s, protect: %s\n", itr->native.BaseAddress, (size_t)itr->native.BaseAddress + itr->native.RegionSize - 1, itr->native.RegionSize, type, state, protect);
		ret = send_till_done(fd, send_buf, len, 0);
		if(ret <= 0){
			return ret;
		}
		total_sent = total_sent + ret;
		itr++;
	}

	static const char padding[] = "\n\n";
	ret = send_till_done(fd, padding, sizeof(padding), 0);
	if(ret <= 0){
		return ret;
	}
	total_sent = total_sent + ret;
	

	return total_sent;
}

static int memory_utils_menu(int fd){
	while(true){
		static const char invalid_input[] = "sorry I don't understand :(\n";

		static const char splash[] = "okay, I can do these things with memory\n"
			"(WIP)search_strings\n"
			"(WIP)search_bytes\n"
			"(WIP)search_uint32\n"
			"(WIP)search_uint64\n"
			"(WIP)search_int32\n"
			"(WIP)search_int64\n"
			"(WIP)search_int64\n"
			"show_memory_layout\n"
			"dump_memory <output file> <base address in hex> <size>\n"
			"back\n"
			"\n\n>";
		if(send_till_done(fd, splash, sizeof(splash), 0) != sizeof(splash)){
			return -1;
		}

		char cmd_buf[512] = {0};
		if(recv_till_new_line(fd, cmd_buf, sizeof(cmd_buf), 0) <= 0){
			return -1;
		}

		char *cmd = trim_string_until(cmd_buf, sizeof(cmd_buf), '\n');
		cmd = trim_string_until(cmd_buf, sizeof(cmd_buf), '\r');
		size_t cmd_offset = (size_t)cmd - (size_t)cmd_buf;

		static const char cmd_show_memory_layout[] = "show_memory_layout";
		if(strncmp(cmd_show_memory_layout, cmd, sizeof(cmd_show_memory_layout) - 1) == 0){
			auto regions = enumerate_memory_regions();
			int ret = send_memory_regions(fd, regions);
			if(ret <= 0){
				return -1;
			}
		}

		static const char cmd_dump_memory[] = "dump_memory";
		if(strncmp(cmd_dump_memory, cmd, sizeof(cmd_dump_memory) - 1) == 0){
			char opt_buf[512] = {0};
			// drop the main command
			const char *next_cmd = get_one_opt(cmd, strlen(cmd_buf) - cmd_offset, opt_buf, sizeof(opt_buf) - 1, ' ');
			cmd_offset = (size_t)cmd - (size_t)cmd_buf;
			cmd = (char *)next_cmd;

			memset(opt_buf, 0, sizeof(opt_buf));
			next_cmd = get_one_opt(cmd, strlen(cmd_buf) - cmd_offset, opt_buf, sizeof(opt_buf) - 1, ' ');
			if(cmd == next_cmd){
				int ret = send_till_done(fd, invalid_input, sizeof(invalid_input), 0);
				if(ret <= 0){
					return ret;
				}
				continue;
			}
			char path_buf[512] = {0};
			strcpy(path_buf, opt_buf);
			cmd = (char *)next_cmd;
			cmd_offset = (size_t)cmd - (size_t)cmd_buf;

			memset(opt_buf, 0, sizeof(opt_buf));
			next_cmd = get_one_opt(cmd, strlen(cmd_buf) - cmd_offset, opt_buf, sizeof(opt_buf) - 1, ' ');
			if(cmd == next_cmd){
				int ret = send_till_done(fd, invalid_input, sizeof(invalid_input), 0);
				if(ret <= 0){
					return ret;
				}
				continue;
			}
			size_t begin = (size_t)strtoull(opt_buf, NULL, 0);
			cmd = (char *)next_cmd;
			cmd_offset = (size_t)cmd - (size_t)cmd_buf;

			memset(opt_buf, 0, sizeof(opt_buf));
			next_cmd = get_one_opt(cmd, strlen(cmd_buf) - cmd_offset, opt_buf, sizeof(opt_buf) - 1, ' ');
			if(cmd == next_cmd){
				int ret = send_till_done(fd, invalid_input, sizeof(invalid_input), 0);
				if(ret <= 0){
					return ret;
				}
				continue;
			}
			size_t size = (size_t)strtoull(opt_buf, NULL, 0);
			cmd = (char *)next_cmd;
			cmd_offset = (size_t)cmd - (size_t)cmd_buf;

			char resp_buf[1024];
			int written = write_memory_to_file(path_buf, (void *)begin, size);
			if(written == size){
				sprintf(resp_buf, "wrote %zu bytes from 0x%p to %s :D\n", size, begin, path_buf);
			}else{
				sprintf(resp_buf, "only wrote %zu bytes out of %zu bytes from 0x%p to %s :(\n", written, size, begin, path_buf);
			}
			int ret = send_till_done(fd, resp_buf, strlen(resp_buf), 0);
			if(ret <= 0){
				return ret;
			}
		}


		static const char cmd_back[] = "back";
		if(strncmp(cmd_back, cmd, sizeof(cmd_back) - 1) == 0){
			break;
		}
	}
	return 0;
}

#endif
