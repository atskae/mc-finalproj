/*

    Microcontrollers SoSe 18 Final Project: Snake

*/

#include "tm4c1294ncpdt.h" // microcontroller components
#include "stdlib.h"
#include "stdio.h"
#include "time.h" // to generate random positions in the gameBoard
#include "string.h"
#include "math.h"

#include "int_handler.h" // custom interrupt handler

/*interrupt variables*/
//UART
volatile unsigned char gucRxChar = 0;

// RL Signal from Port L(0)
volatile unsigned char displayed_intro = 0;
volatile unsigned char left_edge = 0; // used only to display the intro screen
volatile unsigned char game_over = 0;

#define PORTD 3
#define PORTK 9
#define PORTL 10
#define PORTM 11
#define PORTP 13

char display[NUMCOL_LED]; // each element represent a column in the LED pendulum; msut convert gameBoard to display
object gameBoard[NUMROW_LED][NUMCOL_LED];
int snakeLength = 1;
int tail_pos[2] = {7, 0}; // start at bottom left-most position on the screen
int food_pos[2] = {0, 0}; // position of food ; variable used to remove food in constant time
char incrLength = 0; // if snake gets food, this is set to "1"
//char initial_screen[STREAMSIZE] = {0x00, 0x00, 0xFF, 0xA0, 0xE0, 0x00, 0xFF, 0xA0,
//                                0xF0, 0x0F, 0x00, 0xFF, 0x91, 0x91, 0x00, 0xF1, 0x91, 0x9F,
//                                0x00, 0xF1, 0x91, 0x9F, 0x00, 0x00, 0xFF, 0x40, 0x00};

/*

    Microcontroller components

*/

void portConfig() {

    // Port M controls the pendulum display
    SYSCTL_RCGCGPIO_R |= (1 << PORTM); // enable clock for port M
    while (!(SYSCTL_PRGPIO_R & (1 << PORTM))); // wait for port to be ready
    // 0:input 1:output
    GPIO_PORTM_DIR_R |= 0xFF;  // PM(7:0) output
    GPIO_PORTM_DEN_R |= 0xFF;  // enable pins PM(7:0)

    // Port L(0) controls the left/right signals of the pendulum and the 7-segment display
    SYSCTL_RCGCGPIO_R |= (1 << PORTL); // enable clock for port L
    while (!(SYSCTL_PRGPIO_R & (1 << PORTL))); // wait for port to be ready
    GPIO_PORTL_DIR_R &= ~0x01;  // PL(0) input (reads R/L signal)
    GPIO_PORTL_DEN_R |= 0x01;  // enable pin(0)
    // configure interrupts for Port L
    GPIO_PORTL_IM_R &= 0x00; // clear to config interrupts
    GPIO_PORTL_IS_R = 0x0; // interrupt sense ; "0" edge-sensitive, "1" level-sensitive
    GPIO_PORTL_IBE_R = 0x1; // "1" interrupt both edges ; "0" single-edge
    GPIO_PORTL_ICR_R |= (1<<0); // interrupt clear
    GPIO_PORTL_RIS_R = 0x0;
    GPIO_PORTL_IM_R |= (1<<0); // allows interrupt to be sent to interrupt controller
    NVIC_EN1_R |= (1<<(53-32));

    // Port K is a buffer that holds the score (which is sent to the 7-segment display)
    SYSCTL_RCGCGPIO_R |= (1 << PORTK); // enable clock
    while (!(SYSCTL_PRGPIO_R & (1 << PORTK))); // wait for port to be ready
    // 0:input 1:output
    GPIO_PORTK_DIR_R |= 0xFF;  // output
    GPIO_PORTK_DEN_R |= 0xFF;  // enable pins

    // Port D(1:0) are the enable bits for the 7-segment display
    SYSCTL_RCGCGPIO_R |= (1 << PORTD); // enable clock for port D
    while (!(SYSCTL_PRGPIO_R & (1 << PORTD))); // wait for port to be ready
    // "0" = input ; "1" = output
    GPIO_PORTD_AHB_DIR_R |= 0x3;  // PD(1:0) output
    GPIO_PORTD_AHB_DEN_R |= 0x3;  // enable pins PD(1:0)

}

void timerConfig() {

    SYSCTL_RCGCTIMER_R |= 0x1; // activate timer module 0
    while(!(SYSCTL_PRTIMER_R & 0x1)); // wait to be ready

    // Timer 0A
    TIMER0_CFG_R   |= 0x4; // 16 bit
    TIMER0_CTL_R   &= ~0x1; // disable timer A for config
    TIMER0_TAMR_R  = 0x1; // down, one-shot
    TIMER0_TAPR_R  = PRESCALER; // prescaler
}

void UARTConfig() {
    // UART config ; must use UART Module 6
    // Port P(0) receives, P(1) transmits (datasheet, p.1164)
    SYSCTL_RCGCGPIO_R |= (1 << PORTP); // enable clock
    while (!(SYSCTL_PRGPIO_R & (1 << PORTP))); // wait for port to be ready
    // 0:input 1:output
    GPIO_PORTP_DIR_R |= 0x0; // pin 0 recieves ; input
    GPIO_PORTP_DEN_R |= 0x1; // disable digital enable
    GPIO_PORTP_AFSEL_R |= 0xFF; // enable alternate function select
    GPIO_PORTP_PCTL_R |= 0x11; // assign pins to UART signal

    // configure UART Module 6
    SYSCTL_RCGCUART_R |= 0x40;// "UART Clock Gating Control"
    while(!(SYSCTL_PPUART_R & (1 << 6))); // wait until UART module 6 is ready
    UART6_CTL_R &= ~0x01; // disable UART module for configuration
    UART6_IBRD_R = 104; // set the integer portion of BRD ; for bit-rate = 9600
    UART6_FBRD_R = 10; // set fractional portion of BRD
    UART6_LCRH_R |= 0x66; // UART line control ; set data length to 8 bits, enable FIFOs and parity bit, 1 stop bit
    UART6_IM_R |= 0x10; // enable interrupt when UART receives a value from the keyboard ; clear interrupt with UART6_ICR_R
    UART6_CTL_R |= 0x201; // enable UART module
    NVIC_EN1_R |= (1<<(59-32)); // enable PORTP interrupt in NVIC
}

// Configures GPIO ports, timers, and UART ; enables interrupts
void sysConfig() {
    time_t t;
    srand( (unsigned) time(&t) ); // init random number generator

    portConfig();
    timerConfig();
    UARTConfig();
}

// Display game score on 4 digit 7-segment display
// Port D(1:0) are the enable bits for the 7-segment display
// Port K serves as a buffer to hold the score for the 7-segment display
void displayValue(int score) {
    printf("score %i\n", score);
    int digits[4] = {0, 0, 0, 0}; // initialize to 0
    int i, scoreAux = score;

    // Get digits
    for (i = 4; i > 0; i--) {
        digits[i-1] = scoreAux % 10;
        scoreAux = scoreAux / 10;
    }

    // Output digits

    GPIO_PORTK_DATA_R = 0x00;

    // First 2 digits
    // Enable: PL(1) = EN(1) := H
    GPIO_PORTD_AHB_DATA_R |= (1 << 1);
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
    GPIO_PORTD_AHB_DATA_R &= ~(1 << 1);
    GPIO_PORTK_DATA_R = 0x00; // clear display value

    // Last 2 digits
    // Enable: PL(0) = EN(0) := H
    GPIO_PORTD_AHB_DATA_R |= (1 << 0);
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
    GPIO_PORTD_AHB_DATA_R &= ~(1<<0);
}

/*

    Snake Game Logic

*/

void printDir(object dir) {

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
    int shift, col, row;

    for(col=0; col<NUMCOL_LED; col++) { // must iterate by column since each char in the result array represents a column in the LED
        d = 0x00; // reset
        shift = 0;
        for(row=NUMROW_LED-1; row>=0; row--) { // start from the bottom of each column
            if(gameBoard[row][col] == empty) d &= ~(0x1 << shift); // clear the bit
            else d |= (0x1 << shift); // set the bit
            shift++;
        }
        display[col] = d;
    }

//    // test print
//    for(shift=NUMROW_LED-1; shift>=0; shift--) {
//        for(col=0; col<NUMCOL_LED; col++) { // for each bit
//            if(display[col] & (0x1 << shift)) printf("*");
//            else printf(" ");
//        }
//        printf("\n");
//    }

}

void printBoard() {

    printf("-----\n");
    printf("s_left %i, s_right %i, s_up %i, s_down %i\n", s_left, s_right, s_up, s_down);
    printf("length %i, tail_pos (row=%i, col=%i), food_pos (row=%i, col=%i)\n", snakeLength, tail_pos[0], tail_pos[1], food_pos[0], food_pos[1]);
    int i, j;
    for(i=0; i<NUMROW_LED; i++) {
        for(j=0; j<NUMCOL_LED; j++) {
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
object moveSnake(int count, int row, int col, object dir) {

//    printf("moveSnake() count=%i, row=%i, col=%i\n", count, row, col);
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
        //printf("old head (%i,%i), new head (%i,%i))\n", row, col, new_row, new_col);
        if(new_row >= NUMROW_LED || new_row < 0 || new_col < 0 || new_col >= NUMCOL_LED) {
            printf("Out of bounds.\n");
            return invalid;
        }
        else if(gameBoard[new_row][new_col] == food) {

            printf("+1\n");
            //snakeLength++;
            displayValue(++snakeLength); // display new score onto 7-segment display
            incrLength = 1;
            gameBoard[new_row][new_col] = dir;
            //return dir;
            if(gameBoard[row][col] != dir) {
                gameBoard[row][col] = dir; // if the direction is different from the previous
                return dir;
            }
            else return gameBoard[row][col];  // return the old direction to the callee before callee overwrites the position
        }
         else if(gameBoard[new_row][new_col] == empty) {
            gameBoard[new_row][new_col] = dir;
            if(gameBoard[row][col] != dir) {
                gameBoard[row][col] = dir; // if the direction is different from the previous
                return dir;
            }
            else return gameBoard[row][col];  // return the old direction to the callee before callee overwrites the position
        }
         else {
            printf("Snake crashed into itself.\n");
            return invalid;
        }
    }

    // obtain the direction of this position
    object new_dir;

    switch(gameBoard[row][col]) {
        case s_left:
            new_dir = moveSnake(count+1, row, col-1, dir);
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
    object old_dir = gameBoard[row][col]; // obtain the old position before it is overwritten
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

void updateBoard(object dir) {
    //printf("updateBoard() dir = ");
    //printDir(dir);

    object new_dir = moveSnake(1, tail_pos[0], tail_pos[1], dir);
    if(new_dir == invalid) {
        game_over = 1;
        return;
        //exit(0);
    }
 
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
        // printf("tail setting (%i %i) empty\n", tail_pos[0], tail_pos[1]);
        gameBoard[tail_pos[0]][tail_pos[1]] = empty;
        // update tail position
        tail_pos[0] = new_row;
        tail_pos[1] = new_col;
    }
}

void clearBoard() {
    // set all positions to empty
    int i, j;
    for(i=0; i<NUMROW_LED; i++) {
        for(j=0; j<NUMCOL_LED; j++) {
            gameBoard[i][j] = empty;
        }
    }
}

void initialScreen() {
     while (1) {
         if(gucRxChar == '1')  break;
    } // while ; end
}

void newGame() {

    initialScreen();
    GPIO_PORTM_DATA_R = 0x0;
    displayed_intro = 1;

    // init all positions on gameBoard to empty
    clearBoard();
    // place the snake at the bottom left-most position
    gameBoard[7][0] = s_up;
    // update the location of the snake's tail
    tail_pos[0] = 7;
    tail_pos[1] = 0;
    // reset length
    snakeLength = 1;
    displayValue(snakeLength);

    // place food on random position on board
    addFood();
    gameBoard[food_pos[0]][food_pos[1]] = food;

    printf("Initialized board\n");
    //printBoard();
    convertBoard();

}

int main(void) {

    sysConfig();
    newGame();

    while(1) {

    }
}
