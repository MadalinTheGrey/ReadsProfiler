#include "rqprcs.h"
#include "sndans.h"
#include <string.h>
#include <sqlite3.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#define NUME_DB "readsprofiler.db"
#define ANSWER_SIZE 512
#define SQL_SIZE 1024
//end of message from server
#define EOM "01END10"
//beginning and end marker for book download
#define BOOK_DOWNLOAD "8005BOOK5008"

int alfanumeric(char ch)
{
	if(('0' > ch || ch > '9') && ('a' > ch || ch > 'z') && ('A' > ch || ch > 'Z')) return 0;
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
	int exit_code = 0, corect, k, p, index, *result = malloc(sizeof(int)), book_fd, bytes_read;
	char sql[SQL_SIZE], username[21], password[21], number[21], *char_result = malloc(200);
	char titlu[50], genre[60], subgenres[4][60], autor[30]; int an_start, an_end, rating_min; //salveaza filtre comanda cautare
	char aux[ANSWER_SIZE], path[ANSWER_SIZE]; //path retine calea relativa catre carti (cartile sunt salvate intr-un folder denumit "Books" din folderul programului)
	int recoms[5], nr_recoms = 0; //retine isbn-urile cartilor care vor fi date ca recomandari
	
	exit_code = sqlite3_open(NUME_DB, &DB);
	if(exit_code)
	{
		perror("Error on database open\n");
		return;
	}
	
	//pregatim vectorii
	bzero(sql, SQL_SIZE);
	bzero(number, 21);
	bzero(username, 21);
	bzero(password, 21);
	bzero(genre, 60);
	bzero(titlu, 50);
	bzero(autor, 30);
	bzero(recoms, 5);
	for(int i = 0; i < 3; i++)
		bzero(subgenres[i], 60);
	an_start = an_end = rating_min = index = -1;
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
						result[0] = 0;
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
						//verificam intai daca exista intrari in users
						strcpy(sql, "select count(*) from users;");
						exit_code = sqlite3_exec(DB, sql, get_column0_number, result, NULL);
						if(exit_code != SQLITE_OK)
						{
							perror("Error on select from interests\n");
							strcpy(answer, "Eroare info carte database");
							break;
						}
						//daca exista
						if(result[0] != 0)
						{
							//selectam id-ul maxim pentru a obtine urmatorul id
							strcpy(sql, "select max(id) from users;");
							exit_code = sqlite3_exec(DB, sql, get_column0_number, result, NULL);
							if(exit_code != SQLITE_OK)
							{
								perror("Error on select from users\n");
								strcpy(answer, "Eroare creare utilizator");
								break;
							}
						}
						//altfel result[0] va fi 0 si doar il facem 1 pentru a adauga prima intrare
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
						result[0] = 0;
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
				{
					strcpy(answer, "Pentru a executa comanda 'info cont' trebuie sa fiti conectat.");
					strcat(answer, EOM);
				}
				else
				{
					//preluam numele de utilizator si parola din database 
					sprintf(sql, "select username from users where id = %d;", logged[fd]);
					exit_code = sqlite3_exec(DB, sql, get_col0_string, char_result, NULL);
					if(exit_code != SQLITE_OK)
					{
						perror("Error on select from users\n");
						strcpy(answer, "Eroare selectare username");
						strcat(answer, EOM);
						break;
					}
					//trimitem utilizatorului cartile carora le-a oferit rating impreuna cu rating-ul in sine
					sprintf(sql, "select titlu, autor, rating from ratings r join books b on r.isbn = b.isbn where id_user = %d;", logged[fd]);
					sqlite3_stmt *stmt;
					exit_code = sqlite3_prepare_v2(DB, sql, -1, &stmt, NULL);
					if(exit_code != SQLITE_OK)
					{
						perror("Error on select from ratings\n");
						strcpy(answer, "Eroare selectare rating-uri");
						strcat(answer, EOM);
						break;
					}
					if((exit_code = sqlite3_step(stmt)) == SQLITE_ROW)
						sprintf(answer, "username: %s\n%s by %s rating given: %d", char_result, sqlite3_column_text(stmt, 0), sqlite3_column_text(stmt, 1), sqlite3_column_int(stmt, 2));
					else sprintf(answer, "username: %s\nNo ratings.", char_result);
					sendRes(fd, answer);
					while((exit_code = sqlite3_step(stmt)) == SQLITE_ROW)
					{
						sprintf(answer, "\n%s by %s rating given: %d", sqlite3_column_text(stmt, 0), sqlite3_column_text(stmt, 1), sqlite3_column_int(stmt, 2));
						sendRes(fd, answer);
					}
					strcpy(answer, EOM);
					if(exit_code != SQLITE_DONE)
						perror("Database sqlite3_step error");
					sqlite3_finalize(stmt);
				}
				break;
		//cautare
		case 4: corect = 1;
				k = 7;
				if(answer[k] != ' ')
					corect = 0;
				while(answer[k] == ' ')
						k++;
				while(corect == 1)
				{
					if(strncmp(answer + k, "gen:", 4) == 0)
					{
						k += 4;
						while(answer[k] == ' ')
							k++;
						if(answer[k] == '(')
						{
							k++;
							p = k;
							//daca intalnim spatiu inainte de text refuzam
							if(answer[k] == ' ') corect = 0;
							while(corect == 1 && (alfanumeric(answer[p]) == 1 || answer[p] == '-' || answer[p] == ' '))
							{
								//verificam ca lungimea sa nu depaseasca limita
								if(p - k + 1 > 60) 
								{
									corect = 0;
									break;
								}
								//daca intalnim spatii multiple sau inainte de paranteza finala refuzam
								if((answer[p] == ' ' && answer[p + 1] == ' ') || (answer[p] == ' ' && answer[p + 1] == ')'))
									corect = 0;
								genre[p - k] = answer[p];
								p++;
							}
							//daca nu am ajuns la paranteza finala ceva nu a mers bine
							if(answer[p] != ')') 
								corect = 0;
						}
						else corect = 0;
					}
					else if(strncmp(answer + k, "autor:", 6) == 0)
					{
						k += 6;
						while(answer[k] == ' ')
							k++;
						if(answer[k] == '(')
						{
							k++;
							p = k;
							if(answer[k] == ' ') corect = 0;
							while(corect == 1 && (alfanumeric(answer[p]) == 1 || answer[p] == '-' || answer[p] == ' ' || answer[p] == '.'))
							{
								if(p - k + 1 > 30) 
								{
									corect = 0;
									break;
								}
								if((answer[p] == ' ' && answer[p + 1] == ' ') || (answer[p] == ' ' && answer[p + 1] == ')'))
									corect = 0;
								autor[p - k] = answer[p];
								p++;
							}
							if(answer[p] != ')') 
								corect = 0;
						}
						else corect = 0;
					}
					else if(strncmp(answer + k, "subgen:", 7) == 0)
					{
						k += 7;
						index = 0;
						while(answer[k] == ' ')
							k++;
						if(answer[k] == '(')
						{
							k++;
							p = k;
							//parcurgem lista de subgenuri cu un while
							while(answer[p] != ')' &&  corect == 1)
							{
								p = k;
								if(answer[k] == ' ') corect = 0;
								while(corect == 1 && (alfanumeric(answer[p]) == 1 || answer[p] == '-' || answer[p] == ' '))
								{
									if(p - k + 1 > 60) 
									{
										corect = 0;
										break;
									}
									if((answer[p] == ' ' && answer[p + 1] == ' ') || (answer[p] == ' ' && answer[p + 1] == ')'))
										corect = 0;
									subgenres[index][p - k] = answer[p];
									p++;
								}
								//daca am ajuns la virgula crestem index si ne pozitionam la inceputul urmatorului subgen
								if(answer[p] == ',')
								{
									index++;
									p++;
									if(index > 2 || answer[p] != ' ') corect = 0;
									k = p + 1;
									if(answer[k] == ')') corect = 0;
								}
								else if(answer[p] != ')') 
									corect = 0;
							}
						}
						else corect = 0;
					}
					else if(strncmp(answer + k, "titlu:", 6) == 0)
					{
						k += 6;
						while(answer[k] == ' ')
							k++;
						if(answer[k] == '(')
						{
							k++;
							p = k;
							if(answer[k] == ' ') corect = 0;
							while(corect == 1 && (alfanumeric(answer[p]) == 1 || answer[p] == '-' || answer[p] == ' '))
							{
								if(p - k + 1 > 50) 
								{
									corect = 0;
									break;
								}
								if((answer[p] == ' ' && answer[p + 1] == ' ') || (answer[p] == ' ' && answer[p + 1] == ')'))
									corect = 0;
								titlu[p - k] = answer[p];
								p++;
							}
							if(answer[p] != ')') 
								corect = 0;
						}
						else corect = 0;
					}
					else if(strncmp(answer + k, "an:", 3) == 0)
					{
						an_start = an_end = 0;
						k += 3;
						while(answer[k] == ' ')
							k++;
						if(answer[k] == '(')
						{
							k++;
							p = 0;
							if(answer[k] == ' ') corect = 0;
							while(corect == 1 && (('0' <= answer[k] && answer[k] <= '9') || answer[k] == '-'))
							{
								//daca p = 0 nu am terminat de format an_start
								if(answer[k] != '-' && p == 0) 
									an_start = an_start * 10 + answer[k] - '0';
								//p = 1 deci acum formam an_end
								else if(answer[k] != '-' && p == 1)
									an_end = an_end * 10 + answer[k] - '0';
								//daca p este deja 1 si totusi am mai intalnit un '-' atunci semnalam eroarea
								else if(p == 1 && answer[k] == '-')
									corect = 0;
								//dupa ce intalnim caracterul '-' trecem la formarea lui an_end
								else p = 1;
								k++;
							}
							//setam p la k care acum este pe paranteza finala pentru a respecta sablonul celorlalte if-uri
							p = k;
							if(answer[p] != ')') 
								corect = 0;
						}
						else corect = 0;
					}
					else if(strncmp(answer + k, "rating:", 7) == 0)
					{
						k += 7;
						while(answer[k] == ' ')
							k++;
						if(answer[k] == '(')
						{
							k++;
							//daca nu exista un numar de la 1 la 5 intre paranteze atunci semnalam eroare
							if('1' > answer[k] || '5' < answer[k]) 
								corect = 0;
							else rating_min = answer[k] - '0';
							k++;
							p = k;
							if(answer[p] != ')') 
								corect = 0;
						}
						else corect = 0;
					}
					//daca nu am detectat niciunul dintre filtre dar nu am ajuns nici la final inseamna ca comanda este eronata
					else if(answer[k] != '\n')
						corect = 0;
					if(corect == 1) 
					{
						//trecem la urmatorul caracter si ignoram eventualele spatii puse la intamplare pentru a ajunge la urmatorul filtru sau la finalul comenzii
						k = p + 1;
						while(answer[k] == ' ')
							k++;
					}
					else corect = 0;
					if(answer[k] == '\n')
						break;
				}
				//comanda este eronata deci trimitem mesajul corespunzator
				if(corect == 0)
				{
					strcpy(answer, "Exemplu comanda: \"cautare titlu: (The Two Towers) gen: (fiction) subgen: (adventure, fantasy) autor: (J.R.R Tolkien) an: (1950-1960) rating: (4)\" [Maxim 3 exemple de subgenuri] [Rating reprezinta rating-ul minim]");
					strcat(answer, EOM);
				}
				else
				{
					strcpy(sql, "select b.isbn, titlu, autor, avg(rating) from books b join ratings r on b.isbn = r.isbn");
					p = 0; //p retine daca au mai fost date elemente clauzei where pentru a determina daca adaugam and sau where
					if(strlen(titlu) > 0)
					{
						strcat(sql, " where titlu = '");
						strcat(sql, titlu);
						strcat(sql, "'");
						p = 1;
					}
					if(strlen(autor) > 0)
					{
						if(p == 1) strcat(sql, " and ");
						else strcat(sql, " where ");
						strcat(sql, "autor = '");
						strcat(sql, autor);
						strcat(sql, "'");
						p = 1;
					}
					if(strlen(genre) > 0)
					{
						if(p == 1) strcat(sql, " and ");
						else strcat(sql, " where ");
						strcat(sql, "genuri = '");
						strcat(sql, genre);
						strcat(sql, "'");
						p = 1;
					}
					if(strlen(subgenres) > 0)
					{
						if(p == 1) strcat(sql, " and ");
						else strcat(sql, " where ");
						//sortam alfabetic subgenurile pentru a respecta sablonul database-ului
						if(index > 0)
						{
							if(strcmp(subgenres[0], subgenres[1]) > 0)
							{
								strcpy(aux, subgenres[0]);
								strcpy(subgenres[0], subgenres[1]);
								strcpy(subgenres[1], aux);
							}
							if(index > 1)
							{
								if(strcmp(subgenres[1], subgenres[2]) > 0)
								{
									strcpy(aux, subgenres[1]);
									strcpy(subgenres[1], subgenres[2]);
									strcpy(subgenres[2], aux);
								}
								if(strcmp(subgenres[0], subgenres[1]) > 0)
								{
									strcpy(aux, subgenres[0]);
									strcpy(subgenres[0], subgenres[1]);
									strcpy(subgenres[1], aux);
								}
							}
						}
						strcat(sql, "subgenuri like '%");
						strcat(sql, subgenres[0]);
						if(index > 0)
						{
							strcat(sql, "%, ");
							strcat(sql, subgenres[1]);
							if(index > 1)
							{
								strcat(sql, ", ");
								strcat(sql, subgenres[2]);
							}
							else strcat(sql, "%");
						}
						else strcat(sql, "%");
						strcat(sql, "'");
						p = 1;
					}
					if(an_start != -1)
					{
						if(p == 1) strcat(sql, " and ");
						else strcat(sql, " where ");
						strcat(sql, "an between ");
						sprintf(aux, "%d", an_start);
						strcat(sql, aux);
						strcat(sql, " and ");
						sprintf(aux, "%d", an_end);
						strcat(sql, aux);
						p = 1;
					}
					strcat(sql, " group by b.isbn");
					if(rating_min != -1)
					{
						strcat(sql, " having avg(rating) > ");
						sprintf(aux, "%d", rating_min);
						strcat(sql, aux);
					}
					strcat(sql, " order by titlu asc;");
					sqlite3_stmt *stmt;
					exit_code = sqlite3_prepare_v2(DB, sql, -1, &stmt, NULL);
					if(exit_code != SQLITE_OK)
					{
						perror("Error on select from books\n");
						strcpy(answer, "Eroare selectare books");
						strcat(answer, EOM);
						break;
					}
					if((exit_code = sqlite3_step(stmt)) == SQLITE_ROW)
						sprintf(answer, "%d | %s | %s | %.2f", sqlite3_column_int(stmt, 0), sqlite3_column_text(stmt, 1), sqlite3_column_text(stmt, 2), sqlite3_column_double(stmt, 3));
					else sprintf(answer, "No results.");
					sendRes(fd, answer);
					while((exit_code = sqlite3_step(stmt)) == SQLITE_ROW)
					{
						sprintf(answer, "\n%d | %s | %s | %.2f", sqlite3_column_int(stmt, 0), sqlite3_column_text(stmt, 1), sqlite3_column_text(stmt, 2), sqlite3_column_double(stmt, 3));
						sendRes(fd, answer);
					}
					strcpy(answer, EOM);
					if(exit_code != SQLITE_DONE)
						perror("Database sqlite3_step error");
					sqlite3_finalize(stmt);
				}
				break;
		//info carte
		case 5: if(logged[fd] == 0)
					strcpy(answer, "Pentru a executa comanda 'info carte' trebuie sa fiti conectat.");
				else
				{
					corect = 1;
					k = 10;
					if(answer[k] != ' ')
						corect = 0;
					k++;
					p = 0;
					//salvam in p isbn-ul dat
					while(corect == 1 && ('0' <= answer[k] && answer[k] <= '9'))
					{
						p = p * 10 + answer[k] - '0';
						k++;
					}
					if(answer[k] != '\n')
						corect = 0;
					if(corect == 0)
					{
						strcpy(answer, "Comanda este de forma \"info carte ISBN\" unde ISBN este prima coloana din rezultatele cautare");
					}
					else
					{
						//verificam daca ISBN-ul dat exista
						sprintf(sql, "select isbn from books where isbn = %d;", p);
						result[0] = 0;
						exit_code = sqlite3_exec(DB, sql, verify_existence, result, NULL);
						if(exit_code != SQLITE_OK)
						{
							perror("Error on select from books\n");
							strcpy(answer, "Eroare selectare books");
							break;
						}
						if(result[0] == 0)
						{
							strcpy(answer, "ISBN-ul dat nu exista");
							break;
						}
						//formam comanda sql si trimitem raspunsul
						sprintf(sql, "select b.isbn, titlu, autor, genuri, subgenuri, an, avg(rating), count(rating) from books b join ratings r on b.isbn = r.isbn where b.isbn = %d;", p);
						sqlite3_stmt *stmt;
						//-1 inseamna ca statementul va fi evaluat pana la NULL
						//NULL pentru a nu retine intr-un string mesajul de eroare
						exit_code = sqlite3_prepare_v2(DB, sql, -1, &stmt, NULL);
						if(exit_code != SQLITE_OK)
						{
							perror("Error on select from books\n");
							strcpy(answer, "Eroare selectare books");
							strcat(answer, EOM);
							break;
						}
						if((exit_code = sqlite3_step(stmt)) == SQLITE_ROW)
							sprintf(answer, "ISBN: %d\nTitlu: %s\nAutor: %s\nGen: %s Subgenuri: %s\nAn: %d\nRating: %.2f\nA primit nota de la %d utilizatori", sqlite3_column_int(stmt, 0), sqlite3_column_text(stmt, 1), sqlite3_column_text(stmt, 2), sqlite3_column_text(stmt, 3), sqlite3_column_text(stmt, 4), sqlite3_column_int(stmt, 5), sqlite3_column_double(stmt, 6), sqlite3_column_int(stmt, 7));
						else sprintf(answer, "No results.");
						//nu exista mai multe randuri la rezultate dar vom continua executia pentru ca exit_code sa devina SQLITE_DONE
						exit_code = sqlite3_step(stmt);
						if(exit_code != SQLITE_DONE)
							perror("Database sqlite3_step error");
						sqlite3_finalize(stmt);
						//verificam daca cartea nu a fost adaugata deja la interesele utilizatorului
						sprintf(sql, "select isbn, id_user from interests where isbn = %d and id_user = %d and type = 1;", p, logged[fd]);
						result[0] = 0;
						exit_code = sqlite3_exec(DB, sql, verify_existence, result, NULL);
						if(exit_code != SQLITE_OK)
						{
							perror("Error on select from interests\n");
							strcpy(answer, "Eroare info carte database");
							break;
						}
						
						//daca nu a fost adaugata o adaugam
						if(result[0] == 0)
						{
							result[0] = 0;
							//verificam ca tabelul sa nu fie gol
							strcpy(sql, "select count(*) from interests;");
							exit_code = sqlite3_exec(DB, sql, get_column0_number, result, NULL);
							if(exit_code != SQLITE_OK)
							{
								perror("Error on select from interests\n");
								strcpy(answer, "Eroare info carte database");
								break;
							}
							//daca tabelul nu e gol
							if(result[0] != 0)
							{
								//luam id-ul maxim din interests pentru a-l afla pe urmatorul
								strcpy(sql, "select max(id) from interests;");
								exit_code = sqlite3_exec(DB, sql, get_column0_number, result, NULL);
								if(exit_code != SQLITE_OK)
								{
									perror("Error on select from interests\n");
									strcpy(answer, "Eroare info carte database");
									break;
								}
							}
							//altfel result[0] = 0 deci id va fi 1 dupa incrementare
							result[0]++;
							
							//adaugam cartea la interesele utilizatorului
							sprintf(sql, "insert into interests values(%d, %d, %d, 1);", result[0], logged[fd], p);
							exit_code = sqlite3_exec(DB, sql, NULL, NULL, NULL);
							if(exit_code != SQLITE_OK)
							{
								perror("Error on insert into interests\n");
								strcpy(answer, "Eroare info carte database");
								break;
							}
						}
					}
				}
				break;
		//rate
		case 6: if(logged[fd] == 0)
					strcpy(answer, "Pentru a executa comanda 'rate' trebuie sa fiti conectat.");
				else
				{
					corect = 1;
					k = 4;
					if(answer[k] != ' ')
						corect = 0;
					k++;
					int isbn, scor;
					isbn = scor = 0;
					while(corect == 1 && ('0' <= answer[k] && answer[k] <= '9'))
					{
						isbn = isbn * 10 + answer[k] - '0';
						k++;
					}
					if(answer[k] != ' ')
						corect = 0;
					k++;
					if('1' > answer[k] || answer[k] > '5')
						corect = 0;
					else 
					{
						scor = answer[k] - '0';
						k++;
					}
					if(answer[k] != '\n')
						corect = 0;
					if(corect == 0)
					{
						strcpy(answer, "Comanda este de forma \"rate isbn rating\" unde isbn este prima coloana la rezultatele comenzii cautare si rating trebuie sa fie intre 1 si 5.");
					}
					else
					{
						//verificam daca ISBN-ul dat exista
						sprintf(sql, "select isbn from books where isbn = %d;", isbn);
						result[0] = 0;
						exit_code = sqlite3_exec(DB, sql, verify_existence, result, NULL);
						if(exit_code != SQLITE_OK)
						{
							perror("Error on select from books\n");
							strcpy(answer, "Eroare selectare books");
							break;
						}
						if(result[0] == 0)
						{
							strcpy(answer, "ISBN-ul dat nu exista");
							break;
						}
						//verificam daca utilizatorul nu a dat deja scor la aceasta carte
						result[0] = 0;
						sprintf(sql, "select isbn, id_user from ratings where isbn = %d and id_user = %d;", isbn, logged[fd]);
						exit_code = sqlite3_exec(DB, sql, verify_existence, result, NULL);
						if(exit_code != SQLITE_OK)
						{
							perror("Error on select from ratings\n");
							strcpy(answer, "Eroare selectare ratings");
							break;
						}
						//daca nu a dat rating adaugam o intrare noua in ratings
						if(result[0] == 0)
						{
							//verificam daca ratings nu e gol
							strcpy(sql, "select count(*) from ratings;");
							exit_code = sqlite3_exec(DB, sql, get_column0_number, result, NULL);
							if(exit_code != SQLITE_OK)
							{
								perror("Error on select from ratings\n");
								strcpy(answer, "Eroare rate database");
								break;
							}
							//daca nu e gol
							if(result[0] != 0)
							{
								//luam id-ul maxim pentru a-l afla pe urmatorul
								strcpy(sql, "select max(id) from ratings;");
								exit_code = sqlite3_exec(DB, sql, get_column0_number, result, NULL);
								if(exit_code != SQLITE_OK)
								{
									perror("Error on select from ratings\n");
									strcpy(answer, "Eroare rate database");
									break;
								}
							}
							result[0]++;
							//formam comanda si adaugam rating-ul in database
							sprintf(sql, "insert into ratings values(%d, %d, %d, %d);", result[0], logged[fd], isbn, scor);
							exit_code = sqlite3_exec(DB, sql, NULL, NULL, NULL);
							if(exit_code != SQLITE_OK)
							{
								perror("Error on insert into ratings\n");
								strcpy(answer, "Eroare rate database");
								break;
							}
							strcpy(answer, "Rating adaugat cu succes");
						}
						//altfel modificam informatiile existente
						else
						{
							sprintf(sql, "update ratings set rating = %d where isbn = %d and id_user = %d;", scor, isbn, logged[fd]);
							exit_code = sqlite3_exec(DB, sql, NULL, NULL, NULL);
							if(exit_code != SQLITE_OK)
							{
								perror("Error on update ratings\n");
								strcpy(answer, "Eroare rate database");
								break;
							}
							strcpy(answer, "Rating actualizat cu succes");
						}
					}
				}
				break;
		//descarcare
		case 7: if(logged[fd] == 0)
				{
					strcpy(answer, "Pentru a executa comanda 'descarcare' trebuie sa fiti conectat.");
					strcat(answer, EOM);
				}
				else
				{
					corect = 1;
					k = 10;
					if(answer[k] != ' ')
						corect = 0;
					k++;
					int isbn = 0;
					while(corect == 1 && ('0' <= answer[k] && answer[k] <= '9'))
					{
						isbn = isbn * 10 + answer[k] - '0';
						k++;
					}
					if(answer[k] != '\n')
						corect = 0;
					if(corect == 0)
					{
						strcpy(answer, "Comanda este de forma \"descarcare isbn\" unde isbn este prima coloana din rezultatele unei cautari.");
						strcat(answer, EOM);
					}
					else
					{
						//verificam daca ISBN-ul dat exista
						sprintf(sql, "select isbn from books where isbn = %d;", isbn);
						result[0] = 0;
						exit_code = sqlite3_exec(DB, sql, verify_existence, result, NULL);
						if(exit_code != SQLITE_OK)
						{
							perror("Error on select from books\n");
							strcpy(answer, "Eroare selectare books");
							strcat(answer, EOM);
							break;
						}
						if(result[0] == 0)
						{
							strcpy(answer, "ISBN-ul dat nu exista");
							strcat(answer, EOM);
							break;
						}
						
						sprintf(sql, "select titlu from books where isbn = %d;", isbn);
						exit_code = sqlite3_exec(DB, sql, get_col0_string, char_result, NULL);
						if(exit_code != SQLITE_OK)
						{
							perror("Error on select from books\n");
							strcpy(answer, "Eroare selectare books");
							strcat(answer, EOM);
							break;
						}
						//cream calea catre carte
						strcat(char_result, ".txt");
						sprintf(path, "Books/%s", char_result);
						if((book_fd = open(path, O_RDONLY)) == -1)
						{
							perror("[server] Error on open()");
							strcpy(answer, "Eroare la citirea cartii");
							strcat(answer, EOM);
							break;
						}
						strcpy(answer, BOOK_DOWNLOAD);
						strcat(answer, " ");
						strcat(answer, char_result);
						sendRes(fd, answer);
						//apel blocant pana cand clientul confirma
						read(fd, number, 1);
						while((bytes_read = read(book_fd, answer, ANSWER_SIZE)) != 0)
						{
							if(bytes_read < 0)
							{
								perror("[server] Error on read from book");
								strcpy(answer, BOOK_DOWNLOAD);
								sendRes(fd, answer);
								//apel blocant pana cand clientul confirma
								read(fd, number, 1);
								strcpy(answer, "Eroare la citirea cartii");
								strcat(answer, EOM);
								break;
							}
							sendRes(fd, answer);
							//apel blocant pana cand clientul confirma
							read(fd, number, 1);
						}
						strcpy(answer, BOOK_DOWNLOAD);
						sendRes(fd, answer);
						//apel blocant pana cand clientul confirma
						read(fd, number, 1);
						strcpy(answer, "Carte transmisa cu succes");
						strcat(answer, EOM);
						
						//verificam daca cartea nu a fost adaugata deja la interesele utilizatorului
						sprintf(sql, "select isbn, id_user from interests where isbn = %d and id_user = %d and type = 2;", isbn, logged[fd]);
						result[0] = 0;
						exit_code = sqlite3_exec(DB, sql, verify_existence, result, NULL);
						if(exit_code != SQLITE_OK)
						{
							perror("Error on select from interests\n");
							strcpy(answer, "Eroare info carte database");
							break;
						}
						
						//daca nu a fost adaugata o adaugam
						if(result[0] == 0)
						{
							result[0] = 0;
							//verificam ca tabelul sa nu fie gol
							strcpy(sql, "select count(*) from interests;");
							exit_code = sqlite3_exec(DB, sql, get_column0_number, result, NULL);
							if(exit_code != SQLITE_OK)
							{
								perror("Error on select from interests\n");
								strcpy(answer, "Eroare info carte database");
								break;
							}
							//daca tabelul nu e gol
							if(result[0] != 0)
							{
								//luam id-ul maxim din interests pentru a-l afla pe urmatorul
								strcpy(sql, "select max(id) from interests;");
								exit_code = sqlite3_exec(DB, sql, get_column0_number, result, NULL);
								if(exit_code != SQLITE_OK)
								{
									perror("Error on select from interests\n");
									strcpy(answer, "Eroare info carte database");
									break;
								}
							}
							//altfel result[0] = 0 deci id va fi 1 dupa incrementare
							result[0]++;
							
							//adaugam cartea la interesele utilizatorului
							sprintf(sql, "insert into interests values(%d, %d, %d, 2);", result[0], logged[fd], isbn);
							exit_code = sqlite3_exec(DB, sql, NULL, NULL, NULL);
							if(exit_code != SQLITE_OK)
							{
								perror("Error on insert into interests\n");
								strcpy(answer, "Eroare info carte database");
								break;
							}
						}
					}
				}
				break;
		//recomandari
		case 8: if(logged[fd] == 0)
					strcpy(answer, "Pentru a executa comanda 'recomandari' trebuie sa fiti conectat.");
				else
				{
					/*
					1. Select all from user
					|__ Get favorite author (select max(avg(rating)) group by author)
					|__ Get favorite genre (ceva similar)
					|__ Get favorite subgenre (...)
					exclude results and repeat 2 more times for a complete top 3
					Search for users with similar taste until you find 2 books to recommend (backwards search - who gave high ratings to books from x author in x genre and x subgenre. single out filters if no results and exclude current user obviously)
					Search for other books with the same filters. Remove filters gradually if no results
					When no genre/author/subgenre stands out recommend highest rated books (done)
					*/
					char autor_fav[3][30], gen_fav[60], subgen_fav[3][60];
					int nr_autori = 0, nr_subgenuri = 0, no_preference = 0;
					sprintf(sql, "select autor, avg(rating) as avg_rating from ratings r join books b on r.isbn = b.isbn where id_user = %d group by autor order by avg(rating) desc LIMIT 3;", logged[fd]);
					result[0] = 0;
					exit_code = sqlite3_exec(DB, sql, verify_existence, result, NULL);
					if(exit_code != SQLITE_OK)
					{
						perror("Error on select for recommendations\n");
						strcpy(answer, "Eroare selectare recomandari");
						break;
					}
					//daca nu avem destule ratings
					if(result[0] == 0)
					{
						sprintf(sql, "select autor, sum(type) as interest_score from interests i join books b on i.isbn = b.isbn where id_user = %d group by autor order by sum(type) desc LIMIT 3;", logged[fd]);
						exit_code = sqlite3_exec(DB, sql, verify_existence, result, NULL);
						if(exit_code != SQLITE_OK)
						{
							perror("Error on select for recommendations\n");
							strcpy(answer, "Eroare selectare recomandari");
							break;
						}
						//daca utilizatorul nu are nicio preferinta salvata recomandam cartile cu cel mai mare rating, iar daca exista rating-uri egale se iau cele cu mai multe rating-uri primite
						if(result[0] == 0)
						{
							no_preference = 1;
							sprintf(sql, "select b.isbn, titlu, autor, avg(rating) from books b join ratings r on b.isbn = r.isbn group by b.isbn order by avg(rating) desc, count(rating) desc LIMIT 5;");
							sqlite3_stmt *stmt;
							exit_code = sqlite3_prepare_v2(DB, sql, -1, &stmt, NULL);
							if(exit_code != SQLITE_OK)
							{
								perror("Error on select for recommendations\n");
								strcpy(answer, "Eroare selectare recomandari");
								break;
							}
							if((exit_code = sqlite3_step(stmt)) == SQLITE_ROW)
								sprintf(answer, "%d | %s | %s", sqlite3_column_int(stmt, 0), sqlite3_column_text(stmt, 1), sqlite3_column_text(stmt, 2));
							while((exit_code = sqlite3_step(stmt)) == SQLITE_ROW)
							{
								sprintf(aux, "\n%d | %s | %s", sqlite3_column_int(stmt, 0), sqlite3_column_text(stmt, 1), sqlite3_column_text(stmt, 2));
								strcat(answer, aux);
							}
							if(exit_code != SQLITE_DONE)
								perror("Database sqlite3_step error");
							sqlite3_finalize(stmt);
						}
						else
						{
							sqlite3_stmt *stmt;
							exit_code = sqlite3_prepare_v2(DB, sql, -1, &stmt, NULL);
							if(exit_code != SQLITE_OK)
							{
								perror("Error on select for recommendations\n");
								strcpy(answer, "Eroare selectare recomandari");
								break;
							}
							//salvam autorii favoriti (maxim 3)
							while((exit_code = sqlite3_step(stmt)) == SQLITE_ROW)
							{
								sprintf(autor_fav[nr_autori], "%s", sqlite3_column_text(stmt, 0));
								nr_autori++;
							}
							if(exit_code != SQLITE_DONE)
								perror("Database sqlite3_step error");
							sqlite3_finalize(stmt);
							
							//cautam genul favorit
							sprintf(sql, "select genuri, sum(type) as interest_score from interests i join books b on i.isbn = b.isbn where id_user = %d group by genuri order by sum(type) desc LIMIT 1;", logged[fd]);
							exit_code = sqlite3_prepare_v2(DB, sql, -1, &stmt, NULL);
							if(exit_code != SQLITE_OK)
							{
								perror("Error on select for recommendations\n");
								strcpy(answer, "Eroare selectare recomandari");
								break;
							}
							//salvam genul favorit
							if((exit_code = sqlite3_step(stmt)) == SQLITE_ROW)
								sprintf(gen_fav, "%s", sqlite3_column_text(stmt, 0));
							exit_code = sqlite3_step(stmt);
							if(exit_code != SQLITE_DONE)
								perror("Database sqlite3_step error");
							sqlite3_finalize(stmt);
							
							//cautam subgenurile favorite
							sprintf(sql, "select subgenuri, sum(type) as interest_score from interests i join books b on i.isbn = b.isbn where id_user = %d group by subgenuri order by sum(type) desc LIMIT 3;", logged[fd]);
							exit_code = sqlite3_prepare_v2(DB, sql, -1, &stmt, NULL);
							if(exit_code != SQLITE_OK)
							{
								perror("Error on select for recommendations\n");
								strcpy(answer, "Eroare selectare recomandari");
								break;
							}
							//salvam subgenurile favorite (maxim 3)
							while((exit_code = sqlite3_step(stmt)) == SQLITE_ROW)
							{
								if(nr_subgenuri < 3)
								{
									sprintf(aux, "%s", sqlite3_column_text(stmt, 0));
									index = k = 0;
									while(index < strlen(aux))
									{
										if(aux[index] == ',')
										{
											//adaugam null la finalul subgenului salvat
											subgen_fav[nr_subgenuri][k] = '\0';
											nr_subgenuri++;
											index += 2;
											k = 0;
										}
										else 
										{
											subgen_fav[nr_subgenuri][k] = aux[index];
											k++;
											index++;
										}
									}
									//adaugam null la finalul ultimului subgen
									subgen_fav[nr_subgenuri][k] = '\0';
									nr_subgenuri++;
								}
							}
							if(exit_code != SQLITE_DONE)
								perror("Database sqlite3_step error");
							sqlite3_finalize(stmt);
						}
					} //caz in care nu sunt destule ratings de la user
					//avem destule ratings
					else
					{
						sqlite3_stmt *stmt;
						exit_code = sqlite3_prepare_v2(DB, sql, -1, &stmt, NULL);
						if(exit_code != SQLITE_OK)
						{
							perror("Error on select for recommendations\n");
							strcpy(answer, "Eroare selectare recomandari");
							break;
						}
						//salvam autorii favoriti (maxim 3)
						while((exit_code = sqlite3_step(stmt)) == SQLITE_ROW)
						{
							sprintf(autor_fav[nr_autori], "%s", sqlite3_column_text(stmt, 0));
							nr_autori++;
						}
						if(exit_code != SQLITE_DONE)
							perror("Database sqlite3_step error");
						sqlite3_finalize(stmt);
						
						//cautam genul favorit
						sprintf(sql, "select genuri, avg(rating) from ratings r join books b on r.isbn = b.isbn where id_user = %d group by genuri order by avg(rating) desc LIMIT 1;", logged[fd]);
						exit_code = sqlite3_prepare_v2(DB, sql, -1, &stmt, NULL);
						if(exit_code != SQLITE_OK)
						{
							perror("Error on select for recommendations\n");
							strcpy(answer, "Eroare selectare recomandari");
							break;
						}
						//salvam genul favorit
						if((exit_code = sqlite3_step(stmt)) == SQLITE_ROW)
							sprintf(gen_fav, "%s", sqlite3_column_text(stmt, 0));
						exit_code = sqlite3_step(stmt);
						if(exit_code != SQLITE_DONE)
							perror("Database sqlite3_step error");
						sqlite3_finalize(stmt);
						
						//cautam subgenurile favorite
						sprintf(sql, "select subgenuri, avg(rating) from ratings r join books b on r.isbn = b.isbn where id_user = %d group by subgenuri order by avg(rating) desc LIMIT 3;", logged[fd]);
						exit_code = sqlite3_prepare_v2(DB, sql, -1, &stmt, NULL);
						if(exit_code != SQLITE_OK)
						{
							perror("Error on select for recommendations\n");
							strcpy(answer, "Eroare selectare recomandari");
							break;
						}
						//salvam subgenurile favorite (maxim 3)
						while((exit_code = sqlite3_step(stmt)) == SQLITE_ROW)
						{
							if(nr_subgenuri < 3)
							{
								sprintf(aux, "%s", sqlite3_column_text(stmt, 0));
								index = k = 0;
								while(index < strlen(aux))
								{
									if(aux[index] == ',')
									{
										//adaugam null la finalul subgenului salvat
										subgen_fav[nr_subgenuri][k] = '\0';
										nr_subgenuri++;
										index += 2;
										k = 0;
									}
									else 
									{
										subgen_fav[nr_subgenuri][k] = aux[index];
										k++;
										index++;
									}
								}
								//adaugam null la finalul ultimului subgen
								subgen_fav[nr_subgenuri][k] = '\0';
								nr_subgenuri++;
							}
						}
						if(exit_code != SQLITE_DONE)
							perror("Database sqlite3_step error");
						sqlite3_finalize(stmt);
					} //caz in care avem destule ratings
					//cautare recomandari
					if(no_preference == 0)
					{
						int i1, i2, already_added;
						i1 = i2 = nr_recoms = already_added = 0;
						sqlite3_stmt *stmt;
						while(i1 < nr_autori && nr_recoms < 5)
						{
							sprintf(sql, "select isbn from (select b.isbn, avg(rating) from books b join ratings r on b.isbn = r.isbn where autor = '%s' and genuri = '%s' and subgenuri like '%%%s%%' group by b.isbn order by avg(rating) desc) EXCEPT select b.isbn from books b join ratings r on b.isbn = r.isbn where r.id_user = %d EXCEPT select b.isbn from books b join interests i on b.isbn = i.isbn where i.id_user = %d and type = 2 LIMIT 5;", autor_fav[i1], gen_fav, subgen_fav[i2], logged[fd], logged[fd]);
							exit_code = sqlite3_prepare_v2(DB, sql, -1, &stmt, NULL);
							if(exit_code != SQLITE_OK)
							{
								perror("Error on select for recommendations\n");
								strcpy(answer, "Eroare selectare recomandari");
								break;
							}
							while((exit_code = sqlite3_step(stmt)) == SQLITE_ROW)
							{
								if(nr_recoms < 5)
								{
									already_added = 0;
									for(int i = 0; already_added == 0 && i < nr_recoms; i++)
										if(recoms[i] == sqlite3_column_int(stmt, 0))
											already_added = 1;
									if(already_added == 0)
									{
										recoms[nr_recoms] = sqlite3_column_int(stmt, 0);
										nr_recoms++;
									}
								}
							}
							if(exit_code != SQLITE_DONE)
								perror("Database sqlite3_step error");
							sqlite3_finalize(stmt);
							i2++;
							if(i2 == nr_subgenuri)
							{
								i2 = 0;
								i1++;
							}
						}
						//daca nu am gasit inca 5 scoatem filtrul autor
						if(nr_recoms < 5)
						{
							i2 = 0;
							while(i2 < nr_subgenuri && nr_recoms < 5)
							{
								sprintf(sql, "select isbn from (select b.isbn, avg(rating) from books b join ratings r on b.isbn = r.isbn where genuri = '%s' and subgenuri like '%%%s%%' group by b.isbn order by avg(rating) desc) EXCEPT select b.isbn from books b join ratings r on b.isbn = r.isbn where r.id_user = %d EXCEPT select b.isbn from books b join interests i on b.isbn = i.isbn where i.id_user = %d and type = 2 LIMIT 5;", gen_fav, subgen_fav[i2], logged[fd], logged[fd]);
								exit_code = sqlite3_prepare_v2(DB, sql, -1, &stmt, NULL);
								if(exit_code != SQLITE_OK)
								{
									perror("Error on select for recommendations\n");
									strcpy(answer, "Eroare selectare recomandari");
									break;
								}
								while((exit_code = sqlite3_step(stmt)) == SQLITE_ROW)
								{
									if(nr_recoms < 5)
									{
										already_added = 0;
										for(int i = 0; already_added == 0 && i < nr_recoms; i++)
											if(recoms[i] == sqlite3_column_int(stmt, 0))
												already_added = 1;
										if(already_added == 0)
										{
											recoms[nr_recoms] = sqlite3_column_int(stmt, 0);
											nr_recoms++;
										}
									}
								}
								if(exit_code != SQLITE_DONE)
									perror("Database sqlite3_step error");
								sqlite3_finalize(stmt);
								i2++;
							}
						}
						//daca tot nu am gasit 5 scoatem filtrul genuri
						if(nr_recoms < 5)
						{
							i2 = 0;
							while(i2 < nr_subgenuri && nr_recoms < 5)
							{
								sprintf(sql, "select isbn from (select b.isbn, avg(rating) from books b join ratings r on b.isbn = r.isbn where subgenuri like '%%%s%%' group by b.isbn order by avg(rating) desc) EXCEPT select b.isbn from books b join ratings r on b.isbn = r.isbn where r.id_user = %d EXCEPT select b.isbn from books b join interests i on b.isbn = i.isbn where i.id_user = %d and type = 2 LIMIT 5;", subgen_fav[i2], logged[fd], logged[fd]);
								exit_code = sqlite3_prepare_v2(DB, sql, -1, &stmt, NULL);
								if(exit_code != SQLITE_OK)
								{
									perror("Error on select for recommendations\n");
									strcpy(answer, "Eroare selectare recomandari");
									break;
								}
								while((exit_code = sqlite3_step(stmt)) == SQLITE_ROW)
								{
									if(nr_recoms < 5)
									{
										already_added = 0;
										for(int i = 0; already_added == 0 && i < nr_recoms; i++)
											if(recoms[i] == sqlite3_column_int(stmt, 0))
												already_added = 1;
										if(already_added == 0)
										{
											recoms[nr_recoms] = sqlite3_column_int(stmt, 0);
											nr_recoms++;
										}
									}
								}
								if(exit_code != SQLITE_DONE)
									perror("Database sqlite3_step error");
								sqlite3_finalize(stmt);
								i2++;
							}
						}
						int found_similar_taste_recom = 0;							
						sprintf(sql, "select id_user, avg(rating) from ratings r join books b on r.isbn = b.isbn where autor = '%s' and genuri = '%s' and subgenuri like '%%%s%%' and id_user != %d group by id_user having avg(rating) > 4.5 LIMIT 50;", autor_fav[i1], gen_fav, subgen_fav[i2], logged[fd]);
						exit_code = sqlite3_prepare_v2(DB, sql, -1, &stmt, NULL);
						if(exit_code != SQLITE_OK)
						{
							perror("Error on select for recommendations\n");
							strcpy(answer, "Eroare selectare recomandari");
							break;
						}
						while((exit_code = sqlite3_step(stmt)) == SQLITE_ROW)
						{
							sprintf(sql, "select isbn from (select b.isbn from books b join ratings r on b.isbn = r.isbn where id_user = %d order by rating desc) EXCEPT select b.isbn from books b join ratings r on b.isbn = r.isbn where r.id_user = %d EXCEPT select b.isbn from books b join interests i on b.isbn = i.isbn where i.id_user = %d and type = 2 LIMIT 1;", sqlite3_column_int(stmt, 0), logged[fd], logged[fd]);
							sqlite3_stmt *stmt1;
							exit_code = sqlite3_prepare_v2(DB, sql, -1, &stmt1, NULL);
							if(exit_code != SQLITE_OK)
							{
								perror("Error on select for recommendations\n");
								strcpy(answer, "Eroare selectare recomandari");
								break;
							}
							while((exit_code = sqlite3_step(stmt1)) == SQLITE_ROW)
							{
								if(found_similar_taste_recom < 3)
								{
									already_added = 0;
									for(int i = 0; already_added == 0 && i < nr_recoms; i++)
										if(recoms[i] == sqlite3_column_int(stmt1, 0))
											already_added = 1;
									if(already_added == 0)
									{
										if(nr_recoms < 5)
										{
											recoms[nr_recoms] = sqlite3_column_int(stmt1, 0);
											nr_recoms++;
										}
										else recoms[4] = sqlite3_column_int(stmt1, 0);
										found_similar_taste_recom++;
									}
								}
							}
							if(exit_code != SQLITE_DONE)
								perror("Database sqlite3_step error");
							sqlite3_finalize(stmt1);
						}
						if(exit_code != SQLITE_DONE)
							perror("Database sqlite3_step error");
						sqlite3_finalize(stmt);
						
						for(int i = 0; i < nr_recoms; i++)
						{
							sprintf(sql, "select isbn, titlu, autor from books where isbn = %d;", recoms[i]);
							exit_code = sqlite3_prepare_v2(DB, sql, -1, &stmt, NULL);
							if(exit_code != SQLITE_OK)
							{
								perror("Error on select for recommendations\n");
								strcpy(answer, "Eroare selectare recomandari");
								break;
							}
							if((exit_code = sqlite3_step(stmt)) == SQLITE_ROW)
							{
								if(i == 0) 
									sprintf(answer, "%d | %s by %s", sqlite3_column_int(stmt, 0), sqlite3_column_text(stmt, 1), sqlite3_column_text(stmt, 2));
								else 
								{
									sprintf(aux, "\n%d | %s by %s", sqlite3_column_int(stmt, 0), sqlite3_column_text(stmt, 1), sqlite3_column_text(stmt, 2));
									strcat(answer, aux);
								}
							}
							exit_code = sqlite3_step(stmt);
							if(exit_code != SQLITE_DONE)
								perror("Database sqlite3_step error");
							sqlite3_finalize(stmt);
						}
					}
				}
				break;
		//log out
		case 9: logged[fd] = 0;
				strcpy(answer, "Utilizator delogat cu succes!");
				break;
		default: strcpy(answer, "Comanda nu exista.");
	}
	if(command != 3 && command != 4 && command != 7)
		strcat(answer, EOM);
	free(result);
	free(char_result);
	sqlite3_close(DB);
}
