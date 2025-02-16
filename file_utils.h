#ifndef __FILE_UTILS
#define __FILE_UTILS

#include <stdio.h>

#include <windows.h>
#include <memoryapi.h>

static int write_memory_to_file(const char *path, void *begin, size_t len){
	int ret;

	FILE *file = fopen(path, "wb");
	if(file == NULL){
		return 0;
	}

	for(int i = 0;i < len; i++){
		uint8_t data;
		#if 0
		ret = ReadProcessMemory(GetCurrentProcess(), (void *)((size_t)begin + i), &data, 1, NULL);
		if(ret == 0){
			LOG("%s: %p cannot be read\n", __func__, (size_t)begin + i);
			data = 0;
		}
		#else
		data = *(uint8_t *)((size_t)begin + i);
		#endif

		ret = fwrite(&data, 1, 1, file);
		if(ret != 1){
			return i;
		}
	}
	return len;

	#if 0
	char *buf = (char *) malloc(len);
	if(buf == NULL){
		free(buf);
		return 0;
	}

	// hmm fwrite can't directly read sometimes
	int ret = ReadProcessMemory(GetCurrentProcess(), begin, buf, len, NULL);
	if(ret == 0){
		free(buf);
		return 0;
	}

	ret = fwrite(buf, len, 1, file) * len;
	free(buf);

	fclose(file);
	return ret;
	#endif
}

#endif
