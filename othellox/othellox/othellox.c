#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <string.h>

#define EMPTY 0
#define BLACK 1
#define WHITE 2
#define FLIP(x) (3-(x))  
#define SMALLEST_FLOAT -1.0e10
#define LARGEST_FLOAT 1.0e10

typedef enum { false, true } bool;

const int DIRECTION[8][2] = { { 1, 0 },{ 1, 1 },{ 0, 1 },{ -1, 1 },{ -1, 0 },{ -1, -1 },{ 0, -1 },{ 1, -1 } };

char board[676];

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


void printBoard(char *board) {
	// A-Z on the x-axis(left to right), 1-26 on the y-axis(top to bottom)
	for (int a = 0; a < sizeOfArray; a++) {
		printf("%d", board[a]);
		if ((a + 1) % size_x == 0) {
			printf("\n");
		}
	}
}

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
	sizeOfArray = size_x * size_y;
	for (int a = 0; a < sizeOfArray; a++) {
		board[a] = EMPTY;
	}
	for (int i = 0; i < white_size; i++) {
		board[translateInputPosToIndex(white_positions[i])] = WHITE;
	}
	for (int j = 0; j < black_size; j++) {
		board[translateInputPosToIndex(black_positions[j])] = BLACK;
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
	
	for (int dir = 0; dir < 8; dir++) { // checking for legal moves in all 8 directions, horizontal, vertial and diagonal
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
	for (int i = 0; i < sizeOfArray; i++) {
		if (isLegalMove(board, i, legalMoveFor)) {
				result = true;
				lm[count] = i;
				count++;
		}
	}
	*counter = count;
	return result;
}

void countPieces(char *board, int *numBlack, int *numWhite) {
	int black = 0;
	int white = 0;
	for (int i = 0; i < sizeOfArray; i++) {
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

void copyBoardArray(char *from, char *to) {
	for (int i = 0; i < sizeOfArray; i++) {
		to[i] = from[i];
	}
}

int flipPiecesOnBoard(char *board, int legalMove, int legalMoveFor) {
	board[legalMove] = legalMoveFor;

	int x = legalMove + 1;
	int y = 1;
	while (x > size_x) {
		x = x - size_x;
		y++;
	}

	int totalFlips = 0;

	for (int dir = 0; dir < 8; dir++) { // checking for legal moves in all 8 directions, horizontal, vertial and diagonal
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

float evaluateBoard(char *board, int turnColor, int numOfPlayerPieces, int numOfOppPieces) {
	//difference in pieces between player making the move and opponent
	//1st evaluation function
	float diffInPiecesValue = (float)100 * (numOfPlayerPieces - numOfOppPieces) / numOfPlayerPieces + numOfOppPieces;

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
		diffInMovesValue = (float)100 * (numOfLegalMovesForMax - numOfLegalMovesForMin) / numOfLegalMovesForMax + numOfLegalMovesForMin;
	}
	else {
		diffInMovesValue = 0.0;
	}

	//corners and edges captured
	//3rd evaluation function


	//final weighted score
	float score = diffInPiecesValue + diffInMovesValue;
	return score;

}

float getMax(char *board, int turnColor, int numOfPlayerPieces, int numOfOppPieces, int depth, float alpha, float beta, bool isPassedPrev) {
	numOfBoardsAccessed++;
	if (depth > depthOfBoards) {
		depthOfBoards = depth;
	}
	
	if (numOfPlayerPieces == 0) { // lose!
		return SMALLEST_FLOAT;
	}
	else if (numOfOppPieces == 0) {
		return LARGEST_FLOAT;
	}

	if ((numOfPlayerPieces + numOfOppPieces) == sizeOfArray) { //end game
		//return evaluateEndGameBoard(board, turnColor, numOfPlayerPieces, numOfOppPieces);
		return evaluateBoard(board, turnColor, numOfPlayerPieces, numOfOppPieces);
	}
	if (depth == maxDepth) {
		return evaluateBoard(board, turnColor, numOfPlayerPieces, numOfOppPieces);
	}
	if (numOfBoardsAccessed >= maxBoards) {
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
		if (isPassedPrev) {
			return evaluateBoard(board, turnColor, numOfPlayerPieces, numOfOppPieces);
		}
		curValue = getMin(mmBoard, FLIP(turnColor), numOfPlayerPieces, numOfOppPieces, depth + 1, alpha, beta, true);
		if (curValue > maxValue) {
			maxValue = curValue;
		}
		return maxValue;
	}

	for (int i = 0; i < numOfLegalMoves; i++) {
		if (numOfBoardsAccessed >= maxBoards) {
			isEntireSpace = false;
			break;
		}
		copyBoardArray(board, mmBoard);
		numOfFlipped = flipPiecesOnBoard(mmBoard, legalMoves[i], turnColor);
		
		//printf("%d \n", numOfFlipped);printBoard(mmBoard);printf("\n");

		curValue = getMin(mmBoard, FLIP(turnColor), numOfPlayerPieces + numOfFlipped + 1, 
			numOfOppPieces - numOfFlipped, depth + 1, alpha, beta, false);
		if (curValue > maxValue) {
			maxValue = curValue;
		}
		if (maxValue >= beta) {
			return maxValue;
		}
		if (maxValue > alpha) {
			alpha = maxValue;
		}
	}
	return maxValue;
}

float getMin(char *board, int turnColor, int numOfPlayerPieces, int numOfOppPieces, int depth, float alpha, float beta, bool isPassedPrev) {
	numOfBoardsAccessed++;
	if (depth > depthOfBoards) {
		depthOfBoards = depth;
	}

	if (numOfPlayerPieces == 0) { // lose!
		return SMALLEST_FLOAT;
	}
	else if (numOfOppPieces == 0) {
		return LARGEST_FLOAT;
	}

	if ((numOfPlayerPieces + numOfOppPieces) == sizeOfArray) { //end game
		//return evaluateBoard(board, turnColor, numOfPlayerPieces, numOfOppPieces);
		return evaluateBoard(board, turnColor, numOfPlayerPieces, numOfOppPieces);
	}
	if (depth == maxDepth) {
		return evaluateBoard(board, turnColor, numOfPlayerPieces, numOfOppPieces);
	}
	if (numOfBoardsAccessed >= maxBoards) {
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
		if (isPassedPrev) {
			return evaluateBoard(board, turnColor, numOfPlayerPieces, numOfOppPieces);
		}
		curValue = getMax(mmBoard, FLIP(turnColor), numOfPlayerPieces, numOfOppPieces, depth + 1, alpha, beta, true);
		if (curValue < minValue) {
			minValue = curValue;
		}
		return minValue;
	}

	for (int i = 0; i < numOfLegalMoves; i++) {
		if (numOfBoardsAccessed >= maxBoards) {
			isEntireSpace = false;
			break;
		}
		copyBoardArray(board, mmBoard);
		numOfFlipped = flipPiecesOnBoard(mmBoard, legalMoves[i], turnColor);

		//printf("%d \n", numOfFlipped);printBoard(mmBoard);printf("\n");

		curValue = getMax(mmBoard, FLIP(turnColor), numOfPlayerPieces - numOfFlipped, 
			numOfOppPieces + numOfFlipped + 1, depth + 1, alpha, beta, false);
		if (curValue < minValue) {
			minValue = curValue;
		}
		if (minValue <= alpha) {
			return minValue;
		}
		if (minValue < beta) {
			beta = minValue;
		}
	}
	return minValue;
}

void getMinimaxMoves(int *bestMoves, int *bmSize) {
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

	if (turnColor == BLACK) {
		numOfPlayerPieces = black_size;
		numOfOppPieces = white_size;
	}
	else {
		numOfPlayerPieces = white_size;
		numOfOppPieces = black_size;
	}

	for (int i = 0; i < numOfLegalMoves; i++) {
		if (numOfBoardsAccessed >= maxBoards) {
			break;
		}
		copyBoardArray(board, mmBoard);
		numOfFlipped = flipPiecesOnBoard(mmBoard, legalMoves[i], turnColor);

		//printf("%d \n", numOfFlipped);printBoard(mmBoard);printf("\n");

		valuesOfLegalMoves[i] = getMin(mmBoard, FLIP(turnColor), numOfPlayerPieces + numOfFlipped + 1, 
			numOfOppPieces - numOfFlipped, depth + 1, alpha, beta, false);

		if (valuesOfLegalMoves[i] > highestValue) {
			highestValue = valuesOfLegalMoves[i];
		}

		printf("%d -> %f \n", legalMoves[i], valuesOfLegalMoves[i]);
	}

	for (int i = 0; i < numOfLegalMoves; i++) {
		if (valuesOfLegalMoves[i] == highestValue) {
			bestMoves[bestMovesCount] = legalMoves[i];
			bestMovesCount++;
		}
	}
	*bmSize = bestMovesCount;
}

int main(int argc, char **argv)
{
	clock_t begin = clock();

	readFiles(argv[1], argv[2]);
	initBoard();

	int bestMoves[350];
	int numOfBestMoves = 0;
	getMinimaxMoves(bestMoves, &numOfBestMoves);

	clock_t end = clock();
	elapsedTimeInSec = (double)(end - begin) / CLOCKS_PER_SEC;

	printf("Best moves: {");
	for (int i = 0; i < numOfBestMoves; i++) {
		if (i == numOfBestMoves - 1) {
			printf("%s}\n", translateIndexToOutputPos(bestMoves[i]));
		}
		else {
			printf("%s,", translateIndexToOutputPos(bestMoves[i]));
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
	printf("Elapsed time in seconds: %f", elapsedTimeInSec);

	getch();
	return 0;
}