#include "sndans.h"
#include <string.h>
#include <unistd.h>
#include <stdio.h>

void sendRes(int fd, char answer[100])
{
	int bytes = strlen(answer); //necessary to check for errors in write
	
	if(bytes != write(fd, answer, bytes))
	{
		perror("Error on write() to client");
		return;
	}
	
}
