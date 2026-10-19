// Handles converting the analog light signal to PWM to control the backlight

#include <main.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <core/util/settings.h>
#include <core/util/backlight.h>

#define NR_SAMPLES 64

static uint32_t sample_sum = 0; // Added to every sample
static uint16_t brightness_table[256]; // Calculated table for
static uint16_t samples_in_sum = 0; // Count of samples in sample_sum

uint16_t backlight_reading = 0; // Latest input sample
uint16_t backlight_brightness = 0; // The latest PWM output

// Sets the final output PWM and updates internal state
static void update_pwm_timer(uint16_t result) {
	TIM3->CCR1 = (uint32_t)result;
	backlight_brightness = result;
}

static void process_backlight(uint16_t avg) {
	//Store in public register
	backlight_reading = avg;

	//Update timer for PWM, reading from the precalculated table (discarding lowest 8 bits)
	update_pwm_timer(brightness_table[avg >> 8]);
}

// Updates the table for backlight brightness
void backlight_recalc_table() {
	for (int i = 0; i < 256; i++)
		brightness_table[i] = i << 8;
}

// Pushes a new ADC sample into the process
void backlight_update(uint16_t sample) {
	//Push into the running sum
	sample_sum += sample;

	//Add to stat and once it reaches limit, calculate and submit average
	if (samples_in_sum++ >= NR_SAMPLES) {
		//Calculate average and submit
		process_backlight(sample_sum >> 6);

		//Reset
		sample_sum = 0;
		samples_in_sum = 0;
	}
}
