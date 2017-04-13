#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <string.h>
#include <math.h>
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

void printBoard(char *board) {
	// A-Z on the x-axis(left to right), 1-26 on the y-axis(top to bottom)
	int a;
	for (a = 0; a < sizeOfArray; a++) {
		printf("%d", board[a]);
		if ((a + 1) % size_x == 0) {
			printf("\n");
		}
	}
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
			sizeOfArray = size_x * size_y;
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
	
	//broadcast file infomation to all slaves
	int i;
	MPI_Bcast(&size_x, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&size_y, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&sizeOfArray, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
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

void initBoard(char *board){
	int i;
	for (i = 0; i < sizeOfArray; i++) {
		board[i] = EMPTY;
	}
	for (i = 0; i < white_size; i++) {
		board[translateInputPosToIndex(white_positions[i])] = WHITE;
	}
	for (i = 0; i < black_size; i++) {
		board[translateInputPosToIndex(black_positions[i])] = BLACK;
	}
}

bool isLegalMove(char *board, int index, int legalMoveFor) { //legal move for which player color white/black
	int x = index + 1;
	int y = 1;
	while (x > size_x) {
		x = x - size_x;
		y++;
	}

	if (board[index] != EMPTY) {
		return false;
	}
	int dir;
	for (dir = 0; dir < 8; dir++) { // checking for legal moves in all 8 directions, horizontal, vertial and diagonal
		int dx = DIRECTION[dir][0];
		int dy = DIRECTION[dir][1];
		int tempx = x;
		int tempy = y;
		int numFlipped = 0;

		bool cont = true;
		while (cont) {
			tempx = tempx + dx;
			tempy = tempy + dy;

			if (tempx <= 0 || tempx > size_x || tempy <= 0 || tempy > size_y) {
				cont = false;
				continue;
			}

			int tempIndex = (tempx - 1) + (tempy - 1) * size_x;
			if (board[tempIndex] == EMPTY) {
				cont = false;
			}
			else if(board[tempIndex] == WHITE){
				if (legalMoveFor == BLACK) {
					numFlipped++;
				}
				else {
					if (numFlipped == 0) {
						cont = false;
					}
					else {
						return true;
					}
				}
			}
			else {
				if (legalMoveFor == WHITE) {
					numFlipped++;
				}
				else {
					if (numFlipped == 0) {
						cont = false;
					}
					else {
						return true;
					}
				}
			}
		}
	}
	return false;
}

bool findAllLegalMoves(char *board, int legalMoveFor, int *lm, int *counter) {
	bool result = false;
	int count = 0;
	int indexCounter = 0;
	int i, slave_id;
	int jobType = 1;
	bool resultsRecv [1000];
	bool resultRecv;
	MPI_Status status;
	
	
	if(sizeOfArray > slaves){
		//split work to slaves
		for (slave_id = 0; slave_id < slaves; slave_id++) {
			int startIndex = (int)(sizeOfArray * slave_id / slaves);
			int size = (int)(sizeOfArray * (slave_id + 1) / slaves) - startIndex;
			int arraySend[size];
			for(i = 0; i < size; i++){
				arraySend[i] = startIndex + i;
			}
			MPI_Send(&jobType, 1, MPI_INT, slave_id, 1000, MPI_COMM_WORLD);
			MPI_Send(&board[0], sizeOfArray, MPI_CHAR, slave_id, slave_id, MPI_COMM_WORLD);
			MPI_Send(&legalMoveFor, 1, MPI_INT, slave_id, slave_id, MPI_COMM_WORLD);
			MPI_Send(&size, 1, MPI_INT, slave_id, slave_id, MPI_COMM_WORLD);
			MPI_Send(arraySend, size, MPI_INT, slave_id, slave_id, MPI_COMM_WORLD);
		}
		//receive results from slaves
		for (slave_id = 0; slave_id < slaves; slave_id++) {
			int startIndex = (int)(sizeOfArray * slave_id / slaves);
			int size = (int)(sizeOfArray * (slave_id + 1) / slaves) - startIndex;
			MPI_Recv(&resultsRecv, size, MPI_INT, slave_id, slave_id, MPI_COMM_WORLD, &status);
			for(i = 0;i < size; i++){
				if(resultsRecv[i]){
					result = true;
					lm[count] = indexCounter;
					count++;
				}
				indexCounter++;
			}
		}
	}else{
		//split work to slaves
		for (i = 0; i < sizeOfArray; i++) {
			MPI_Send(&jobType, 1, MPI_INT, i, 1000, MPI_COMM_WORLD);
			MPI_Send(&board[0], sizeOfArray, MPI_CHAR, i, i, MPI_COMM_WORLD);
			MPI_Send(&legalMoveFor, 1, MPI_INT, i, i, MPI_COMM_WORLD);
			MPI_Send(&i, 1, MPI_INT, i, i, MPI_COMM_WORLD);
		}
		//receive results from slaves
		for (i = 0; i < sizeOfArray; i++) {
			MPI_Recv(&resultRecv, 1, MPI_INT, i, i, MPI_COMM_WORLD, &status);
			if(resultRecv){
				result = true;
				lm[count] = i;
				count++;
			}
		}
	}
	printf("---------------%d-----------------", count);
	*counter = count;
	return result;
}

void getMinimaxMoves(char *board, int *bestMoves, int *bmSize) {
	int depth = 0;
	int numOfBoards = 0;
	float alpha = SMALLEST_FLOAT;
	float beta = LARGEST_FLOAT;
	int numOfFlipped;
	int numOfPlayerPieces, numOfOppPieces;
	int turnColor = bestMovesForColor;

	float highestValue = SMALLEST_FLOAT;
	int bestMovesCount = 0;

	char mmBoard[676];

	int legalMoves[350];
	float valuesOfLegalMoves[350];
	int numOfLegalMoves = 0;
	findAllLegalMoves(board, turnColor, legalMoves, &numOfLegalMoves);
	
	
	//break the while loop in slave
	int slave_id;
	int jobType = 0;
	for (slave_id = 0; slave_id < slaves; slave_id++) {
		MPI_Send(&jobType, 1, MPI_INT, slave_id, 1000, MPI_COMM_WORLD);
	}
	
	printf("%d", numOfLegalMoves);
	
	/*
	if (turnColor == BLACK) {
		numOfPlayerPieces = black_size;
		numOfOppPieces = white_size;
	}
	else {
		numOfPlayerPieces = white_size;
		numOfOppPieces = black_size;
	}
	int i;
	for (i = 0; i < numOfLegalMoves; i++) {
		if (numOfBoardsAccessed >= maxBoards) {
			break;
		}
		copyBoardArray(board, mmBoard);
		numOfFlipped = flipPiecesOnBoard(mmBoard, legalMoves[i], turnColor);

		valuesOfLegalMoves[i] = getMin(mmBoard, FLIP(turnColor), numOfPlayerPieces + numOfFlipped + 1, 
			numOfOppPieces - numOfFlipped, depth + 1, alpha, beta, false);

		if (valuesOfLegalMoves[i] > highestValue) {
			highestValue = valuesOfLegalMoves[i];
		}

		printf("%d -> %f \n", legalMoves[i], valuesOfLegalMoves[i]);
	}

	for (i = 0; i < numOfLegalMoves; i++) {
		if (valuesOfLegalMoves[i] == highestValue) {
			bestMoves[bestMovesCount] = legalMoves[i];
			bestMovesCount++;
		}
	}
	*bmSize = bestMovesCount; */
}

void slave(){
	int i;
	MPI_Bcast(&size_x, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&size_y, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&sizeOfArray, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
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
	
	int jobType;
	int arrayRecv [1000];
	char boardRecv [676];
	int legalMoveForRecv;
	int sizeRecv;
	int indexRecv;
	bool results[1000];
	bool result;
	MPI_Status status;
	
	while(true){
		MPI_Recv(&jobType, 1, MPI_INT, MPI_ANY_SOURCE, 1000, MPI_COMM_WORLD, &status);
		if(jobType == 1){ // findAllLegalMoves
			MPI_Recv(&boardRecv, sizeOfArray, MPI_CHAR, MASTER_ID, myid, MPI_COMM_WORLD, &status);
			MPI_Recv(&legalMoveForRecv, 1, MPI_INT, MASTER_ID, myid, MPI_COMM_WORLD, &status);
			if(sizeOfArray > slaves){
				MPI_Recv(&sizeRecv, 1, MPI_INT, MASTER_ID, myid, MPI_COMM_WORLD, &status);
				MPI_Recv(&arrayRecv, sizeRecv, MPI_INT, MASTER_ID, myid, MPI_COMM_WORLD, &status);
				
				for(i = 0; i < sizeRecv; i++){
					if(isLegalMove(boardRecv, arrayRecv[i], legalMoveForRecv)){
						results[i] = true;
					}else{
						results[i] = false;
					}
				}
				MPI_Send(results, sizeRecv, MPI_INT, MASTER_ID, myid, MPI_COMM_WORLD);
			}else{
				MPI_Recv(&indexRecv, 1, MPI_INT, MASTER_ID, myid, MPI_COMM_WORLD, &status);
				if(isLegalMove(boardRecv, indexRecv, legalMoveForRecv)){
					result = true;
					MPI_Send(&result, 1, MPI_INT, MASTER_ID, myid, MPI_COMM_WORLD);
				}else{
					result = false;
					MPI_Send(&result, 1, MPI_INT, MASTER_ID, myid, MPI_COMM_WORLD);
				}
			}
		}else{
			break;
		}
	}
}

void master(char *initialbrd, char *evalparams){
	char board[676];
	readFiles(initialbrd, evalparams);
	initBoard(board);
	printBoard(board);
	
	int bestMoves[350];
	int numOfBestMoves = 0;
	getMinimaxMoves(board, bestMoves, &numOfBestMoves);
	
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