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

static int get_column0_number(void *result, int count, char **data, char **columns)
{
	int *x = result;
	if(sscanf(data[0], "%d", x) == EOF)
	{
		perror("sscanf");
		return 1;
	}
	return 0;
}

static int get_col0_string(void *result, int count, char **data, char **columns)
{
	char *s = result;
	strcpy(s, data[0]);
	return 0;
}

void prcsReq(int command, char answer[ANSWER_SIZE], int logged[100], int fd)
{
	sqlite3* DB;
	int exit_code = 0, corect, k, index, *result = malloc(sizeof(int));
	char sql[ANSWER_SIZE], username[21], password[21], number[21], *char_result = malloc(200);
	
	exit_code = sqlite3_open(NUME_DB, &DB);
	if(exit_code)
	{
		perror("Error on database open\n");
		return;
	}
	
	//pregatim vectorul sql pentru transmiterea de comenzi
	bzero(sql, ANSWER_SIZE);
	bzero(number, 21);
	
	//identificam comanda
	switch(command)
	{
		//create account
		case 1: if(logged[fd] != 0)
					strcpy(answer, "Sunteti deja in cont.");
				else
				{
					corect = 1;
					//verificam ca inputul este in forma corecta (create account username password\n)
					if(answer[14] != ' ') corect = 0;
					k = 15;
					while(corect == 1 && alfanumeric(answer[k]) == 1)
					{
						//lungimea este maxim 20
						if(k - 14 > 20)
							corect = 0;
						//copiem caracterele usernameului pentru verificare si eventuala salvare
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
						//lungimea este maxim 20
						if(k - index > 20)
							corect = 0;
						//copiem caracterele parolei pentru eventuala salvare
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
					
					//resetam answer
					bzero(answer, ANSWER_SIZE);
					
					if(corect == 0)
						strcpy(answer, "Comanda este de forma: \"create account username password\" unde username si password sunt alfanumerice si au lungimea maxim 20.");
					else
					{
						//verificare daca exista username-ul
						sprintf(sql, "select id from users where username = '%s';", username);
						exit_code = sqlite3_exec(DB, sql, verify_existence, result, NULL);
						if(exit_code != SQLITE_OK)
						{
							perror("Error on select from users\n");
							strcpy(answer, "Eroare verificare nume");
							break;
						}
						if(result[0] == 1)
						{
							strcpy(answer, "Numele de utilizator exista deja");
							break;
						}
						
						//adaugam username si password in baza de date daca nu exista deja
						//selectam id-ul maxim pentru a obtine urmatorul id
						strcpy(sql, "select max(id) from users;");
						exit_code = sqlite3_exec(DB, sql, get_column0_number, result, NULL);
						if(exit_code != SQLITE_OK)
						{
							perror("Error on select from users\n");
							strcpy(answer, "Eroare creare utilizator");
							break;
						}
						result[0]++;
						
						//formam comanda catre database pentru a insera informatiile despre noul cont
						sprintf(sql, "insert into users values(%d, '%s', '%s');", result[0], username, password);
						exit_code = sqlite3_exec(DB, sql, NULL, NULL, NULL);
						if(exit_code != SQLITE_OK)
						{
							perror("Error on insert into users\n");
							strcpy(answer, "Eroare creare utilizator");
							break;
						}
						
						//contul a fost creat asa ca salvam id-ul si trimitem confirmare
						logged[fd] = result[0];
						strcpy(answer, "Contul a fost creat cu succes!");
						free(result);
					}
				}
				break;
		//log in
		case 2: if(logged[fd] != 0)
					strcpy(answer, "Sunteti deja in cont.");
				else
				{
					corect = 1;
					//verificam ca inputul este in forma corecta (log in username password\n)
					if(answer[6] != ' ') corect = 0;
					k = 7;
					while(corect == 1 && alfanumeric(answer[k]) == 1)
					{
						//lungimea este maxim 20
						if(k - 7 > 20)
							corect = 0;
						//copiem caracterele usernameului pentru verificare
						else
						{
							username[k - 7] = answer[k];
							k++;
						}
					}
					if(corect == 1 && answer[k] == ' ')
					{
						username[k - 7] = '\0';
						k++;
						index = k;
					}
					else corect = 0;
					while(corect == 1 && alfanumeric(answer[k]) == 1)
					{
						//lungimea este maxim 20
						if(k - index > 20)
							corect = 0;
						//copiem caracterele parolei pentru verificare
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
					
					//resetam answer
					bzero(answer, ANSWER_SIZE);
					
					if(corect == 0)
						strcpy(answer, "Comanda este de forma: \"log in username password\"");
					else
					{
						//verificare daca exista contul
						sprintf(sql, "select id from users where username = '%s' and password = '%s';", username, password);
						exit_code = sqlite3_exec(DB, sql, verify_existence, result, NULL);
						if(exit_code != SQLITE_OK)
						{
							perror("Error on select from users\n");
							strcpy(answer, "Eroare conectare");
							break;
						}
						//contul a fost gasit -> confirmam conectarea
						if(result[0] == 1)
						{
							//cautam id-ul utilizatorului acum ca stim ca exista pentru a-l retine
							sprintf(sql, "select id from users where username = '%s';", username);
							exit_code = sqlite3_exec(DB, sql, get_column0_number, result, NULL);
							if(exit_code != SQLITE_OK)
							{
								perror("Error on select from users\n");
								strcpy(answer, "Eroare conectare");
								break;
							}
							logged[fd] = result[0];
							strcpy(answer, "Conectarea a avut succes!");
							break;
						}
						//contul nu exista
						else
						{
							strcpy(answer, "Nume de utilizator sau parola incorecte");
							break;
						}
					}
				}
				break;
		//info cont
		case 3: if(logged[fd] == 0)
					strcpy(answer, "Pentru a executa comanda 'info cont' trebuie sa fiti conectat.");
				else
				{
					//preluam numele de utilizator si parola din database 
					sprintf(sql, "select username from users where id = %d;", logged[fd]);
					exit_code = sqlite3_exec(DB, sql, get_col0_string, char_result, NULL);
					if(exit_code != SQLITE_OK)
					{
						perror("Error on select from users\n");
						strcpy(answer, "Eroare conectare");
						break;
					}
					sprintf(answer, "username: %s\n", char_result);
					//de trimis si rating-urile oferite
				}
				break;
		//cautare
		case 4: strcpy(answer, "Comanda identificata: 4. cautare");
				break;
		//info carte
		case 5: strcpy(answer, "Comanda identificata: 5. info carte");
				break;
		//rate
		case 6: if(logged[fd] == 0)
					strcpy(answer, "Pentru a executa comanda 'rate' trebuie sa fiti conectat.");
				break;
		//descarcare
		case 7: strcpy(answer, "Comanda identificata: 7. descarcare");
				break;
		//recomandari
		case 8: if(logged[fd] == 0)
					strcpy(answer, "Pentru a executa comanda 'recomandari' trebuie sa fiti conectat.");
				break;
		//log out
		case 9: logged[fd] = 0;
				strcpy(answer, "Utilizator delogat cu succes!");
				break;
		default: strcpy(answer, "Comanda nu exista.");
	}
	free(result);
	free(char_result);
	sqlite3_close(DB);
}
