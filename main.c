/*

	Microcontrollers SoSe 18 Final Project: Snake	

*/

#include "inc/tm4c1294ncpt.h" // microcontroller components
#include "stdlib.h"
#include "stdio.h"
#include "time.h" // to generate random positions in the gameBoard
#include "string.h"

#define PORTK 9
#define PORTL 10
#define PORTM 11

#define TMAX 0.01
#define FCLK 16.0E6 // CPU frequency
#define PRESCALER 2

#define SEGMENT 1000 // us

#define S // S(Snake) I(initial screen)
#define CHARACTERSIZE 5
#define STREAMSIZE 29

#define NUMROW_LED 8 // number of rows in the LED pendulum display 
#define NUMCOL_LED 20 // number of columns in the LED pendulum display

char display[NUMCOL_LED]; // each element represent a column in the LED pendulum; msut convert gameBoard to display
enum object{empty, s_left, s_right, s_up, s_down, food, invalid}; // invalid is used moveSnake() if collision occurs
enum object gameBoard[NUMROW_LED][NUMCOL_LED];
int snakeLength = 1;
int tail_pos[2] = {7, 0}; // start at bottom left-most position on the screen
int food_pos[2] = {0, 0}; // position of food ; variable used to remove food in constant time
char incrLength = 0; // if snake gets food, this is set to "1"

/*

	Microcontroller Configurations

*/

void sysConfig() {

	// Port M controls the pendulum display	
	SYSCTL_RCGCGPIO_R |= (1 << PORTM); // enable clock for port M
	while (!(SYSCTL_PRGPIO_R & (1 << PORTM))); // wait for port to be ready
	// 0:input 1:output
	GPIO_PORTM_DIR_R |= 0xFF;  // PM(7:0) input
	GPIO_PORTM_DEN_R |= 0xFF;  // enable pins PM(7:0)

	// Port L controls the left/right signals of the pendulum and the 7-segment display	
	SYSCTL_RCGCGPIO_R |= (1 << PORTL); // enable clock for port L
	while (!(SYSCTL_PRGPIO_R & (1 << PORTL))); // wait for port to be ready
	GPIO_PORTL_DIR_R |= 0x03;  // PL(0:1) output to the 7-segment display
	GPIO_PORTL_DIR_R &= ~0x04;  // PL(2) input (reads R/L signal)
	GPIO_PORTL_DEN_R |= 0x07;  // enable pins (3:0)	
	// configure interrupts for Port L
	GPIO_PORTL_AHB_IS_R &= ~0x01; // interrupt sense ; "0" edge-sensitive, "1" level-sensitive	
	GPIO_PORTL_AHB_IBE_R &= ~0x1; // interrupt both edges ; "0" single-edge
	GPIO_PORTL_AHB_IEV_R |= 0x01; // interrupt event ; "1" rising edges
	GPIO_PORTL_AHB_ICR_R |= 0x01; // interrupt clear
	GPIO_PORTL_AHB_IM_R |= 0x01; // mask register
	NVIC_END0_R |= (1<<PORTL); // enable PORTM interrupt in NVIC 

	// Port K is a buffer that holds the score (which is sent to the 7-segment display)
	SYSCTL_RCGCGPIO_R |= (1 << PORTK); // enable clock
	while (!(SYSCTL_PRGPIO_R & (1 << PORTK))); // wait for port to be ready
	// 0:input 1:output
	GPIO_PORTK_DIR_R |= 0xFF;  // output 
	GPIO_PORTK_DEN_R |= 0xFF;  // enable pins

	// Add UART config here	
}

// up to 10SEGMENT 
void timerConfig() {

    SYSCTL_RCGCTIMER_R |= 0x1; // activate timer clock
    while(!(SYSCTL_PRTIMER_R & 0x1)); // wait to be ready
    TIMER0_CTL_R   &= ~0x1; // stop timer for config
    TIMER0_CFG_R   |= 0x4; // 16 bit
    TIMER0_TAMR_R  |= 0x1; // down, one-shot
    TIMER0_TAPR_R  |= PRESCALER; // prescaler

}

void timerWait(unsigned short usec) {
	
	unsigned short loadValue = (unsigned short) ceil((usec * 1.0e-6 * FCLK) / (PRESCALER+1)) - 1;
	TIMER0_TAILR_R = loadValue; // set interval load value
	TIMER0_CTL_R   |= 0x1; // start timer
	while(!(TIMER0_RIS_R & (1<<0))); // wait for time-out, blocking
	TIMER0_ICR_R |= (1<<0); // clear interrupt flag
	TIMER0_CTL_R &= ~0x1; // disable timer

}

// Display game score on 4 digit 7-segment display
// Port L(1:0) are the enable bits for the 7-segment display
// Port K serves as a buffer to hold the score for the 7-segment display 
void displayValue(int score) {
    int digits[4] = {0}; // initialize to 0
    int i, scoreAux = score;

    // Get digits
    for (i = 4; i > 0; i--) {
        digits[i-1] = scoreAux % 10;
        scoreAux = scoreAux / 10;
    }

    // Output digits

    // First 2 digits
    // Enable: PL(1) = EN(1) := H
    GPIO_PORTL_DATA_R |= (1 << 1);
    // Set data
    if (score >= 1000) {
        //printf("digit[3] %d \n", digits[3]);
        GPIO_PORTK_DATA_R |= digits[0] << 4;
    }
    if (score >= 100) {
        //printf("digit[2] %d \n", digits[2]);
        GPIO_PORTK_DATA_R |= digits[1];
    }

    // Latch mode (disable)
    GPIO_PORTL_DATA_R &= ~(1 << 1);
    GPIO_PORTK_DATA_R = 0x00;

    // Last 2 digits
    // Enable: PL(0) = EN(0) := H
    GPIO_PORTL_DATA_R |= (1 << 0);
    // Set data
    if (score >= 10) {
        //printf("digit[1] %d \n", digits[1]);
        GPIO_PORTK_DATA_R |= digits[2] << 4;
    }
    if (score >= 0) {
        //printf("digit[0] %d \n", digits[0]);
        GPIO_PORTK_DATA_R |= digits[3];
    }
    // Latch mode (disable)
    GPIO_PORTL_DATA_R &= ~(1<<0);
}

/*

	Snake Game Logic

*/

void printDir(enum object dir) {

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
			printf("Invalid direction %i\n", dir);
			return;
	}


}

// converts the matrix into a char array
void convertBoard() {

	char d; // a single column on the LED
	t shift;

	for(int col=0; col<NUMCOL_LED; col++) { // must iterate by column since each char in the result array represents a column in the LED
		d = 0x00; // reset
		shift = 0;			
		for(int row=NUMROW_LED-1; row>=0; row--) { // start from the bottom of each column
			if(gameBoard[row][col] == empty) d &= ~(0x1 << shift); // clear the bit
			else d |= (0x1 << shift); // set the bit	
			shift++;
		}
		display[col] = d;
	}

	// test print
	for(int shift=NUMROW_LED-1; shift>=0; shift--) {
		for(int col=0; col<NUMCOL_LED; col++) { // for each bit
			if(display[col] & (0x1 << shift)) printf("*");
			else printf(" ");	
		}	
		printf("\n");
	}

}

void printBoard() {

	printf("-----\n");	
	printf("s_left %i, s_right %i, s_up %i, s_down %i\n", s_left, s_right, s_up, s_down);
	printf("length %i, tail_pos (row=%i, col=%i), food_pos (row=%i, col=%i)\n", snakeLength, tail_pos[0], tail_pos[1], food_pos[0], food_pos[1]);
	for(int i=0; i<NUMROW_LED; i++) {
		for(int j=0; j<NUMCOL_LED; j++) {
			switch(gameBoard[i][j]) {
				case empty:
					printf("  ");
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
					printf("v ");
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

// count = number of pixels of the snake that we have seen already; (row, col) = current location on board
enum object moveSnake(int count, int row, int col, enum object dir) { 

	printf("moveSnake() count=%i, row=%i, col=%i\n", count, row, col);
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
//		printf("old head (%i,%i), new head (%i,%i))\n", row, col, new_row, new_col);
		if(new_row >= NUMROW_LED || new_row < 0 || new_col < 0 || new_col >= NUMCOL_LED) {
			printf("Out of bounds.\n");
			return invalid;
		}
		else if(gameBoard[new_row][new_col] == food) {
			
			printf("+1\n");
			snakeLength++;
			incrLength = 1;
			gameBoard[new_row][new_col] = dir;
//			return gameBoard[row][col]; // return the old direction to the callee before callee overwrites the position	
			return dir;			
		}
		 else if(gameBoard[new_row][new_col] == empty) {
			gameBoard[new_row][new_col] = dir;
//			if(gameBoard[row][col] != dir) {
				gameBoard[row][col] = dir; // if the direction is different from the previous
				return dir;
//			}
//			else return gameBoard[row][col];  // return the old direction to the callee before callee overwrites the position	
		}
		 else {
			printf("Snake crashed into itself.\n");	
			return invalid;	
		}	
	}	

	// obtain the direction of this position
	enum object new_dir;

	switch(gameBoard[row][col]) {
		case s_left:
			new_dir	= moveSnake(count+1, row, col-1, dir);	
			break;
		case s_right:
			new_dir = moveSnake(count+1, row, col+1, dir);	
			break;
		case s_up:	
			new_dir = moveSnake(count+1, row-1, col, dir);	
			break;
		case s_down:
			new_dir = moveSnake(count+1, row+1, col, dir);	
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

	if(old_dir != new_dir) {
		switch(old_dir) {
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

	} else {
	
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
	
	}

	gameBoard[new_row][new_col] = new_dir;
	return old_dir;
}

void updateBoard(enum object dir) {
	printf("updateBoard() dir = ");
	printDir(dir);

	enum object new_dir = moveSnake(1, tail_pos[0], tail_pos[1], dir);
	if(new_dir == invalid) exit(0);

	//	gameBoard[new_row][new_col] = new_dir;
	
	if(incrLength){	
		// reset
		incrLength = 0;
		addFood();	
	} else {
		// update tail	
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
		}

		// empty the snake at the current tail position
//		printf("tail setting (%i %i) empty\n", tail_pos[0], tail_pos[1]);
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
//	gameBoard[food_pos[0]][food_pos[1]] = food;

	printf("Initialized board\n");	
//	printBoard();
	convertBoard();
}

int main(void) {

	sysConfig(); // configure ports, UART, and their interrupts
	timerConfig();

	time_t t;
	srand( (unsigned) time(&t) ); // init random number generator

	newGame();
	
	while(1) {
		// waits for interrupts	
	}
//	enum object dirs[] = {s_up, s_up, s_up, s_right, s_right, s_down, s_down, s_left, s_left, s_left};
//
//	printf("Snake, los gehts!\n");
//	for(int i=0; i<sizeof(dirs)/sizeof(enum object); i++) {
//		updateBoard(dirs[i]);
//		printBoard();
//	}
//
//	char input[10];
//	while(1) {
//		printf("type: j=left, i=up, l=left, k=down, q=quit\n");
//		scanf("%s", input);
//		if(strcmp(input, "i") == 0) updateBoard(s_up);
//		else if(strcmp(input, "k") == 0) updateBoard(s_down);	
//		else if(strcmp(input, "j") == 0) updateBoard(s_left);
//		else if(strcmp(input, "l") == 0) updateBoard(s_right);	
//		else if(strcmp(input, "q") == 0) break;
//		else printf("%s is invalid.\n", input);	
//		//printBoard();
//		convertBoard();
//	}
	
}
