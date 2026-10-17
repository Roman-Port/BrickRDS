#ifndef INC_CORE_UTIL_TUNER_H_
#define INC_CORE_UTIL_TUNER_H_

#include <stdint.h>

extern uint16_t tuner_sample; // The raw sample (after noise reduction) of the VFO reading
extern int tuner_current_channel; // The current FM channel where 0 is 87.9. -1 if invalid.
extern int tuner_last_changed; // Tick where the channel was last changed
extern uint16_t tuner_diag_min; // Min value, resettable with tuner_diag_reset
extern uint16_t tuner_diag_max; // Max value, resettable with tuner_diag_reset
extern int tuner_diag_reset; // Set to clear diag

// Recalculates the bounds table using the calibration data
void tuner_recalc_table();

// Samples the ADC and updates the current reading
void tuner_update(uint16_t sample);

// Gets the lower/upper raw ADC bounds of the specified channel.
void tuner_get_bounds(int channel, uint16_t* lower, uint16_t* upper);

// Gets a renderable frequency and writes it into (at least 5 character) result.
void tuner_get_channel_freq(int channel, char* result);

// Used for reading stats; Gets number of samples per second
uint32_t tuner_get_samples_per_sec();

#endif /* INC_CORE_UTIL_TUNER_H_ */
