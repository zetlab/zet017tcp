#include "zet017tcp.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32)
	#include <time.h>
#endif

const char device_ip[] = "192.168.1.100";

volatile sig_atomic_t running = 0;

void signal_handler(int signal)
{
	if (signal == SIGINT)
		running = 0;
}

float calculate_mean(float* data, uint32_t size)
{
	if (size == 0)
		return 0.f;

	float sum = 0.f;
	for (uint32_t i = 0; i < size; ++i)
		sum += data[i];
	
	return sum / (float)size;
}

int main(void)
{
	printf("start: example of working with ZET 017 device via TCP/IP\n");

#if !defined(_WIN32)
	struct timespec ts = { 0, 100000000 };
#endif

	running = 1;
	signal(SIGINT, &signal_handler);

	struct zet017_server* server = NULL;
	if (zet017_server_create(&server) != 0)
	{
		fprintf(stderr, "end: create zet017 server object error\n");
		return -1;
	}

	if (zet017_server_add_device(server, device_ip) != 0)
	{
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
	uint32_t mask_icp = 0x02;
	uint32_t channel = 3;
	uint32_t gain[] = { 1, 1, 1, 100, 1, 1, 1, 1 };
	uint32_t pointer_adc = 0;
	float* adc_data = malloc(portion_data_adc * sizeof(float));

	struct zet017_state state_prev, state;
	memset(&state, 0x0, sizeof(struct zet017_state));
	memset(&state_prev, 0x0, sizeof(struct zet017_state));
	
	struct zet017_info info;
	memset(&info, 0x0, sizeof(struct zet017_info));
	
	while (running)
	{
		if (zet017_device_get_state(server, number, &state) == 0)
		{
			if (state.connected != state_prev.connected)
			{
				memset(&info, 0x0, sizeof(struct zet017_info));
				if (zet017_device_get_info(server, number, &info) == 0)
				{
					if (state.connected)
						printf("%s: connected device %s s/n %d (ver. %s)\n", info.ip, info.name, info.serial, info.version);
					else
						printf("%s: disconnected device %s s/n %d\n", info.ip, info.name, info.serial);
				}

				configured = 0;
				counter = 0;
				pointer_adc = 0;
			}

			memcpy(&state_prev, &state, sizeof(struct zet017_state));
		}

		if (state.connected)
		{
			if (!configured)
			{
				struct zet017_config config;
				memset(&config, 0x0, sizeof(struct zet017_config));
				if (zet017_device_get_config(server, number, &config) == 0)
				{
					config.sample_rate_adc = sample_rate_adc;
					config.mask_channel_adc = mask_channel_adc;
					config.mask_icp = mask_icp;
					memcpy(config.gain, gain, sizeof(gain));
					if (zet017_device_set_config(server, number, &config) == 0)
					{
						printf("%s: %s s/n %d: device configured\n", info.ip, info.name, info.serial);
						if (zet017_device_start(server, number) == 0)
						{
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
			if (size >= portion_data_adc)
			{
				pointer_adc += portion_data_adc;
				if (pointer_adc >= state.buffer_size_adc)
					pointer_adc -= state.buffer_size_adc;
				if (0 == zet017_channel_get_data(server, number, channel, pointer_adc, adc_data, portion_data_adc))
				{
					float mean = calculate_mean(adc_data, portion_data_adc);
					printf("%s: %s s/n %d: channel %d: %d sec: mean value: %f V\n", 
						info.ip, info.name, info.serial, channel, ++counter, mean);
				}
			}

			
		}

#if defined(_WIN32)
		Sleep(100);
#else
		nanosleep(&ts, NULL);
#endif
	}

	free(adc_data);

	if (state.connected && configured)
	{
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
