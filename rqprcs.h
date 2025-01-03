//checks if a character is a number or a letter
int alfanumeric(char ch);
//processes the command, producing a result which will be sent to the client
void prcsReq(int command, char answer[512], int logged[100], int fd);
