#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <fcntl.h>

#define MSG_SIZE 512
//marks the end of the message from the server
#define EOM "01END10"
//beginning and end marker for book download
#define BOOK_DOWNLOAD "8005BOOK5008"

/* codul de eroare returnat de anumite apeluri */
extern int errno;

/* portul de conectare la server*/
int port;

int main (int argc, char *argv[])
{
  int sd;			// descriptorul de socket
  int connection = 1; //determines if the client is still sending requests (1 if yes, 0 otherwise)
  int downloading_book = 0; //1 if book download is in progress, 0 otherwise
  int book_fd;
  char nume_carte[60]; //numele cartii pe care o descarcam
  int exit_code, len;
  struct sockaddr_in server;	// structura folosita pentru conectare 
  char msg[MSG_SIZE], *s, ch = '1';		// folosit pentru comunicarea cu serverul

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
		printf("[server]");
		while ((exit_code = read (sd, msg, MSG_SIZE)))
		{
			if(exit_code < 0)
			{
				perror("[client] Error on read\n");
			}
			len = 0;
			if(strncmp(msg, BOOK_DOWNLOAD, 12) == 0)
			{
				if(downloading_book == 0)
				{
					//confirmam citirea
					write(sd, &ch, 1);
					
					downloading_book = 1;
					strcpy(nume_carte, msg + 13);
					if((book_fd = open(nume_carte, O_WRONLY | O_CREAT | O_TRUNC, 0666)) == -1)
					{
						perror("[client] Error on open()");
					}
				}
				else
				{
					//confirmam citirea
					write(sd, &ch, 1);
					
					downloading_book = 0;
					close(book_fd);
				}
			}
			else if(downloading_book == 1)
			{
				//confirmam citirea
				write(sd, &ch, 1);
					
				if(write(book_fd, msg, strlen(msg)) < 0)
				{
					perror("[client] Error on write()");
				}
			}
			else if((s = strstr(msg, EOM)) != NULL)
			{
				//determine length of message without EOM
				len = strlen(msg) - 7;
				if(len == 0) break; //string can contain just the EOM in which case we end the loop right away
				//if command 9 has been confirmed we end the request loop
				if(strncmp(msg, "Utilizator delogat cu succes!", len) == 0)
					connection = 0;
				//print the message without the EOM and end the read loop
				printf("%.*s", len, msg);
				break;
			}
			else printf("%s", msg);
			bzero(msg, MSG_SIZE);
		}
		printf ("\n");
	}

  /* inchidem conexiunea, am terminat */
  close (sd);
}

