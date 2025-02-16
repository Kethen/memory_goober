#ifndef __PROCESS_UTILS_H
#define __PROCESS_UTILS_H

#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>

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

static HANDLE pause_threads(){
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if(snapshot == INVALID_HANDLE_VALUE){
		return INVALID_HANDLE_VALUE;
	}

	THREADENTRY32 entry;
	entry.dwSize = sizeof(entry);

	if(!Thread32First(snapshot, &entry)){
		CloseHandle(snapshot);
		return INVALID_HANDLE_VALUE;
	}

	pthread_mutex_lock(&goober_tids_mutex);
	do{
		if(entry.th32OwnerProcessID == GetProcessId(GetCurrentProcess())){
			if(entry.th32ThreadID == GetThreadId(GetCurrentThread())){
				continue;
			}
			auto itr = goober_tids.begin();
			while(itr != goober_tids.end()){
				if(*itr == entry.th32ThreadID){
					break;
				}
				itr++;
			}
			if(itr != goober_tids.end()){
				continue;
			}
			LOG("%s: pausing thread %u\n", __func__, entry.th32ThreadID);
			HANDLE t = OpenThread(THREAD_SUSPEND_RESUME, false, entry.th32ThreadID);
			if(t == NULL){
				LOG("%s: failed fetching handle to pause thread %u, 0x%08x\n", __func__, entry.th32ThreadID, GetLastError());
				continue;
			}
			// it this errors out, what to do?
			if(SuspendThread(t) == -1){
				LOG("%s: failed suspending thread %u, 0x%08x\n", __func__, entry.th32ThreadID, GetLastError());
			}
			CloseHandle(t);
		}
	}while(Thread32Next(snapshot, &entry));
	pthread_mutex_unlock(&goober_tids_mutex);

	return snapshot;
}

static void resume_threads(HANDLE snapshot){
	THREADENTRY32 entry;
	entry.dwSize = sizeof(entry);

	if(!Thread32First(snapshot, &entry)){
		LOG("%s: failed enumerating threads from snapshot, things might be stuck now\n", __func__);
		CloseHandle(snapshot);
		return;
	}

	pthread_mutex_lock(&goober_tids_mutex);
	do{
		if(entry.th32OwnerProcessID == GetProcessId(GetCurrentProcess())){
			if(entry.th32ThreadID == GetThreadId(GetCurrentThread())){
				continue;
			}
			auto itr = goober_tids.begin();
			while(itr != goober_tids.end()){
				if(*itr == entry.th32ThreadID){
					break;
				}
				itr++;
			}
			if(itr != goober_tids.end()){
				continue;
			}
			LOG("%s: resuming thread %u\n", __func__, entry.th32ThreadID);
			HANDLE t = OpenThread(THREAD_SUSPEND_RESUME, false, entry.th32ThreadID);
			if(t == NULL){
				LOG("%s: failed fetching handle to resume thread %u, 0x%08x\n", __func__, entry.th32ThreadID, GetLastError());
				continue;
			}
			if(ResumeThread(t) == -1){
				LOG("%s: failed resuming thread %u, 0x%08x\n", __func__, entry.th32ThreadID, GetLastError());
			}
			CloseHandle(t);
		}
	}while(Thread32Next(snapshot, &entry));
	pthread_mutex_unlock(&goober_tids_mutex);

	CloseHandle(snapshot);
}

#endif
