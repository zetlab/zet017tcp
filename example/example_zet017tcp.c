#include "zet017tcp.h"

#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32)
	#include <time.h>
#endif

const double pi = 3.14159265358979323846;

const char device_ip[] = "192.168.1.100";

volatile sig_atomic_t running = 0;

struct signal_data {
	double sine_ampl;
	double sine_freq;
	double sine_phase;
	double sine_dphase;
};

void signal_handler(int signal){
	if (signal == SIGINT)
		running = 0;
}

float calculate_mean(float* data, uint32_t size){
	if (size == 0)
		return 0.f;

	float sum = 0.f;
	for (uint32_t i = 0; i < size; ++i)
		sum += data[i];

	return sum / (float)size;
}

void generate_signal(float* data, uint32_t size, struct signal_data* sig_data) {
	for (uint32_t i = 0; i < size; ++i) {
		data[i] = (float)(sig_data->sine_ampl * sin(sig_data->sine_phase));
		sig_data->sine_phase += sig_data->sine_dphase;
		if (sig_data->sine_phase >= 2. * pi)
			sig_data->sine_phase -= 2. * pi;
	}
}

int main(void) {
	printf("start: example of working with ZET 017 device via TCP/IP\n");

#if !defined(_WIN32)
	struct timespec ts = { 0, 100000000 };
#endif

	running = 1;
	signal(SIGINT, &signal_handler);

	struct zet017_server* server = NULL;
	if (zet017_server_create(&server) != 0) {
		fprintf(stderr, "end: create zet017 server object error\n");
		return -1;
	}

	if (zet017_server_add_device(server, device_ip) != 0) {
		fprintf(stderr, "end: add device %s error\n", device_ip);
		return -2;
	}
	
	printf("%s: device added\n", device_ip);

	const uint32_t number = 0;
	int32_t configured = 0;
	uint32_t counter = 0;

	uint32_t sample_rate_adc = 25000;
	uint32_t portion_data_adc = sample_rate_adc;
	uint32_t mask_channel_adc = 0x0e;
	uint32_t mask_icp = 0x08;
	uint32_t channel_adc = 3;
	uint32_t gain[] = { 1, 1, 1, 100, 1, 1, 1, 1 };
	uint32_t pointer_adc = 0;
	float* adc_data = malloc(portion_data_adc * sizeof(float));

	uint32_t sample_rate_dac = 50000;
	uint32_t portion_data_dac = sample_rate_dac / 10;
	uint32_t advance_data_dac = sample_rate_dac / 2;
	uint32_t channel_dac = 0;
	uint32_t pointer_dac = sample_rate_dac + portion_data_dac;
	float* dac_data = malloc(portion_data_dac * sizeof(float));

	struct signal_data sig_data;
	sig_data.sine_ampl = 1.;
	sig_data.sine_freq = 1011.213;
	sig_data.sine_phase = 0.;
	sig_data.sine_dphase = sig_data.sine_freq / sample_rate_dac * 2. * pi;

	struct zet017_state state_prev, state;
	memset(&state, 0x0, sizeof(struct zet017_state));
	memset(&state_prev, 0x0, sizeof(struct zet017_state));

	struct zet017_info info;
	memset(&info, 0x0, sizeof(struct zet017_info));

	while (running) {
		if (zet017_device_get_state(server, number, &state) == 0) {
			if (state.is_connected != state_prev.is_connected || state.reconnect != state_prev.reconnect) {
				memset(&info, 0x0, sizeof(struct zet017_info));
				if (zet017_device_get_info(server, number, &info) == 0) {
					if (state.is_connected)
						printf("%s: connected device %s s/n %d (ver. %s)\n", info.ip, info.name, info.serial, info.version);
					else
						printf("%s: disconnected device %s s/n %d\n", info.ip, info.name, info.serial);
				}

				configured = 0;
				counter = 0;
				pointer_adc = 0;
				pointer_dac = sample_rate_dac + portion_data_dac;
				sig_data.sine_phase = 0.;
			}

			memcpy(&state_prev, &state, sizeof(struct zet017_state));
		}

		if (state.is_connected) {
			if (!configured) {
				struct zet017_config config;
				memset(&config, 0x0, sizeof(struct zet017_config));
				if (zet017_device_get_config(server, number, &config) == 0) {
					config.sample_rate_adc = sample_rate_adc;
					config.mask_channel_adc = mask_channel_adc;
					config.mask_icp = mask_icp;
					memcpy(config.gain, gain, sizeof(gain));
					config.sample_rate_dac = sample_rate_dac;
					if (zet017_device_set_config(server, number, &config) == 0) {
						printf("%s: %s s/n %d: device configured\n", info.ip, info.name, info.serial);
						if (zet017_device_start(server, number, 1) == 0) {
							printf("%s: %s s/n %d: device started\n", info.ip, info.name, info.serial);
							configured = 1;
						}
					}
				}
			}

			uint32_t size = 0;
			if (state.pointer_adc > pointer_adc)
				size = state.pointer_adc - pointer_adc;
			else if (state.pointer_adc < pointer_adc)
				size = state.buffer_size_adc + state.pointer_adc - pointer_adc;
			if (size >= portion_data_adc) {
				pointer_adc += portion_data_adc;
				if (pointer_adc >= state.buffer_size_adc)
					pointer_adc -= state.buffer_size_adc;
				if (0 == zet017_channel_get_data(server, number, channel_adc, pointer_adc, adc_data, portion_data_adc)) {
					float mean = calculate_mean(adc_data, portion_data_adc);
					printf("%s: %s s/n %d: channel %d: %d sec: mean value: %f V\n", 
						info.ip, info.name, info.serial, channel_adc, ++counter, mean);
				}
			}

			for (;;) {
				if (pointer_dac >= state.pointer_dac)
					size = pointer_dac - state.pointer_dac;
				else if (pointer_dac < state.pointer_dac)
					size = pointer_dac + state.buffer_size_dac - state.pointer_dac;
				if (size < portion_data_dac + advance_data_dac) {
					pointer_dac += portion_data_dac;
					if (pointer_dac >= state.buffer_size_dac)
						pointer_dac -= state.buffer_size_dac;

					generate_signal(dac_data, portion_data_dac, &sig_data);
					zet017_channel_put_data(server, number, channel_dac, pointer_dac, dac_data, portion_data_dac);
				}
				else
					break;
			}
		}

#if defined(_WIN32)
		Sleep(100);
#else
		nanosleep(&ts, NULL);
#endif
	}

	free(adc_data);

	if (state.is_connected && configured) {
		if (zet017_device_stop(server, number) != 0)
			fprintf(stderr, "%s: %s s/n %d: stop device error\n", info.ip, info.name, info.serial);
		else
			printf("%s: %s s/n %d: device stopped\n", info.ip, info.name, info.serial);
	}

	if (zet017_server_free(&server) != 0)
		fprintf(stderr, "zet017 server object free error\n");

	printf("end: example of working with ZET 017 device via TCP/IP\n");

	return 0;
}
