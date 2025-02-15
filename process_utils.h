#ifndef __PROCESS_UTILS_H
#define __PROCESS_UTILS_H

#include <windows.h>
#include <psapi.h>

#include <stdint.h>
#include <string.h>

#include <vector>

struct module_info {
	char name[512];
	MODULEINFO native;
};

// https://learn.microsoft.com/en-us/windows/win32/psapi/enumerating-all-modules-for-a-process
static std::vector<module_info> enumerate_modules(){
	HMODULE modules[1024];
	DWORD num_modules = 0;
	std::vector<module_info> ret;
	int res = EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules), &num_modules);
	if(res == 0){
		return ret;
	}
	for(uint32_t i = 0;i < num_modules;i++){
		struct module_info p = {0};
		res = GetModuleFileNameExA(GetCurrentProcess(), modules[i], p.name, sizeof(p.name));
		if(res == 0){
			memset(p.name, 0, sizeof(p.name));
		}
		res = GetModuleInformation(GetCurrentProcess(), modules[i], &p.native, sizeof(p.native));
		if(res == 0){
			continue;
		}
		ret.push_back(p);
	}
	return ret;
}

#endif
