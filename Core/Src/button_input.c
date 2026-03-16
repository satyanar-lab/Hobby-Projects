#include "main.h"         // includes hadc1
#include "button_input.h"   // your header


extern ADC_HandleTypeDef hadc;

void ADC_ButtonInit(void) {
    HAL_ADC_Start(&hadc);
}

Button GetButton(void) {
    uint32_t sum = 0;

    HAL_ADC_Start(&hadc);
    HAL_ADC_PollForConversion(&hadc, HAL_MAX_DELAY);

    for (int i = 0; i < 10; i++) {
        sum += HAL_ADC_GetValue(&hadc);
        HAL_Delay(2); // small delay to avoid reading the exact same sample
    }

    uint32_t avg = sum / 10;

    if (avg < 50) return BUTTON_RIGHT;
    else if (avg < 1100) return BUTTON_UP;
    else if (avg < 1900) return BUTTON_DOWN;
    else if (avg < 2700) return BUTTON_LEFT;
    else if (avg < 3400) return BUTTON_SELECT;
    else return NONE;
}
