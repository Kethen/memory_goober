#include <windows.h>

#include <stdlib.h>
#include <stdio.h>

#include <unistd.h>

int main(){
	if(LoadLibraryA("memory_goober.asi") != NULL || LoadLibraryA("memory_goober64.asi")){
		printf("goober loaded\n");
	}else{
		printf("failed loading goober :(\n");
		exit(1);
	}
	while(true){
		sleep(3600);
	}
}
