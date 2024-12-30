#include "rqprcs.h"
#include <string.h>
#include <sqlite3.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#define NUME_DB "readsprofiler.db"
#define ANSWER_SIZE 500

int alfanumeric(char ch)
{
	if(('0' > ch || ch > '9') && ('a' > ch || ch > 'z')) return 0;
	return 1;
}

static int verify_existence(void *result, int count, char **data, char **columns)
{
	int *x = result;
	if(count > 0)
		x[0] = 1;
	return 0;
}

void prcsReq(int command, char answer[ANSWER_SIZE], int logged[100], int fd)
{
	sqlite3* DB;
	int exit_code = 0, corect, k, index, *result = malloc(sizeof(int));
	char sql[500], username[21], password[21];
	
	exit_code = sqlite3_open(NUME_DB, &DB);
	if(exit_code)
	{
		perror("Error on database open\n");
		return;
	}
	
	switch(command)
	{
		case 1: if(logged[fd] == 1)
					strcpy(answer, "Sunteti deja in cont.");
				else
				{
					corect = 1;
					//verificam ca inputul este in forma corecta (create account username password\n)
					if(answer[14] != ' ') corect = 0;
					k = 15;
					while(corect == 1 && alfanumeric(answer[k]) == 1)
					{
						if(k - 14 > 20)
							corect = 0;
						else
						{
							username[k - 15] = answer[k];
							k++;
						}
					}
					if(corect == 1 && answer[k] == ' ')
					{
						username[k - 15] = '\0';
						k++;
						index = k;
					}
					else corect = 0;
					while(corect == 1 && alfanumeric(answer[k]) == 1)
					{
						if(k - 14 > 20)
							corect = 0;
						else
						{
							password[k - index] = answer[k];
							k++;
						}
					}
					if(corect == 1 && answer[k] == '\n')
					{
						password[k - index] = '\0';
						k++;
					}
					else corect = 0;
					bzero(answer, ANSWER_SIZE);
					if(corect == 0)
						strcpy(answer, "Comanda este de forma: \"create account username password\" unde username si password sunt alfanumerice si au lungimea maxim 20");
					else
					{
						//de facut verificare daca exista username-ul
						//de facut github cu proiectul ca sa nu ajung la varianta care nu mai merge :)))))
						strcpy(sql, "select id from users where username = '");
						strcat(sql, username);
						strcat(sql, "';");
						exit_code = sqlite3_exec(DB, sql, verify_existence, result, NULL);
						if(exit_code != SQLITE_OK)
							perror("Error on select from users\n");
						if(result[0] == 1)
							strcpy(answer, "Numele de utilizator exista deja");
						//de adaugat username si password in baza de date daca nu exista deja
						free(result);
					}
				}
				break;
		case 2: if(logged[fd] == 1)
					strcpy(answer, "Sunteti deja in cont.");
				break;
		case 3: if(logged[fd] == 0)
					strcpy(answer, "Pentru a executa comanda 'info cont' trebuie sa fiti logat.");
				break;
		case 4: strcpy(answer, "Comanda identificata: 4. cautare");
				break;
		case 5: strcpy(answer, "Comanda identificata: 5. info carte");
				break;
		case 6: if(logged[fd] == 0)
					strcpy(answer, "Pentru a executa comanda 'rate' trebuie sa fiti logat.");
				break;
		case 7: strcpy(answer, "Comanda identificata: 7. descarcare");
				break;
		case 8: if(logged[fd] == 0)
					strcpy(answer, "Pentru a executa comanda 'recomandari' trebuie sa fiti logat.");
				break;
		case 9: logged[fd] = 0;
				strcpy(answer, "Utilizator delogat cu succes!");
				break;
		default: strcpy(answer, "Comanda nu exista");
	}
	sqlite3_close(DB);
}
