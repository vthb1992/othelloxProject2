#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <string.h>
#include <mpi.h>

int slaves;
int myid;

#define EMPTY 0
#define BLACK 1
#define WHITE 2
#define FLIP(x) (3-(x))  
#define SMALLEST_FLOAT -1.0e10
#define LARGEST_FLOAT 1.0e10
#define MASTER_ID slaves

typedef enum { false, true } bool;

const int DIRECTION[8][2] = { { 1, 0 },{ 1, 1 },{ 0, 1 },{ -1, 1 },{ -1, 0 },{ -1, -1 },{ 0, -1 },{ 1, -1 } };

// initialbrd variables
int sizeOfArray;
int size_x, size_y;
char white_positions[350][5];
char black_positions[350][5];
int black_size, white_size;

// evalparams variables
int maxDepth, maxBoards, cornerValue, edgeValue, bestMovesForColor, timeOut;


/* 
Determines the current time
*/
long long wall_clock_time() {
#ifdef LINUX
	struct timespec tp;
	clock_gettime(CLOCK_REALTIME, &tp);
	return (long long)(tp.tv_nsec + (long long)tp.tv_sec * 1000000000ll);
#else
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (long long)(tv.tv_usec * 1000 + (long long)tv.tv_sec * 1000000000ll);
#endif
}

char *trimWhiteSpace(char *str) {
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
		else if (strcmp(label, "Color") == 0) {
			if (strcmp(details, "Black") == 0) {
				bestMovesForColor = 1;
			}
			else {
				bestMovesForColor = 2;
			}
		}
		else if (strcmp(label, "Timeout") == 0) {
			timeOut = atoi(details);
		}
		else {
			//nth yet
		}
	}

	fclose(file1);
	fclose(file2);
	
	//broadcast file infomation to all slaves
	int i;
	MPI_Bcast(&size_x, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&size_y, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&black_size, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&white_size, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	for(i = 0;i<white_size;i++){
		MPI_Bcast(&white_positions[i], 5, MPI_CHAR, MASTER_ID, MPI_COMM_WORLD);
	}
	for(i = 0;i<black_size;i++){
		MPI_Bcast(&black_positions[i], 5, MPI_CHAR, MASTER_ID, MPI_COMM_WORLD);
	}
	MPI_Bcast(&maxDepth, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&maxBoards, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&cornerValue, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&edgeValue, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&bestMovesForColor, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&timeOut, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
}

void slave(){
	int i;
	MPI_Bcast(&size_x, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&size_y, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&black_size, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&white_size, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	for(i = 0;i<white_size;i++){
		MPI_Bcast(&white_positions[i], 5, MPI_CHAR, MASTER_ID, MPI_COMM_WORLD);
	}
	for(i = 0;i<black_size;i++){
		MPI_Bcast(&black_positions[i], 5, MPI_CHAR, MASTER_ID, MPI_COMM_WORLD);
	}
	MPI_Bcast(&maxDepth, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&maxBoards, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&cornerValue, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&edgeValue, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&bestMovesForColor, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&timeOut, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	
	
	
}

void master(char *initialbrd, char *evalparams){
	readFiles(initialbrd, evalparams);
	
}

/*
Using master-slave paradigm

Master process has id = 0
Total number of processes is 1 + number of slaves
*/
int main (int argc, char **argv) {
	int nprocs;
	MPI_Init(&argc,&argv);
	MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
	MPI_Comm_rank(MPI_COMM_WORLD, &myid);
	
	slaves = nprocs - 1;
	
	if(argc != 3){
		if(myid == MASTER_ID){
			fprintf(stderr,"Error, invalid number of arguments!");
			MPI_Finalize();
			exit(1);
		}
	}
	
	if (myid == MASTER_ID){
		master(argv[1],argv[2]);
	}
	else{
		slave();
	}
	
	MPI_Finalize();
	return 0;
}