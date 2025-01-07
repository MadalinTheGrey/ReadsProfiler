#include "rqid.h"
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#define REQ_SIZE 512

int idReq(int fd, char req[REQ_SIZE])
{
	int bytes; //necessary to check for errors in read
	//req saves request given by client
	
	bytes = read(fd, req, REQ_SIZE);
	if(bytes < 0)
	{
		perror("Erorr on read() from client.\n");
		return 0;
	}

	if(strncmp(req, "create account", 14) == 0)
		return 1;
	else if(strncmp(req, "log in", 6) == 0)
		return 2;
	else if(strcmp(req, "info cont\n") == 0)
		return 3;
	else if(strncmp(req, "cautare", 7) == 0)
		return 4;
	else if(strncmp(req, "info carte", 10) == 0)
		return 5;
	else if(strncmp(req, "rate", 4) == 0)
		return 6;
	else if(strcmp(req, "descarcare\n") == 0)
		return 7;
	else if(strcmp(req, "recomandari\n") == 0)
		return 8;
	else if(strcmp(req, "log out\n") == 0)
		return 9;
	return 0;
}
