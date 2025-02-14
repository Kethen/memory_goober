#ifndef __SOCKET_UTIL_H
#define __SOCKET_UTIL_H
#include <winsock2.h>
#include <windows.h>

#include <stdio.h>

static int send_till_done(int fd, const char *buf, int len, int flags){
	int sent = 0;
	while(sent < len){
		int just_sent = send(fd, &buf[sent], len - sent, flags);
		if(just_sent == SOCKET_ERROR){
			return just_sent;
		}
		sent = sent + just_sent;
	}
	return sent;
}

static int recv_till_done(int fd, char *buf, int len, int flags){
	int received = 0;
	while(received < len){
		int just_received = recv(fd, &buf[received], len - received, flags);
		if(just_received <= 0){
			return just_received;
		}
		received = received + just_received;
	}
	return received;
}

static int recv_till_new_line(int fd, char *buf, int len, int flags){
	int received = 0;
	while(received < len){
		int just_received = recv(fd, &buf[received], 1, flags);
		if(just_received <= 0){
			return just_received;
		}
		received = received + just_received;
		if(buf[received - 1] == '\n'){
			return received;
		}
	}
	return received;
}
#endif
