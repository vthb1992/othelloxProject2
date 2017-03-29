#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int size_x, size_y;
char white_positions[400][3];
char black_positions[400][3];
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
		
		}
	}


	fclose(file1);
	fclose(file2);
}

int main(int argc, char **argv)
{
	readFiles(argv[1], argv[2]);

	//printf("%d", maxBoards);

	getch();
	return 0;
}


