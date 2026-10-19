// Handles reading of the VFO to determine the current frequency
// The Ford radio uses a 4th degree polynomial that we interpolate using the calibration data

#include <main.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <core/util/settings.h>
#include <core/util/tuner.h>

#define NR_SAMPLES 128

static uint16_t bounds_table[102]; // Contains the calculated lower bounds of the ADC voltage reading for channels 87.9-108.1 (last one is upper bound of 107.9); Calculated by tuner_recalc_table();
static uint16_t latest_samples[NR_SAMPLES];
static int samples_since_update = 0;
static uint32_t start_time = 0;
static uint32_t total_samples = 0;

uint16_t tuner_sample; // The raw sample (after noise reduction) of the VFO reading
int tuner_current_channel = -1; // The current FM channel where 0 is 87.9. -1 if invalid.
int tuner_last_changed = 0; // Tick where the channel was last changed
uint16_t tuner_diag_min = 0; // Min value, resettable with tuner_diag_reset
uint16_t tuner_diag_max = 0; // Max value, resettable with tuner_diag_reset
int tuner_diag_reset = 0; // Set to clear diag

// I'll be honest I don't understand how this works. But it takes the calibration data, y, and calculates the voltage at target, tar.
static double lagrange_interpolate(const uint16_t* y, double tar) {
	double x[] = { 0.0, 0.25, 0.5, 0.75, 1.0 };
    double result = 0.0;
    for (int j = 0; j < 5; j++)
    {
        double basis = 1.0;

        //Calculate the basis polynomial L_j(x)
        for (int i = 0; i < 5; i++)
        {
            if (i != j)
                basis *= (tar - x[i]) / (x[j] - x[i]);
        }

        //Add current term y_j * L_j(x) to total sum
        result += y[j] * basis;
    }
    return result;
}

// Recalculates the current channel, if any, and updates info
static void calculate_channel(uint16_t sample) {
	//Check if above min but below max
	if (sample > bounds_table[0] && sample < bounds_table[101]) {
		//Scan
		int nextCh = 1;
		for (int i = 0; i < 101; i++) {
			if (sample < bounds_table[nextCh++]) {
				tuner_last_changed = HAL_GetTick();
				tuner_current_channel = i;
				return;
			}
		}
	}

	//Failed (should never land here...)
	tuner_last_changed = HAL_GetTick();
	tuner_current_channel = -1;
}

// Recalculates the bounds table using the calibration data
void tuner_recalc_table() {
	//Calculate the lower bounds of 101 channels (the last one is the upper bound of 107.9)
	double channelNormalized;
	for (int channel = 0; channel < 102; channel++) {
		channelNormalized = (double)channel / 100; // 0-1 (inclusive)
		bounds_table[channel] = (uint16_t)lagrange_interpolate(user_settings.vfo_calibration, channelNormalized - 0.005);
	}
}

// Samples the ADC and updates the current reading
void tuner_update(uint16_t sample) {
	//Init/update stat counter
	if (total_samples++ == 0) {
		start_time = HAL_GetTick();
	}

	//Store
	latest_samples[samples_since_update++] = sample;

	//Check if it's time to update
	if (samples_since_update >= NR_SAMPLES) {
		//Sum and average
		uint32_t sum = 32768; // used to round at final step
		for (int i = 0; i < NR_SAMPLES; i++)
			sum += latest_samples[i];

		//Get sample
		tuner_sample = sum >> 7;

		//Reset counter
		samples_since_update = 0;

		//Diag
		int diagReset = tuner_diag_reset;
		if (tuner_sample < tuner_diag_min || diagReset)
			tuner_diag_min = tuner_sample;
		if (tuner_sample > tuner_diag_max || diagReset)
			tuner_diag_max = tuner_sample;
		if (diagReset)
			tuner_diag_reset = 0;

		//Handle differently if we already have a channel or not
		sample = tuner_sample;
		if (tuner_current_channel < 0) {
			//No channel selected; Check if sample is in range and only then attempt to resolve it
			if (sample >= bounds_table[0] && sample < bounds_table[101])
				calculate_channel(sample);
		} else {
			//Check if we've gone out of range of the current channel, otherwise stay on this channel
			if (sample < bounds_table[tuner_current_channel] || sample > bounds_table[tuner_current_channel + 1])
				calculate_channel(sample);
		}
	}
}

// Gets the lower/upper raw ADC bounds of the specified channel.
void tuner_get_bounds(int channel, uint16_t* lower, uint16_t* upper) {
	//Validate
	assert(channel >= 0 && channel <= 101);

	//Read
	*lower = bounds_table[channel];
	*upper = bounds_table[channel + 1];
}

// Gets a renderable frequency and writes it into (at least 5 character) result.
void tuner_get_channel_freq(int channel, char* result) {
	int i = 879 + (channel * 2);
	sprintf(result, "%i.%i", i / 10, i % 10);
}

// Used for reading stats; Gets number of samples per second
uint32_t tuner_get_samples_per_sec() {
	return (total_samples * 1000) / (HAL_GetTick() - start_time);
}
