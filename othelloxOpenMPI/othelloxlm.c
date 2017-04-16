#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include <mpi.h>

#define EMPTY 0
#define BLACK 1
#define WHITE 2
#define FLIP(x) (3-(x))  
#define SMALLEST_FLOAT -1.0e10
#define LARGEST_FLOAT 1.0e10
#define MASTER_ID slaves
#define JOB_TAG 1001

typedef enum { false, true } bool;

// To specify the 8 different directions in (x, y) for finding legal moves and flipping pieces on board
const int DIRECTION[8][2] = { { 1, 0 },{ 1, 1 },{ 0, 1 },{ -1, 1 },{ -1, 0 },{ -1, -1 },{ 0, -1 },{ 1, -1 } };

int slaves;
int myid;

long long comm_time = 0;
long long comp_time = 0;
clock_t begin, end, current;

//for output data analysis
int numOfBoardsAccessed = 0;
int depthOfBoards = 0;
bool isEntireSpace = true;
double elapsedTimeInSec = 0.0;

// initialbrd variables
int sizeOfArray;
int size_x, size_y;
char white_positions[350][5];
char black_positions[350][5];
int black_size, white_size;

// evalparams variables
int maxDepth, maxBoards, cornerValue, edgeValue, bestMovesForColor, timeOut;

//declaration of function due to being mutually recursive
float getMax(char *board, int turnColor, int numOfPlayerPieces, int numOfOppPieces, int depth, float alpha, float beta, bool isPassPrev);
float getMin(char *board, int turnColor, int numOfPlayerPieces, int numOfOppPieces, int depth, float alpha, float beta, bool isPassPrev);

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

/*
Trim all spaces front and back from string
*/
char *trimWhiteSpace(char *str) {
	char *end;

	if (str == NULL) {
		return str;
	}

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

/*
Translate a position on the board to an index used in board array
"a1" -> 0 or h10 -> #number
*/
int translateInputPosToIndex(char *pos) {
	int letter = pos[0] - 96;
	int num, result;
	if (strlen(pos) == 2) {
		num = pos[1] - '0';
	}
	else {
		num = (pos[1] - '0') * 10 + (pos[2] - '0');
	}

	result = (num - 1) + (letter - 1) * size_x;
	return result;
}

/*
Translate an index used in board array back to the position on the board
0 -> "a1" or #number -> h10
*/
char *translateIndexToOutputPos(int index) {
	int num = index + 1;
	int letter = 1;
	while (num > size_x) {
		num = num - size_x;
		letter++;
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

/*
Print the current state of the board (for debugging)
A-Z on the y-axis(top to bottom), 1-26 on the x-axis(left to right)
*/
void printBoard(char *board) {
	int a;
	for (a = 0; a < sizeOfArray; a++) {
		printf("%d", board[a]);
		if ((a + 1) % size_x == 0) {
			printf("\n");
		}
	}
}

/*
Read initialbrd.txt and evalparams.txt and parse them
*/
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
		if (strcmp(label, "Size") == 0) {
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
					strcpy(white_positions[count], posInv);
				}
				else if (strcmp(label, "Black") == 0) {
					strcpy(black_positions[count], posInv);
				}
				posInv = strtok(NULL, ",");
				count++;
			}
			if (strcmp(label, "White") == 0) {
				white_size = count;
			}
			else if (strcmp(label, "Black") == 0) {
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
		else if (strcmp(label, "MaxBoards") == 0) {
			maxBoards = atoi(details);
		}
		else if (strcmp(label, "CornerValue") == 0) {
			cornerValue = atoi(details);
		}
		else if (strcmp(label, "EdgeValue") == 0) {
			edgeValue = atoi(details);
		}
		else {
			//nothing
		}
	}

	fclose(file1);
	fclose(file2);

	//broadcast readFiles data from master to all slaves
	int i;
	MPI_Bcast(&size_x, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&size_y, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&sizeOfArray, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&black_size, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&white_size, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	for (i = 0; i < white_size; i++) {
		MPI_Bcast(&white_positions[i], 5, MPI_CHAR, MASTER_ID, MPI_COMM_WORLD);
	}
	for (i = 0; i < black_size; i++) {
		MPI_Bcast(&black_positions[i], 5, MPI_CHAR, MASTER_ID, MPI_COMM_WORLD);
	}
	MPI_Bcast(&maxDepth, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&maxBoards, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&cornerValue, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&edgeValue, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&bestMovesForColor, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&timeOut, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
}

/*
Initialise board using parsed info from both .txt files
*/
void initBoard(char *board) {
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

/*
Checks if a position on the current board is a legal move for a specific player turn
index: position on the board
legalMoveFor: either black or white player's turn
*/
bool isLegalMove(char *board, int index, int legalMoveFor) {
	//convert index into (x,y) on the board
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

			//out of the board 
			if (tempx <= 0 || tempx > size_x || tempy <= 0 || tempy > size_y) {
				cont = false;
				continue;
			}

			//check if pieces around can be flipped
			int tempIndex = (tempx - 1) + (tempy - 1) * size_x;
			if (board[tempIndex] == EMPTY) {
				cont = false;
			}
			else if (board[tempIndex] == WHITE) {
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

/*
Note: Not used in this program, a parallelised function findAllLegalMoves is used here
Checks for all positions on the board and determine if they are legal moves
lm: stores the indexs in the array that are legal moves
counter: stores the number of legal moves found
result: true if there are legal moves
*/
bool findAllLegalMovesSeq(char *board, int legalMoveFor, int *lm, int *counter) {
	bool result = false;
	int count = 0;
	int i;
	for (i = 0; i < sizeOfArray; i++) {
		if (isLegalMove(board, i, legalMoveFor)) {
			result = true;
			lm[count] = i;
			count++;
		}
	}
	*counter = count;
	return result;
}

/*
Note: Parallelised version (OpenMPI) used in this program
Checks for all positions on the board and determine if they are legal moves
lm: stores the indexs in the array that are legal moves
counter: stores the number of legal moves found
result: true if there are legal moves
*/
bool findAllLegalMoves(char *board, int legalMoveFor, int *lm, int *counter) {
	int jobNo = 1; // used to differentiate jobs in slaves 
	bool result = false;
	int count = 0;
	int i, slave_id;
	int indexCounter = 0;
	bool resultsRecv[1000];
	bool resultRecv;
	MPI_Status status;

	//has to split work equally in this case
	if (sizeOfArray > slaves) {
		//split work to slaves
		for (slave_id = 0; slave_id < slaves; slave_id++) {
			//formula to split work among slaves available
			int startIndex = (int)(sizeOfArray * slave_id / slaves);
			int size = (int)(sizeOfArray * (slave_id + 1) / slaves) - startIndex;
			int arraySend[size];
			for (i = 0; i < size; i++) {
				arraySend[i] = startIndex + i;
			}
			//send info needed by slaves
			MPI_Send(&jobNo, 1, MPI_INT, slave_id, JOB_TAG, MPI_COMM_WORLD);
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
			for (i = 0; i < size; i++) {
				if (resultsRecv[i]) {
					result = true;
					lm[count] = indexCounter;
					count++;
				}
				indexCounter++; //to cycle through the whole board array
			}
		}
	}
	else {
		//split work to slaves
		for (i = 0; i < sizeOfArray; i++) {
			MPI_Send(&jobNo, 1, MPI_INT, i, JOB_TAG, MPI_COMM_WORLD);
			MPI_Send(&board[0], sizeOfArray, MPI_CHAR, i, i, MPI_COMM_WORLD);
			MPI_Send(&legalMoveFor, 1, MPI_INT, i, i, MPI_COMM_WORLD);
			MPI_Send(&i, 1, MPI_INT, i, i, MPI_COMM_WORLD);
		}
		//receive results from slaves
		for (i = 0; i < sizeOfArray; i++) {
			MPI_Recv(&resultRecv, 1, MPI_INT, i, i, MPI_COMM_WORLD, &status);
			if (resultRecv) {
				result = true;
				lm[count] = i;
				count++;
			}
		}
	}
	*counter = count;
	return result;
}

/*
Not used in the program since a better technique is used to keep track of pieces on the board
flipPiecesOnBoard function helps in tracking of pieces on board
*/
void countPieces(char *board, int *numBlack, int *numWhite) {
	int black = 0;
	int white = 0;
	int i;
	for (i = 0; i < sizeOfArray; i++) {
		if (board[i] == BLACK) {
			black++;
		}
		else if (board[i] == WHITE) {
			white++;
		}
	}
	*numBlack = black;
	*numWhite = white;
}

/*
To copy from the state of a board to another
*/
void copyBoardArray(char *from, char *to) {
	int i;
	for (i = 0; i < sizeOfArray; i++) {
		to[i] = from[i];
	}
}

/*
To flip the pieces on the current board using a specific legal move position
Returns the total number of pieces flipped for a specific player turn
*/
int flipPiecesOnBoard(char *board, int legalMove, int legalMoveFor) {
	board[legalMove] = legalMoveFor;
	// convert legalMove index to (x,y) on board
	int x = legalMove + 1;
	int y = 1;
	while (x > size_x) {
		x = x - size_x;
		y++;
	}

	int totalFlips = 0;
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
			// out of the board
			if (tempx <= 0 || tempx > size_x || tempy <= 0 || tempy > size_y) {
				cont = false;
				continue;
			}

			int tempIndex = (tempx - 1) + (tempy - 1) * size_x;
			// checks if the pieces around can be flipped
			if (board[tempIndex] == EMPTY) {
				cont = false;
			}
			else if (board[tempIndex] == WHITE) {
				if (legalMoveFor == BLACK) {
					numFlipped++;
				}
				else {
					if (numFlipped == 0) {
						cont = false;
					}
					else {
						totalFlips += numFlipped;
						int flipIndex;
						// flipping done on the board
						while (numFlipped > 0) {
							tempx = tempx - dx;
							tempy = tempy - dy;
							flipIndex = (tempx - 1) + (tempy - 1) * size_x;
							board[flipIndex] = FLIP(board[flipIndex]);
							numFlipped--;
						}
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
						totalFlips += numFlipped;
						int flipIndex;
						// flipping done on the board
						while (numFlipped > 0) {
							tempx = tempx - dx;
							tempy = tempy - dy;
							flipIndex = (tempx - 1) + (tempy - 1) * size_x;
							board[flipIndex] = FLIP(board[flipIndex]);
							numFlipped--;
						}
					}
				}
			}
		}
	}
	return totalFlips;
}

/*
To evaluate the current state of the board and return a score
Score is calculated based on 4 different evaluation function added together
*/
float evaluateBoard(char *board, int turnColor, int numOfPlayerPieces, int numOfOppPieces) {
	//difference in pieces between player making the move and opponent
	//1st evaluation function
	float diffInPiecesValue = (float)100 * (numOfPlayerPieces - numOfOppPieces) / (numOfPlayerPieces + numOfOppPieces);

	//relative difference in number of legal moves for both players
	//2nd evaluation function
	int legalMovesForMax[350];
	int numOfLegalMovesForMax = 0;
	findAllLegalMoves(board, bestMovesForColor, legalMovesForMax, &numOfLegalMovesForMax);

	int legalMovesForMin[350];
	int numOfLegalMovesForMin = 0;
	findAllLegalMoves(board, FLIP(bestMovesForColor), legalMovesForMin, &numOfLegalMovesForMin);

	float diffInMovesValue;
	if (numOfLegalMovesForMax + numOfLegalMovesForMin != 0) {
		diffInMovesValue = (float)100 * (numOfLegalMovesForMax - numOfLegalMovesForMin) / (numOfLegalMovesForMax + numOfLegalMovesForMin);
	}
	else {
		diffInMovesValue = 0.0;
	}

	//corners captured
	//3rd evaluation function
	float cornerHeuristicValue = 0.0;
	float cornerValueForMax = 0.0, cornerValueForMin = 0.0;
	int corners[4];

	if (size_x > 1 && size_y > 1) {
		corners[0] = 0;
		corners[1] = size_x - 1;
		corners[2] = (size_y - 1) * size_x;
		corners[3] = size_x * size_y - 1;
		int j;
		for (j = 0; j < 4; j++) {
			if (board[corners[j]] == BLACK) {
				if (bestMovesForColor == BLACK) {
					cornerValueForMax += 1.0;
				}
				else {
					cornerValueForMin += 1.0;
				}
			}
			else if (board[corners[j]] == WHITE) {
				if (bestMovesForColor == WHITE) {
					cornerValueForMax += 1.0;
				}
				else {
					cornerValueForMin += 1.0;
				}
			}
		}
		if (cornerValueForMax + cornerValueForMin != 0.0) {
			cornerHeuristicValue = (float)100 * (cornerValueForMax - cornerValueForMin) / (cornerValueForMax + cornerValueForMin);
		}
	}

	//edges captured
	//4th evaluation function
	float edgesHeuristicValue = 0.0;
	float edgesValueForMax = 0.0, edgesValueForMin = 0.0;
	int edges[100];
	int edgesCount = 0;

	if (size_x > 1 && size_y > 1) {
		int i;
		for (i = corners[0] + 1; i < corners[1]; i++) {
			edges[edgesCount] = i;
			edgesCount++;
		}
		for (i = corners[1] + size_x; i < corners[3]; i += size_x) {
			edges[edgesCount] = i;
			edgesCount++;
		}
		for (i = corners[0] + size_x; i < corners[2]; i += size_x) {
			edges[edgesCount] = i;
			edgesCount++;
		}
		for (i = corners[2] + 1; i < corners[3]; i++) {
			edges[edgesCount] = i;
			edgesCount++;
		}

		for (i = 0; i < edgesCount; i++) {
			if (board[edges[i]] == BLACK) {
				if (bestMovesForColor == BLACK) {
					edgesValueForMax += 1.0;
				}
				else {
					edgesValueForMin += 1.0;
				}
			}
			else if (board[edges[i]] == WHITE) {
				if (bestMovesForColor == WHITE) {
					edgesValueForMax += 1.0;
				}
				else {
					edgesValueForMin += 1.0;
				}
			}
		}
		if (edgesValueForMax + edgesValueForMin != 0.0) {
			edgesHeuristicValue = (float)100 * (edgesValueForMax - edgesValueForMin) / (edgesValueForMax + edgesValueForMin);
		}
	}

	//final weighted score
	float w1 = 1.0, w2 = 5.0, w3 = (float)cornerValue, w4 = (float)edgeValue;
	float score = w1 * diffInPiecesValue + w2 * diffInMovesValue + w3 * cornerHeuristicValue + w4 * edgesHeuristicValue;

	return score;
}

/*
Part of the alpha-beta pruning Minimax algorithm
Return the max value
*/
float getMax(char *board, int turnColor, int numOfPlayerPieces, int numOfOppPieces, int depth, float alpha, float beta, bool isPassedPrev) {
	numOfBoardsAccessed++;
	if (depth > depthOfBoards) {
		depthOfBoards = depth;
	}

	// lose!
	if (numOfPlayerPieces == 0) {
		return SMALLEST_FLOAT;
	}
	else if (numOfOppPieces == 0) {
		return LARGEST_FLOAT;
	}

	if ((numOfPlayerPieces + numOfOppPieces) == sizeOfArray) { //end game
		return evaluateBoard(board, turnColor, numOfPlayerPieces, numOfOppPieces);
	}
	if (depth == maxDepth) { //stops when max depth is reached
		return evaluateBoard(board, turnColor, numOfPlayerPieces, numOfOppPieces);
	}
	if (numOfBoardsAccessed >= maxBoards) { //stops when max number of boards is reached
		return evaluateBoard(board, turnColor, numOfPlayerPieces, numOfOppPieces);
	}
	
	current = clock(); 
	double currentElapsedTimeInSec = (double)(current - begin) / CLOCKS_PER_SEC;
	if (currentElapsedTimeInSec > (double)(timeOut - 1)) { // stops when current time is reaching timeout
		isEntireSpace = false;
		return evaluateBoard(board, turnColor, numOfPlayerPieces, numOfOppPieces);
	}
	
	float maxValue = SMALLEST_FLOAT;
	float curValue = 0.0;
	int numOfFlipped;
	char mmBoard[676];

	int legalMoves[350];
	int numOfLegalMoves = 0;
	findAllLegalMoves(board, turnColor, legalMoves, &numOfLegalMoves);

	//No legal moves, have to pass
	if (numOfLegalMoves == 0) {
		//if the previous turn is passed as well, game ends
		if (isPassedPrev) {
			return evaluateBoard(board, turnColor, numOfPlayerPieces, numOfOppPieces);
		}
		//isPassedPrev parameter in getMin is true since the current turn is passed
		curValue = getMin(mmBoard, FLIP(turnColor), numOfPlayerPieces, numOfOppPieces, depth + 1, alpha, beta, true);
		if (curValue > maxValue) {
			maxValue = curValue;
		}
		return maxValue;
	}

	int i;
	for (i = 0; i < numOfLegalMoves; i++) {
		// since the max number of boards to be accessed is reached and there are legal moves,
		// the entire space is not check as a result
		if (numOfBoardsAccessed >= maxBoards) {
			isEntireSpace = false;
			break;
		}
		copyBoardArray(board, mmBoard);
		numOfFlipped = flipPiecesOnBoard(mmBoard, legalMoves[i], turnColor);
		// the number of player/opp pieces is updated using numOfFlipped and tracked
		curValue = getMin(mmBoard, FLIP(turnColor), numOfPlayerPieces + numOfFlipped + 1,
			numOfOppPieces - numOfFlipped, depth + 1, alpha, beta, false);
		if (curValue > maxValue) {
			maxValue = curValue;
		}
		// for prunning
		if (maxValue >= beta) {
			return maxValue;
		}
		if (maxValue > alpha) {
			alpha = maxValue;
		}
	}
	return maxValue;
}

/*
Part of the alpha-beta pruning Minimax algorithm
Return the min value
*/
float getMin(char *board, int turnColor, int numOfPlayerPieces, int numOfOppPieces, int depth, float alpha, float beta, bool isPassedPrev) {
	numOfBoardsAccessed++;
	if (depth > depthOfBoards) {
		depthOfBoards = depth;
	}
	// lose!
	if (numOfPlayerPieces == 0) {
		return SMALLEST_FLOAT;
	}
	else if (numOfOppPieces == 0) {
		return LARGEST_FLOAT;
	}

	if ((numOfPlayerPieces + numOfOppPieces) == sizeOfArray) { //end game
		return evaluateBoard(board, turnColor, numOfPlayerPieces, numOfOppPieces);
	}
	if (depth == maxDepth) { //stops when max depth is reached
		return evaluateBoard(board, turnColor, numOfPlayerPieces, numOfOppPieces);
	}
	if (numOfBoardsAccessed >= maxBoards) { //stops when max number of boards is reached
		return evaluateBoard(board, turnColor, numOfPlayerPieces, numOfOppPieces);
	}
	
	current = clock(); 
	double currentElapsedTimeInSec = (double)(current - begin) / CLOCKS_PER_SEC;
	if (currentElapsedTimeInSec > (double)(timeOut - 1)) { // stops when current time is reaching timeout
		isEntireSpace = false;
		return evaluateBoard(board, turnColor, numOfPlayerPieces, numOfOppPieces);
	}

	float minValue = LARGEST_FLOAT;
	float curValue = 0.0;
	int numOfFlipped;
	char mmBoard[676];

	int legalMoves[350];
	int numOfLegalMoves = 0;
	findAllLegalMoves(board, turnColor, legalMoves, &numOfLegalMoves);

	//No legal moves, have to pass
	if (numOfLegalMoves == 0) {
		//if the previous turn is passed as well, game ends
		if (isPassedPrev) {
			return evaluateBoard(board, turnColor, numOfPlayerPieces, numOfOppPieces);
		}
		//isPassedPrev parameter in getMax is true since the current turn is passed
		curValue = getMax(mmBoard, FLIP(turnColor), numOfPlayerPieces, numOfOppPieces, depth + 1, alpha, beta, true);
		if (curValue < minValue) {
			minValue = curValue;
		}
		return minValue;
	}
	int i;
	for (i = 0; i < numOfLegalMoves; i++) {
		// since the max number of boards to be accessed is reached and there are legal moves,
		// the entire space is not check as a result
		if (numOfBoardsAccessed >= maxBoards) {
			isEntireSpace = false;
			break;
		}
		copyBoardArray(board, mmBoard);
		numOfFlipped = flipPiecesOnBoard(mmBoard, legalMoves[i], turnColor);
		// the number of player/opp pieces is updated using numOfFlipped and tracked
		curValue = getMax(mmBoard, FLIP(turnColor), numOfPlayerPieces - numOfFlipped,
			numOfOppPieces + numOfFlipped + 1, depth + 1, alpha, beta, false);
		if (curValue < minValue) {
			minValue = curValue;
		}
		// for prunning
		if (minValue <= alpha) {
			return minValue;
		}
		if (minValue < beta) {
			beta = minValue;
		}
	}
	return minValue;
}

/*
Part of the alpha-beta pruning Minimax algorithm
The main function that starts the algorithm by calling getMin
*/
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
	// get all legal moves from the current state of board and turn player
	findAllLegalMoves(board, turnColor, legalMoves, &numOfLegalMoves);

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
		// since the max number of boards to be accessed is reached and there are legal moves,
		// the entire space is not check as a result
		if (numOfBoardsAccessed >= maxBoards) {
			break;
		}
		copyBoardArray(board, mmBoard);
		numOfFlipped = flipPiecesOnBoard(mmBoard, legalMoves[i], turnColor);
		// the number of player/opp pieces is updated using numOfFlipped and tracked
		valuesOfLegalMoves[i] = getMin(mmBoard, FLIP(turnColor), numOfPlayerPieces + numOfFlipped + 1,
			numOfOppPieces - numOfFlipped, depth + 1, alpha, beta, false);

		//update and get the highest value/score 	
		if (valuesOfLegalMoves[i] > highestValue) {
			highestValue = valuesOfLegalMoves[i];
		}

		printf("%d -> %f \n", legalMoves[i], valuesOfLegalMoves[i]);
	}

	// get all legal best moves with the highest score
	for (i = 0; i < numOfLegalMoves; i++) {
		if (valuesOfLegalMoves[i] == highestValue) {
			bestMoves[bestMovesCount] = legalMoves[i];
			bestMovesCount++;
		}
	}
	*bmSize = bestMovesCount;
}

void slave() {
	int i;
	long long before, after;
	// receive broadcast readFiles data from master to slaves
	MPI_Bcast(&size_x, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&size_y, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&sizeOfArray, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&black_size, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&white_size, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	for (i = 0; i < white_size; i++) {
		MPI_Bcast(&white_positions[i], 5, MPI_CHAR, MASTER_ID, MPI_COMM_WORLD);
	}
	for (i = 0; i < black_size; i++) {
		MPI_Bcast(&black_positions[i], 5, MPI_CHAR, MASTER_ID, MPI_COMM_WORLD);
	}
	MPI_Bcast(&maxDepth, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&maxBoards, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&cornerValue, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&edgeValue, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&bestMovesForColor, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);
	MPI_Bcast(&timeOut, 1, MPI_INT, MASTER_ID, MPI_COMM_WORLD);

	int jobNo;
	int sizeOfArrayRecv;
	int arrayRecv[1000];
	char boardRecv[676];
	int legalMoveForRecv;
	int indexRecv;
	bool results[1000];
	bool isLegalMoveResult;

	int turnColor;
	int numOfPlayerPieces;
	int numOfOppPieces;
	int depth;
	float alpha;
	float beta;
	float value;
	int slavesLeft;
	MPI_Status status;

	while (true) {
		before = wall_clock_time();
		// receive message about job number to be executed
		MPI_Recv(&jobNo, 1, MPI_INT, MPI_ANY_SOURCE, JOB_TAG, MPI_COMM_WORLD, &status);
		if (jobNo == 1) { // called by findAllLegalMoves
			// receive the data needed for computation from Master
			MPI_Recv(&boardRecv, sizeOfArray, MPI_CHAR, MASTER_ID, myid, MPI_COMM_WORLD, &status);
			MPI_Recv(&legalMoveForRecv, 1, MPI_INT, MASTER_ID, myid, MPI_COMM_WORLD, &status);
			if (sizeOfArray > slaves) {
				MPI_Recv(&sizeOfArrayRecv, 1, MPI_INT, MASTER_ID, myid, MPI_COMM_WORLD, &status);
				MPI_Recv(&arrayRecv, sizeOfArrayRecv, MPI_INT, MASTER_ID, myid, MPI_COMM_WORLD, &status);
				after = wall_clock_time();
				comm_time += after - before;

				// work done in slave
				before = wall_clock_time();
				for (i = 0; i < sizeOfArrayRecv; i++) {
					if (isLegalMove(boardRecv, arrayRecv[i], legalMoveForRecv)) {
						results[i] = true;
					}
					else {
						results[i] = false;
					}
				}
				after = wall_clock_time();
				comp_time += after - before;

				before = wall_clock_time();
				// send the results back to master
				MPI_Send(results, sizeOfArrayRecv, MPI_INT, MASTER_ID, myid, MPI_COMM_WORLD);
				after = wall_clock_time();
				comm_time += after - before;
			}
			else {
				// receive the data needed for computation from Master
				MPI_Recv(&indexRecv, 1, MPI_INT, MASTER_ID, myid, MPI_COMM_WORLD, &status);
				after = wall_clock_time();
				comm_time += after - before;

				// work done in slave
				before = wall_clock_time();
				isLegalMoveResult = isLegalMove(boardRecv, indexRecv, legalMoveForRecv);
				after = wall_clock_time();
				comp_time += after - before;

				before = wall_clock_time();
				// send the results back to master
				MPI_Send(&isLegalMoveResult, 1, MPI_INT, MASTER_ID, myid, MPI_COMM_WORLD);
				after = wall_clock_time();
				comm_time += after - before;
			}
		}
		else if (jobNo == 2) { // called by getMinimaxMoves
			// receive the data needed for computation from Master
			MPI_Recv(&boardRecv, sizeOfArray, MPI_CHAR, MASTER_ID, myid, MPI_COMM_WORLD, &status);
			MPI_Recv(&turnColor, 1, MPI_INT, MASTER_ID, myid, MPI_COMM_WORLD, &status);
			MPI_Recv(&numOfPlayerPieces, 1, MPI_INT, MASTER_ID, myid, MPI_COMM_WORLD, &status);
			MPI_Recv(&numOfOppPieces, 1, MPI_INT, MASTER_ID, myid, MPI_COMM_WORLD, &status);
			MPI_Recv(&depth, 1, MPI_INT, MASTER_ID, myid, MPI_COMM_WORLD, &status);
			MPI_Recv(&alpha, 1, MPI_FLOAT, MASTER_ID, myid, MPI_COMM_WORLD, &status);
			MPI_Recv(&beta, 1, MPI_FLOAT, MASTER_ID, myid, MPI_COMM_WORLD, &status);
			after = wall_clock_time();
			comm_time += after - before;

			// work done in slave
			before = wall_clock_time();
			bool isPassedPrev = false;
			value = getMin(boardRecv, FLIP(turnColor), numOfPlayerPieces, numOfOppPieces, depth, alpha, beta, isPassedPrev);
			after = wall_clock_time();
			comp_time += after - before;

			before = wall_clock_time();
			// send the results back to master
			MPI_Send(&value, 1, MPI_FLOAT, MASTER_ID, myid, MPI_COMM_WORLD);
			MPI_Send(&numOfBoardsAccessed, 1, MPI_INT, MASTER_ID, myid, MPI_COMM_WORLD);
			MPI_Send(&depthOfBoards, 1, MPI_INT, MASTER_ID, myid, MPI_COMM_WORLD);
			MPI_Send(&isEntireSpace, 1, MPI_INT, MASTER_ID, myid, MPI_COMM_WORLD);
			after = wall_clock_time();
			comm_time += after - before;
		}
		else { //jobNo == 0, means slaves are no longer needed and no jobs left
			break;
		}
	}
	// print info for report and analysis
	printf(" --- SLAVE %d: communication_time=%6.2f seconds; computation_time=%6.2f seconds\n", myid, comm_time / 1000000000.0, comp_time / 1000000000.0);
}

void master(char *initialbrd, char *evalparams) {
	char board[676]; // initial board specified by initialbrd.txt
	int bestMoves[350];
	int numOfBestMoves = 0;

	//get the time taken to run the program
	begin = clock();
	readFiles(initialbrd, evalparams);
	initBoard(board);
	getMinimaxMoves(board, bestMoves, &numOfBestMoves);
	end = clock();
	elapsedTimeInSec = (double)(end - begin) / CLOCKS_PER_SEC;

	// break while loop in slaves when there is no more jobs left
	int slave_id;
	int jobType = 0;
	for (slave_id = 0; slave_id < slaves; slave_id++) {
		MPI_Send(&jobType, 1, MPI_INT, slave_id, JOB_TAG, MPI_COMM_WORLD);
	}

	// print details for analysis after a run
	printf("Best moves: {");
	if (numOfBestMoves == 0) {
		printf("na}\n");
	}
	else {
		int i;
		for (i = 0; i < numOfBestMoves; i++) {
			if (i == numOfBestMoves - 1) {
				printf("%s}\n", translateIndexToOutputPos(bestMoves[i]));
			}
			else {
				printf("%s,", translateIndexToOutputPos(bestMoves[i]));
			}
		}
	}

	printf("Number of boards assessed: %d\n", numOfBoardsAccessed);
	printf("Depth of boards: %d\n", depthOfBoards);
	if (isEntireSpace) {
		printf("Entire space: true");
	}
	else {
		printf("Entire space: false");
	}
	printf("\n");
	printf("Elapsed time in seconds: %f\n", elapsedTimeInSec);
}

/*
Using master-slave paradigm

Master process has id = the number of processes - 1
Total number of processes is 1 + number of slaves
*/
int main(int argc, char **argv) {
	int nprocs;
	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
	MPI_Comm_rank(MPI_COMM_WORLD, &myid);

	slaves = nprocs - 1;

	if (argc != 3) {
		if (myid == MASTER_ID) {
			fprintf(stderr, "Error, invalid number of arguments!");
			MPI_Finalize();
			exit(1);
		}
	}

	if (myid == MASTER_ID) {
		master(argv[1], argv[2]);
	}
	else {
		slave();
	}

	MPI_Finalize();
	return 0;
}