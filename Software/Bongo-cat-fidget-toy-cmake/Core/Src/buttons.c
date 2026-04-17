#include "buttons.h"

int sw_state_left;
int sw_state_right;

void readPins(void) {
    sw_state_left  = !HAL_GPIO_ReadPin(SW_LEFT_GPIO_Port,  SW_LEFT_Pin);
    sw_state_right = !HAL_GPIO_ReadPin(SW_RIGHT_GPIO_Port, SW_RIGHT_Pin);
}
