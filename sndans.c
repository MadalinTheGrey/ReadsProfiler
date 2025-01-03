#include "sndans.h"
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#define ANSWER_SIZE 512

void sendRes(int fd, char answer[ANSWER_SIZE])
{
	int bytes = strlen(answer); //necessary to check for errors in write
	
	if(bytes != write(fd, answer, bytes))
	{
		perror("Error on write() to client");
		return;
	}
	bzero(answer, ANSWER_SIZE);
}
