#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EMPTY 0
#define BLACK 1
#define WHITE 2

char board[676];

int size_x, size_y;
char white_positions[400][5];
char black_positions[400][5];
int black_size, white_size;

int maxDepth, maxBoards, cornerValue, edgeValue;

char *trimWhiteSpace(char *str)
{
	char *end;

	// Trim leading space
	while (isspace((unsigned char)*str)) {
		str++;
	}
	if (*str == 0) {
		return str;
	}
	// Trim trailing space
	end = str + strlen(str) - 1;
	while (end > str && isspace((unsigned char)*end)) {
		end--;
	}
	*(end + 1) = 0;

	return str;
}

void readFiles(char *initialbrd, char *evalparams) {
	char line[256];
	char *label, *details;
	FILE *file1 = fopen(initialbrd, "r");
	FILE *file2 = fopen(evalparams, "r");
	
	// initialbrd.txt
	while (fgets(line, sizeof(line), file1)) {
		label = strtok(line, ":");
		if (label != NULL) {
			details = trimWhiteSpace(strtok(NULL, ":"));
		}
		if (strcmp(label,"Size") == 0) {
			char *x_temp = strtok(details, ",");
			char *y_temp = strtok(NULL, ",");
			size_x = atoi(x_temp);
			size_y = atoi(y_temp);
		}
		else {
			int count = 0;
			char *pos = trimWhiteSpace(strtok(details, "{}"));
			char *posInv = strtok(pos, ",");
			while (posInv != NULL) {
				if (strcmp(label, "White") == 0) {
					strcpy(white_positions[count],posInv);
				}
				else {
					strcpy(black_positions[count],posInv);
				}
				posInv = strtok(NULL, ",");
				count++;
			}
			if (strcmp(label, "White") == 0) {
				white_size = count;
			}
			else {
				black_size = count;
			}
		}
	}

	//evalparams.txt
	while (fgets(line, sizeof(line), file2)) {
		label = strtok(line, ":");
		if (label != NULL) {
			details = trimWhiteSpace(strtok(NULL, ":"));
		}
		if (strcmp(label, "MaxDepth") == 0) {
			maxDepth = atoi(details);
		}
		else if (strcmp(label, "MaxBoards") == 0){
			maxBoards = atoi(details);
		}
		else if (strcmp(label, "CornerValue") == 0){
			cornerValue = atoi(details);
		}
		else if (strcmp(label, "EdgeValue") == 0){
			edgeValue = atoi(details);
		}
		else {
			//nth yet
		}
	}

	fclose(file1);
	fclose(file2);
}

int translateInputPosToIndex(char *pos) {
	int letter = pos[0] - 96; 
	int num, result;
	if (strlen(pos) == 2) {
		num = pos[1] - '0';
	}
	else {
		num = (pos[1] - '0') * 10 + (pos[2] - '0');
	}

	result = (letter - 1) + (num - 1) * size_x;
	return result;
}

char *translateIndexToOutputPos(int index) {
	int letter = index + 1;
	int num = 1;
	while (letter > size_x) {
		letter = letter - size_x;
		num++;
	}
	char outputLetter = letter + 96;

	char strNum[10];
	sprintf(strNum, "%d", num);

	char string[5];
	string[0] = outputLetter;
	string[1] = strNum[0];
	if (num >= 10) {
		string[2] = strNum[1];
		string[3] = '\0';
	}
	else {
		string[2] = '\0';
	}

	char *result = malloc(5);
	strcpy(result, string);
	return result;
}

void initBoard() {
	for (int a = 0; a < size_x * size_y; a++) {
		board[a] = EMPTY;
	}
	for (int i = 0; i < white_size; i++) {
		board[translateInputPosToIndex(white_positions[i])] = WHITE;

	}
	for (int j = 0; j < black_size; j++) {
		board[translateInputPosToIndex(black_positions[j])] = BLACK;
	}

}
void printBoard() {
	// A-Z on the x-axis(left to right), 1-26 on the y-axis(top to bottom)
	int sizeOfArray = size_x * size_y;
	for (int a = 0; a < sizeOfArray; a++) {
		printf("%d", board[a]);
		if ((a+1) % size_x == 0) {
			printf("\n");
		}
	}
}
int main(int argc, char **argv)
{
	readFiles(argv[1], argv[2]);

	initBoard();
	printBoard();
	


	//printf("%d", maxBoards);
	getch();
	return 0;
}


