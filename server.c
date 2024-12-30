#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <string.h>
#include "rqid.h"
#include "rqprcs.h"
#include "sndans.h"

#define PORT 2025
#define ANSWER_SIZE 500

/* codul de eroare returnat de anumite apeluri */
extern int errno;

int main ()
{
	struct sockaddr_in server;	/* structurile pentru server si clienti */
	struct sockaddr_in from;
	fd_set readfds;		/* multimea descriptorilor de citire */
	fd_set actfds;		/* multimea descriptorilor activi */
	struct timeval tv;		/* structura de timp pentru select() */
	int sd, client;		/* descriptori de socket */
	int optval=1; 			/* optiune folosita pentru setsockopt()*/ 
	int fd;			/* descriptor folosit pentru 
				   parcurgerea listelor de descriptori */
	int nfds;			/* numarul maxim de descriptori */
	int len;			/* lungimea structurii sockaddr_in */
	int command; //number that helps identify which command should be executed
	char answer[ANSWER_SIZE]; //saves the result of prcsReq() to send to the client
	int logged[100]; //remembers which clients are logged in

	/* creare socket */
	if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
		perror ("[server] Eroare la socket().\n");
		return errno;
    }

	/*setam pentru socket optiunea SO_REUSEADDR */ 
	setsockopt(sd, SOL_SOCKET, SO_REUSEADDR,&optval,sizeof(optval));

	/* pregatim structurile de date */
	bzero (&server, sizeof (server));

	/* umplem structura folosita de server */
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = htonl (INADDR_ANY);
	server.sin_port = htons (PORT);

	/* atasam socketul */
	if (bind (sd, (struct sockaddr *) &server, sizeof (struct sockaddr)) == -1)
    {
		perror ("[server] Eroare la bind().\n");
		return errno;
    }

	/* punem serverul sa asculte daca vin clienti sa se conecteze */
	if (listen (sd, 5) == -1)
    {
		perror ("[server] Eroare la listen().\n");
		return errno;
    }
  
	/* completam multimea de descriptori de citire */
	FD_ZERO (&actfds);		/* initial, multimea este vida */
	FD_SET (sd, &actfds);		/* includem in multime socketul creat */

	tv.tv_sec = 1;		/* se va astepta un timp de 1 sec. */
	tv.tv_usec = 0;
  
	/* valoarea maxima a descriptorilor folositi */
	nfds = sd;
	memset(logged, 0, 100 * sizeof(logged[0]));
	
	printf ("[server] Asteptam la portul %d...\n", PORT);
	fflush (stdout);
        
	/* servim in mod concurent clientii... */
	while (1)
    {
		/* ajustam multimea descriptorilor activi (efectiv utilizati) */
		bcopy ((char *) &actfds, (char *) &readfds, sizeof (readfds));

		/* apelul select() */
		if (select (nfds+1, &readfds, NULL, NULL, &tv) < 0)
		{
			perror ("[server] Eroare la select().\n");
			return errno;
		}
		/* vedem daca e pregatit socketul pentru a-i accepta pe clienti */
		if (FD_ISSET (sd, &readfds))
		{
			/* pregatirea structurii client */
			len = sizeof (from);
			bzero (&from, sizeof (from));

			/* a venit un client, acceptam conexiunea */
			client = accept (sd, (struct sockaddr *) &from, &len);

			/* eroare la acceptarea conexiunii de la un client */
			if (client < 0)
			{
				perror ("[server] Eroare la accept().\n");
				continue;
			}

			if (nfds < client) /* ajusteaza valoarea maximului */
				nfds = client;
            
			/* includem in lista de descriptori activi si acest socket */
			FD_SET (client, &actfds);
			fflush (stdout);
		}
		/* vedem daca e pregatit vreun socket client pentru a trimite raspunsul */
		for (fd = 0; fd <= nfds; fd++)	/* parcurgem multimea de descriptori */
		{
			/* este un socket de citire pregatit? */
			if (fd != sd && FD_ISSET (fd, &readfds))
			{
				bzero(answer, ANSWER_SIZE);
				command = idReq(fd, answer);
				prcsReq(command, answer, logged, fd);
				sendRes(fd, answer);
				if(command == 9)
				{
					fflush (stdout);
					close (fd);		/* inchidem conexiunea cu clientul */
					FD_CLR (fd, &actfds);/* scoatem si din multime */
				}
			}
		}			/* for */
    }			/* while */
}			/* main */
