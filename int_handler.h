#ifndef INT_HANDLER_H
#define INT_HANDLER_H

extern volatile unsigned char gucNewData;
extern volatile unsigned char gucRxChar;

void IntHandlerUART2 (void);

#endif