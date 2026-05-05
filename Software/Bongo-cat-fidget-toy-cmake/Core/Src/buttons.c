#include "buttons.h"

volatile int sw_state_left;
volatile int sw_state_right;

volatile uint8_t sw_released_left;
volatile uint8_t sw_released_right;

static uint8_t prev_left  = 0;
static uint8_t prev_right = 0;

void readPins(void) {
    uint8_t left  = !HAL_GPIO_ReadPin(SW_LEFT_GPIO_Port,  SW_LEFT_Pin);
    uint8_t right = !HAL_GPIO_ReadPin(SW_RIGHT_GPIO_Port, SW_RIGHT_Pin);

    if (prev_left  && !left)  sw_released_left  = 1;
    if (prev_right && !right) sw_released_right = 1;

    prev_left  = left;
    prev_right = right;

    sw_state_left  = left;
    sw_state_right = right;
}
