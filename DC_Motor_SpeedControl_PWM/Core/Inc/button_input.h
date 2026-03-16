
#ifndef __BUTTON_INPUT_H
#define __BUTTON_INPUT_H

typedef enum {
    BUTTON_UP,
    BUTTON_DOWN,
    BUTTON_LEFT,
	BUTTON_RIGHT,
	BUTTON_SELECT,
	NONE
} Button;

void ADC_ButtonInit(void);
Button GetButton(void);

#endif
