#ifndef INT_HANDLER_H_
#define INT_HANDLER_H_

extern volatile unsigned char displayed_intro;

//UART
extern volatile unsigned char gucNewData;
extern volatile unsigned char gucRxChar;
//RL Signal
extern volatile unsigned char left_edge;

//timer
extern volatile unsigned char time_out;
extern volatile unsigned char update_board;

#define TMAX 0.01
#define FCLK 16.0E6 // CPU frequency
#define PRESCALER 2

typedef enum {empty, s_left, s_right, s_up, s_down, food, invalid} object;

#define NUMCOL_LED 36
extern char display[NUMCOL_LED];

void IntHandlerUART6(void);
void IntHandlerRLSignal(void);

extern void updateBoard(object dir);
extern void convertBoard();
void timerWait(unsigned short usec);

#endif
