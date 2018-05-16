#include "stdlib.h"
//#include "inc/tm4c1294ncpt.h" // microcontroller components
#include "stdio.h"
#include "time.h" // to generate random positions in the gameBoard

#define NUMROW_LED 8 // number of rows in the LED pendulum display 
#define NUMCOL_LED 60 // number of columns in the LED pendulum display
char display[NUMCOL_LED]; // each element represent a column in the LED pendulum; msut convert gameBoard to display

enum object{empty, s_left, s_right, s_up, s_down, food, invalid}; // invalid is used moveSnake() if collision occurs
enum object gameBoard[NUMROW_LED][NUMCOL_LED];
int snakeLength = 1;
int tail_pos[2] = {7, 0}; // start at bottom left-most position on the screen
int food_pos[2] = {0, 0}; // position of food ; variable used to remove food in constant time

char incrLength = 0; // if snake gets food, this is set to "1"

void printBoard() {

	printf("-----\n");	
	printf("tail_pos (row=%i, col=%i), food_pos (row=%i, col=%i)\n", tail_pos[0], tail_pos[1], food_pos[0], food_pos[1]);
	for(int i=0; i<NUMROW_LED; i++) {
		for(int j=0; j<NUMCOL_LED; j++) {
			switch(gameBoard[i][j]) {
				case empty:
					printf("0 ");
					break;
				case s_left:
					printf("< ");
					break;
				case s_right:
					printf("> ");
					break;
				case s_up:
					printf("^ ");
					break;
				case s_down:
					printf("^ ");
					break;
				case food:
					printf("* ");
					break;
				default:
					printf("");
					break;
			}	
		}
		printf("\n");
	}
	printf("-----\n");	
}

void addFood() {

	char placed = 0;
	int row, col;

	// remove the existing food on the board, if there is any
	if(gameBoard[food_pos[0]][food_pos[1]] == food) {
		gameBoard[food_pos[0]][food_pos[1]] = empty; 
	}

	// rand() % (max_number + 1 - minimum_number) + minimum_number
	while(!placed) {
		// choose a random coordiante on the board
		row = rand() % NUMROW_LED; 
		col = rand() % NUMCOL_LED;
		
		if(gameBoard[row][col] == empty) {
			gameBoard[row][col] = food;
			food_pos[0] = row;
			food_pos[1] = col;
			placed = 1;
		}
	}
	
}

enum object moveSnake(int count, int row, int col, enum object dir) { // count = number of pixels of the snake that we have seen already; (row, col) = current location on board

	printf("moveSnake() row=%i, col=%i\n", row, col);
	// base case	
	if(count == snakeLength) { // in the position in front of the snake's head
	
		// calculate the new position of the head
		int new_row = row;
		int new_col = col;
		switch(dir) {
			case s_left:
				new_col--;
				break;
			case s_right:
				new_col++;
				break;
			case s_up:
				new_row--;
				break;
			case s_down:
				new_row++;
				break;
			default:
				printf("Invalid\n");
				return invalid;
		}
		printf("old head (%i,%i), new head (%i,%i))\n", row, col, new_row, new_col);
		if(new_row >= NUMROW_LED || new_col >= NUMCOL_LED) {
			printf("Out of bounds.\n");
			return invalid;
		}
		else if(gameBoard[new_row][new_col] == food) {
			printf("+1\n");
			snakeLength++;
			incrLength = 1;
			gameBoard[new_row][new_col] = dir;
			return gameBoard[row][col]; // return the old direction to the callee before callee overwrites the position	

		}
		 else if(gameBoard[new_row][new_col] == empty) {
			gameBoard[new_row][new_col] = dir;	
			return gameBoard[row][col];  // return the old direction to the callee before callee overwrites the position	
		}
		 else {
			printf("Snake crashed into itself.\n");	
		}	
		return invalid;	
	}	

	// obtain the direction of this position
	enum object new_dir;

	switch(gameBoard[row][col]) {
		case s_left:
			new_dir	= moveSnake(count++, row, col--, dir);	
			break;
		case s_right:
			new_dir = moveSnake(count++, row, col++, dir);	
			break;
		case s_up:	
			new_dir = moveSnake(count++, row--, col, dir);	
			break;
		case s_down:
			new_dir = moveSnake(count++, row++, col, dir);	
			break;
		default:
			printf("Invalid\n");
			break;
	}
	
	if(new_dir == invalid) return invalid;
	enum object old_dir = gameBoard[row][col]; // obtain the old position before it is overwritten
	
	// calculate the new position to move to 
	int new_row = row;
	int new_col = col;
	switch(new_dir) {
		case s_left:
			new_col--;
			break;
		case s_right:
			new_col++;
			break;
		case s_up:
			new_row--;
			break;
		case s_down:
			new_row++;
			break;
		default:
			printf("Invalid\n");
			return invalid;
	}

	gameBoard[new_row][new_col] = new_dir;
	return old_dir;
}

void updateBoard(enum object dir) {
	printf("updateBoard() dir = ");
	switch(dir) {
		case s_left:
			printf("left\n");
			break;
		case s_right:
			printf("right\n");
			break;
		case s_up:
			printf("up\n");
			break;
		case s_down:
			printf("down\n");
			break;
		default:
			printf("Invalid!\n");
			return;
	}

	enum object new_dir = moveSnake(1, tail_pos[0], tail_pos[1], dir);
	if(new_dir == invalid) return;

	int new_row = tail_pos[0];
	int new_col = tail_pos[1];
	
	switch(new_dir) {
		case s_left:
			new_col--;
			break;
		case s_right:
			new_col++;
			break;
		case s_up:
			new_row--;
			break;
		case s_down:
			new_row++;
			break;
		default:
			printf("Invalid\n");
			break;
	}
	gameBoard[new_row][new_col] = new_dir;
	
	if(incrLength){	
		// reset
		incrLength = 0;
		addFood();	
	} else {
		// empty the snake at the current tail position
		gameBoard[tail_pos[0]][tail_pos[1]] = empty;
		// update tail position
		tail_pos[0] = new_row;
		tail_pos[1] = new_col;	
	}
}

void clearBoard() {
	// set all positions to empty 
	for(int i=0; i<NUMROW_LED; i++) {
		for(int j=0; j<NUMCOL_LED; j++) {
			gameBoard[i][j] = empty;
		}
	}
}

void newGame() {

	// init all positions on gameBoard to empty
	clearBoard();
			
	// place the snake at the bottom left-most position
	gameBoard[7][0] = s_up;
	// update the location of the snake's tail
	tail_pos[0] = 7;
	tail_pos[1] = 0;
	// reset length
	snakeLength = 1;
	
	// place food on random position on board
	addFood();

	printf("Initialized board\n");	
	printBoard();

}

int main() {

	time_t t;
	srand( (unsigned) time(&t) ); // init random number generator

	newGame();
	enum object dirs[8] = {s_up, s_up, s_up, s_right, s_right, s_down, s_down, s_left};
	
	for(int i=0; i<sizeof(dirs)/sizeof(enum object); i++) {
		updateBoard(dirs[i]);
		printBoard();
	}
	
	return 0;	
}
