#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>

#define MSG_SIZE 500

/* codul de eroare returnat de anumite apeluri */
extern int errno;

/* portul de conectare la server*/
int port;

int main (int argc, char *argv[])
{
  int sd;			// descriptorul de socket
  int connection = 1; //determines if the client is still sending requests (1 if yes, 0 otherwise)
  struct sockaddr_in server;	// structura folosita pentru conectare 
  char msg[MSG_SIZE];		// folosit pentru comunicarea cu serverul

  /* exista toate argumentele in linia de comanda? */
  if (argc != 3)
    {
      printf ("[client] Sintaxa: %s <adresa_server> <port>\n", argv[0]);
      return -1;
    }

  /* stabilim portul */
  port = atoi (argv[2]);

  /* cream socketul */
  if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
      perror ("[client] Eroare la socket().\n");
      return errno;
    }
  

  /* umplem structura folosita pentru realizarea conexiunii cu serverul */
  /* familia socket-ului */
  server.sin_family = AF_INET;
  /* adresa IP a serverului */
  server.sin_addr.s_addr = inet_addr(argv[1]);
  /* portul de conectare */
  server.sin_port = htons (port);
  
  /* ne conectam la server */
  if (connect (sd, (struct sockaddr *) &server,sizeof (struct sockaddr)) == -1)
    {
      perror ("[client]Eroare la connect().\n");
      return errno;
    }

	while(connection == 1)
	{
		/* citirea mesajului */
		bzero (msg, MSG_SIZE);
		printf ("[client]Introduceti comanda: ");
		fflush (stdout);
		read (0, msg, MSG_SIZE);
	  
		/* trimiterea mesajului la server */
		if (write (sd, msg, MSG_SIZE) <= 0)
		{
			perror ("[client]Eroare la write() spre server.\n");
			return errno;
		}
		
		//resetam msg pentru a citi mesajul de la server
		bzero(msg, MSG_SIZE);
		
		/* citirea raspunsului dat de server 
		(apel blocant pina cind serverul raspunde) */
		if (read (sd, msg, MSG_SIZE) < 0)
		{
			perror ("[client]Eroare la read() de la server.\n");
			return errno;
		}
		/* afisam mesajul primit */
		printf ("[client]Mesajul primit este: %s\n", msg);
		
		//if command 9 has been confirmed we end the request loop
		if(strcmp(msg, "Utilizator delogat cu succes!") == 0)
			connection = 0;
	}

  /* inchidem conexiunea, am terminat */
  close (sd);
}

