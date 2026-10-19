#ifndef INC_CORE_UTIL_BACKLIGHT_H_
#define INC_CORE_UTIL_BACKLIGHT_H_

#include <stdint.h>

extern uint16_t backlight_reading; // Latest input sample
extern uint16_t backlight_brightness; // The latest PWM output

// Updates the table for backlight brightness
void backlight_recalc_table();

// Pushes a new ADC sample into the process
void backlight_update(uint16_t sample);

#endif /* INC_CORE_UTIL_BACKLIGHT_H_ */
