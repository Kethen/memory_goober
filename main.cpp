#include <winsock2.h>
#include <windows.h>
#include <psapi.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include <unistd.h>
#include <pthread.h>

#include "socket_utils.h"
#include "library_utils.h"

static FILE *log_file = NULL;
#define LOG(...){ \
	if(log_file == NULL){ \
		log_file = fopen("./memory_goober.log", "w"); \
	} \
	if(log_file != NULL){ \
		fprintf(log_file, __VA_ARGS__); \
		fflush(log_file); \
	} \
}

struct session_ctx{
	sockaddr_in incoming_addr;
	int fd;
};

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

static void *session(void *arg){
	struct session_ctx *ctx = (struct session_ctx*)arg;
	while(true){
		static const char initial_message[] = "\n\ncaw caw! tell me what to do!\n"
			"load_library <library path>\n"
			"free_library <library name>\n"
			"read_memory <begin> <len>\n"
			"(WIP)search_memory\n"
			"\n\n>"
		;
		if(send_till_done(ctx->fd, initial_message, sizeof(initial_message), 0) != sizeof(initial_message)){
			const char* str_addr = inet_ntoa(ctx->incoming_addr.sin_addr);
			int port = ntohs(ctx->incoming_addr.sin_port);
			LOG("%s: connection from %s:%d closed during sending\n", __func__, str_addr, port);
			break;
		}

		char cmd_buf[512] = {0};
		int len = recv_till_new_line(ctx->fd, cmd_buf, sizeof(cmd_buf), 0);
		if(len == SOCKET_ERROR){
			const char *recv_err_msg = "sorry I don't understand, say it again :(\n";
			send_till_done(ctx->fd, recv_err_msg, strlen(recv_err_msg), 0);
			continue;
		}
		if(len == 0){
			// socket closed on the other side
			const char* str_addr = inet_ntoa(ctx->incoming_addr.sin_addr);
			int port = ntohs(ctx->incoming_addr.sin_port);
			LOG("%s: connection from %s:%d closed during reading\n", __func__, str_addr, port);
			break;
		}
		char *cmd = trim_string_until(cmd_buf, sizeof(cmd_buf), '\n');
		cmd = trim_string_until(cmd, strlen(cmd), '\r');
		uint64_t cmd_offset = (uint64_t)cmd - (uint64_t)cmd_buf;

		char response_buf[512] = {0};
		int response_len = 0;

		static const char cmd_load_library[] = "load_library";
		if(strncmp(cmd_buf, cmd_load_library, sizeof(cmd_load_library) - 1) == 0){
			cmd_offset = cmd_offset + sizeof(cmd_load_library) - 1;
			uint32_t ret = load_library(string_until_not(&cmd_buf[cmd_offset], sizeof(cmd_buf) - cmd_offset, ' '));
			if(ret != 0){
				response_len = sprintf(response_buf, "I can't load the library :( 0x%08x\n", ret);
			}else{
				response_len = sprintf(response_buf, "library loaded :D\n");
			}
		}

		static const char cmd_free_library[] = "free_library";
		if(strncmp(cmd_buf, cmd_free_library, sizeof(cmd_free_library) - 1) == 0){
			cmd_offset = cmd_offset + sizeof(cmd_free_library) - 1;
			uint32_t ret = free_library(string_until_not(&cmd_buf[cmd_offset], sizeof(cmd_buf) - cmd_offset, ' '));
			if(ret != 0){
				response_len = sprintf(response_buf, "I can't free the lobrary :( 0x%08x\n", ret);
			}else{
				response_len = sprintf(response_buf, "library freed once :D\n");
			}
		}
		if(response_len != 0){
			send_till_done(ctx->fd, response_buf, response_len, 0);
		}
	}

	closesocket(ctx->fd);
	free(ctx);
	return NULL;
}

static void *listen(void *arg){
	sleep(1);

	WSADATA wsa_data;
	if(WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0){
		LOG("%s: WSAStartup failed\n");
		return NULL;
	}

	int tcp_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(tcp_sock == INVALID_SOCKET){
		LOG("%s: socket creation failed, 0x%08x\n", __func__, WSAGetLastError());
		return NULL;
	}

	const char *str_addr = "127.0.0.1";
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(str_addr);

	int ret = 0;
	int port;
	for(port = 27000;port < 27100; port++){
		addr.sin_port = htons(port);
		int ret = bind(tcp_sock, (SOCKADDR *)&addr, sizeof(addr));
		if(ret == 0){
			break;
		}
	}

	if(ret != 0){
		LOG("%s: failed binding socket, 0x%08x\n", __func__, ret);
		closesocket(tcp_sock);
		return NULL;
	}

	if(listen(tcp_sock, 10) != 0){
		LOG("%s: failed listening to socket, 0x%08x\n", __func__, ret);
		closesocket(tcp_sock);
		return NULL;
	}

	char process_name[512] = {0};
	GetProcessImageFileNameA(GetCurrentProcess(), process_name, sizeof(process_name));
	LOG("%s: %s listening on %s:%d\n", __func__, process_name, str_addr, port);

	while(true){
		sockaddr_in incoming_addr;
		int addr_len = sizeof(incoming_addr);
		int incoming_sock = accept(tcp_sock, (SOCKADDR *)&incoming_addr, &addr_len);
		if(incoming_sock == INVALID_SOCKET){
			continue;
		}
		const char *incoming_str_addr = inet_ntoa(incoming_addr.sin_addr);
		const int incoming_port = ntohs(incoming_addr.sin_port);
		LOG("%s: %s:%d accepting connection from %s:%d\n", __func__, str_addr, port, incoming_str_addr, incoming_port);
		struct session_ctx *ctx = (struct session_ctx *)malloc(sizeof(struct session_ctx));
		if(ctx == NULL){
			LOG("%s: ran out of memory while accepting session\n", __func__);
			closesocket(incoming_sock);
			continue;
		}
		ctx->incoming_addr = incoming_addr;
		ctx->fd = incoming_sock;
		pthread_t t;
		if(pthread_create(&t, NULL, session, ctx) != 0){
			LOG("%s: cannot create thread while accepting session\n", __func__);
			free(ctx);
			closesocket(incoming_sock);
			continue;
		}
		pthread_detach(t);
	}

	closesocket(tcp_sock);
	return NULL;
}

static int __attribute__((constructor)) init(){
	pthread_t t;
	if(pthread_create(&t, NULL, listen, NULL) != 0){
		LOG("%s: failed creating thread\n", __func__);
	}
	pthread_detach(t);
	return 0;
}
