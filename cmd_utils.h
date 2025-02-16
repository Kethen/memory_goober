#ifndef __CMD_UTILS_H
#define __CMD_UTILS_H

static const char* string_until_not(const char *orig, int len, char n){
	for(int i = 0;i < len; i++){
		if(orig[i] != n){
			return &orig[i];
		}
	}
	return &orig[len - 1];
}

static char* trim_string_until(char *orig, int len, char until){
	char *begin = (char *)string_until_not(orig, len, ' ');
	uint64_t offset = (uint64_t)begin - (uint64_t)len;
	for(int i = 0;i < len - offset; i++){
		if(begin[i] == until){
			begin[i] = 0;
			break;
		}
	}
	return begin;
}

static const char* get_one_opt(const char *orig, int len, char *out_buf, int out_len, char delim){
	for(int i = 0; i < len; i++){
		if(orig[i] != delim){
			break;
		}
		orig = (const char *)((size_t)orig + 1);
		len = len - 1;
	}
	for(int i = 0; i < len && i < out_len; i++){
		if(orig[i] == delim ){
			return (const char *)((size_t)orig + i);
		}
		out_buf[i] = orig[i];
	}
	return (const char *)((size_t)orig + (len > out_len? out_len : len));
}

#endif
