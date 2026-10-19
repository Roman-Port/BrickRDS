// Handles ADC related tasks

#include "main.h"
#include <core/util/tuner.h>
#include <core/util/backlight.h>

static int current_channel = 0;

// Called when ADC value was read
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
	//Read sample
	uint16_t reading = HAL_ADC_GetValue(&hadc1);

	//Dispatch
	switch (current_channel) {
	case 0: // VFO
		tuner_update(reading);
		break;
	case 1: // Backlight
		backlight_update(reading);
		break;
	}

	//Update counter
	current_channel = (current_channel + 1) % 2;
}

// Enables ADC scanning
void adc_init() {
	current_channel = 0;
	HAL_ADCEx_Calibration_Start(&hadc1);
	HAL_ADC_Start_IT(&hadc1);
}
