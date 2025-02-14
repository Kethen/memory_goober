#include <windows.h>
#include <stdint.h>
#include <string.h>

static uint32_t load_library(const char *path){
	if(LoadLibraryA(path) != NULL){
		return 0;
	}else{
		return GetLastError();
	}
}

static uint32_t free_library(const char *name){
	char name_buf[512] = {0};
	strcpy(name_buf, name);
	HMODULE lib = GetModuleHandleA(name_buf);
	if(lib == NULL){
		return GetLastError();
	}
	if(FreeLibrary(lib) == 0){
		return GetLastError();
	}
	return 0;
}
