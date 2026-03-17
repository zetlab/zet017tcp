#include "zet017tcp.h"

#include <libxml/parser.h>

#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#include <shlwapi.h>
#else
#include <libgen.h>
#include <limits.h>
#include <time.h>
#endif

const char device_ip[] = "192.168.1.100";
char config_file_name[] = "files/devices.cfg";

#if defined(_WIN32)
#define PATH_LENGTH MAX_PATH
#else
#define PATH_LENGTH PATH_MAX
#endif

volatile sig_atomic_t running = 0;

void signal_handler(int signal) {
	if (signal == SIGINT)
		running = 0;
}

float calculate_mean(float* data, uint32_t size) {
	if (size == 0)
		return 0.f;

	float sum = 0.f;
	for (uint32_t i = 0; i < size; ++i)
		sum += data[i];

	return sum / (float)size;
}

float calculate_rms(float* data, uint32_t size)
{
	if (size == 0)
		return 0.f;

	float mean = calculate_mean(data, size);

	double sum2 = 0.;
	for (uint32_t i = 0; i < size; ++i) {
		double val = (double)(data[i] - mean);
		sum2 += val * val;
	}

	return (float)sqrt(sum2 / size );
}

float calculate_tenso_dc(float* data_meas, float* data_ref, uint32_t size) {
	float mean_meas = calculate_mean(data_meas, size);
	float mean_ref = calculate_mean(data_ref, size);

	return mean_ref ? (mean_meas / mean_ref) * 1000.f : 0.f;
}

float calculate_tenso_ac(float* data_meas, float* data_ref, uint32_t size) {
	float mean_meas = calculate_rms(data_meas, size);
	float mean_ref = calculate_rms(data_ref, size);

	return mean_ref ? (mean_meas / mean_ref) * 1000.f : 0.f;
}

int get_path(char* path, uint32_t length) {
#if defined(_WIN32)
	if (GetModuleFileNameA(NULL, path, length) == 0 || PathRemoveFileSpecA(path) == 0)
		return -1;
#else
	if (realpath("/proc/self/exe", path) == NULL || dirname(path) == NULL)
		return -1;
#endif
	return 0;
}

int load_config_doc(xmlDocPtr doc, const struct zet017_info* info, struct zet017_config* config, struct zet017_tenso_config* tenso_config) {
	if (!doc || !info || !config)
		return -1;

	xmlNodePtr config_node = xmlDocGetRootElement(doc);
	if (config_node == NULL)
		return -2;

	if (xmlStrcmp(config_node->name, "Config") != 0)
		return -3;

	xmlChar* str_version = xmlGetProp(config_node, "version");
	if (str_version == NULL)
		return -4;

	double result = strtod(str_version, NULL);
	xmlFree(str_version);
	if (result != 1.2)
		return -5;

	xmlNodePtr device_node = NULL;
	for (device_node = config_node->children; device_node != NULL; device_node = device_node->next) {
		if (device_node->type != XML_ELEMENT_NODE)
			continue;

		if (xmlStrcmp(device_node->name, "Device") != 0)
			continue;

		xmlChar* str_serial = xmlGetProp(device_node, "serial");
		if (str_serial == NULL)
			return -6;

		uint32_t serial = strtoul(str_serial, NULL, 10);
		xmlFree(str_serial);

		if (serial == info->serial)
			break;
	}
	if (device_node == NULL)
		return -6;

	for (xmlNodePtr child = device_node->children; child != NULL; child = child->next) {
		if (child->type != XML_ELEMENT_NODE)
			continue;

		if (xmlStrcmp(child->name, "Channel") == 0) {
			if (child->children != NULL) {
				xmlChar* str_channel = xmlNodeGetContent(child->children);
				config->mask_channel_adc = strtoul(str_channel, NULL, 10);
				xmlFree(str_channel);
			}
		}
		else if (xmlStrcmp(child->name, "HCPChannel") == 0) {
			if (child->children != NULL) {
				xmlChar* str_hcp_channel = xmlNodeGetContent(child->children);
				config->mask_icp = strtoul(str_hcp_channel, NULL, 10);
				xmlFree(str_hcp_channel);
			}
		}
		else if (xmlStrcmp(child->name, "ModaADC") == 0) {
			if (child->children != NULL) {
				xmlChar* str_moda_adc = xmlNodeGetContent(child->children);
				config->moda_adc = (uint16_t)strtoul(str_moda_adc, NULL, 10);
				config->sample_rate_adc = 0;
				xmlFree(str_moda_adc);
			}
		}
		else if (xmlStrcmp(child->name, "RateDAC") == 0) {
			if (child->children != NULL) {
				xmlChar* str_rate_dac = xmlNodeGetContent(child->children);
				config->rate_dac = (uint16_t)strtoul(str_rate_dac, NULL, 10);
				config->sample_rate_dac = 0;
				xmlFree(str_rate_dac);
			}
		}
		else if (xmlStrcmp(child->name, "KodAmplify") == 0) {
			if (child->children != NULL) {
				xmlChar* str_kod_amplify = xmlNodeGetContent(child->children);
				xmlChar* token = strtok(str_kod_amplify, ",");
				uint16_t i = 0;
				while (token != NULL) {
					config->gain_code[i] = (uint16_t)strtoul(token, NULL, 10);
					config->gain[i] = 0;
					if (++i >= sizeof(config->gain) / sizeof(config->gain[0]))
						break;
					token = strtok(NULL, ",");
				}
				xmlFree(str_kod_amplify);
			}
		}
		else if (xmlStrcmp(child->name, "BuiltinGenActive") == 0) {
			if (child->children != NULL) {
				xmlChar* str_builtin_gen_active = xmlNodeGetContent(child->children);
				if (strtoul(str_builtin_gen_active, NULL, 10))
					config->builtin_dac_state |= 0x1;
				xmlFree(str_builtin_gen_active);
			}
		}
		else if (xmlStrcmp(child->name, "BuiltinGenSineActive") == 0) {
			if (child->children != NULL) {
				xmlChar* str_builtin_gen_sine_active = xmlNodeGetContent(child->children);
				if (strtoul(str_builtin_gen_sine_active, NULL, 10))
					config->builtin_dac_state |= 0x2;
				xmlFree(str_builtin_gen_sine_active);
			}
		}
		else if (xmlStrcmp(child->name, "BuiltinGenSineFreq") == 0) {
			if (child->children != NULL) {
				xmlChar* str_builtin_gen_sine_freq = xmlNodeGetContent(child->children);
				config->builtin_dac_sine_freq = strtod(str_builtin_gen_sine_freq, NULL);
				xmlFree(str_builtin_gen_sine_freq);
			}
		}
		else if (xmlStrcmp(child->name, "BuiltinGenSineAmpl") == 0) {
			if (child->children != NULL) {
				xmlChar* str_builtin_gen_sine_ampl = xmlNodeGetContent(child->children);
				config->builtin_dac_sine_ampl = strtod(str_builtin_gen_sine_ampl, NULL);
				xmlFree(str_builtin_gen_sine_ampl);
			}
		}
		else if (xmlStrcmp(child->name, "BuiltinGenSineBias") == 0) {
			if (child->children != NULL) {
				xmlChar* str_builtin_gen_sine_bias = xmlNodeGetContent(child->children);
				config->builtin_dac_sine_offset = strtod(str_builtin_gen_sine_bias, NULL);
				xmlFree(str_builtin_gen_sine_bias);
			}
		}
		if (xmlStrcmp(child->name, "Channels") == 0) {
			for (xmlNodePtr child2 = child->children; child2 != NULL; child2 = child2->next) {
				if (child2->type != XML_ELEMENT_NODE)
					continue;

				if (xmlStrcmp(child2->name, "Channel") == 0) {
					xmlChar* str_id = xmlGetProp(child2, "id");
					if (str_id == NULL)
						continue;

					uint32_t i = strtoul(str_id, NULL, 10);
					xmlFree(str_id);
					if (i >= 8)
						continue;

					for (xmlNodePtr child3 = child2->children; child3 != NULL; child3 = child3->next) {
						if (child3->type != XML_ELEMENT_NODE)
							continue;

						if (xmlStrcmp(child3->name, "Tenso") == 0) {
							if (child3->children != NULL) {
								xmlChar* str_tenso = xmlNodeGetContent(child3->children);
								tenso_config->scheme[i] = (enum zet017_scheme)strtoul(str_tenso, NULL, 10);
								xmlFree(str_tenso);
							}
						}
						else if (xmlStrcmp(child3->name, "Pot1") == 0) {
							if (child3->children != NULL) {
								xmlChar* str_pot1 = xmlNodeGetContent(child3->children);
								tenso_config->correction_1[i] = (uint8_t)strtoul(str_pot1, NULL, 10);
								xmlFree(str_pot1);
							}
						}
						else if (xmlStrcmp(child3->name, "Pot2") == 0) {
							if (child3->children != NULL) {
								xmlChar* str_pot2 = xmlNodeGetContent(child3->children);
								tenso_config->correction_2[i] = (uint8_t)strtoul(str_pot2, NULL, 10);
								xmlFree(str_pot2);
							}
						}
					}
				}
			}
		}
	}

	return 0;
}

int load_config(const char* file_name, const struct zet017_info* info, struct zet017_config* config, struct zet017_tenso_config* tenso_config) {
	if (!file_name || !info || !config)
		return -1;

	xmlDocPtr doc = xmlParseFile(file_name);
	if (doc == NULL)
		return -1;

	int r = load_config_doc(doc, info, config, tenso_config) != 0 ? -2 : 0;

	xmlFreeDoc(doc);
	xmlCleanupParser();

	return r;
}

int main(void) {
	printf("start: example of ZET 017 device configuring from xml file via TCP/IP\n");

	char path[PATH_LENGTH];
	if (get_path(path, PATH_LENGTH) == 0) {
		strcat(path, "/");
		strcat(path, config_file_name);
	}

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

	uint32_t pointer_adc = 0;
	uint32_t portion_data_adc = 0;
	float* adc_data_meas = NULL;
	float* adc_data_ref = NULL;

	struct zet017_state state_prev, state;
	memset(&state, 0x0, sizeof(struct zet017_state));
	memset(&state_prev, 0x0, sizeof(struct zet017_state));

	struct zet017_info info;
	memset(&info, 0x0, sizeof(struct zet017_info));

	struct zet017_config config;
	memset(&config, 0x0, sizeof(struct zet017_config));

	struct zet017_tenso_config tenso_config;
	memset(&tenso_config, 0x0, sizeof(struct zet017_tenso_config));

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
				portion_data_adc = 0;
				if (adc_data_meas)
					free(adc_data_meas);
				if (adc_data_ref)
					free(adc_data_ref);
			}

			memcpy(&state_prev, &state, sizeof(struct zet017_state));
		}

		if (state.is_connected) {
			if (!configured) {
				if (zet017_device_get_config(server, number, &config) == 0 &&
					zet017_device_get_tenso_config(server, number, &tenso_config) == 0) {
					if (load_config(path, &info, &config, &tenso_config) == 0) {
						if (zet017_device_set_config(server, number, &config) == 0 &&
							zet017_device_set_tenso_config(server, number, &tenso_config) == 0) {
							printf("%s: %s s/n %d: device configured\n", info.ip, info.name, info.serial);
							if (zet017_device_get_config(server, number, &config) == 0 &&
								zet017_device_get_tenso_config(server, number, &tenso_config) == 0) {
								if (zet017_device_start(server, number, 1) == 0) {
									printf("%s: %s s/n %d: device started\n", info.ip, info.name, info.serial);
									configured = 1;

									portion_data_adc = config.sample_rate_adc;
									adc_data_meas = malloc(portion_data_adc * sizeof(float));
									adc_data_ref = malloc(portion_data_adc * sizeof(float));
								}
							}
						}
					}
				}
			}

			if (portion_data_adc && adc_data_meas && adc_data_ref) {
				uint32_t size = 0;
				if (state.pointer_adc > pointer_adc)
					size = state.pointer_adc - pointer_adc;
				else if (state.pointer_adc < pointer_adc)
					size = state.buffer_size_adc + state.pointer_adc - pointer_adc;
				if (size >= portion_data_adc) {
					pointer_adc += portion_data_adc;
					if (pointer_adc >= state.buffer_size_adc)
						pointer_adc -= state.buffer_size_adc;

					int32_t can_calculate = 1;
					if (0 != zet017_channel_get_data(server, number, 0, pointer_adc, adc_data_meas, portion_data_adc))
						can_calculate = 0;
					if (0 != zet017_channel_get_data(server, number, 8, pointer_adc, adc_data_ref, portion_data_adc))
						can_calculate = 0;

					if (can_calculate) {
						if (config.builtin_dac_sine_ampl && config.builtin_dac_sine_freq) {
							float mean_tenso = calculate_tenso_ac(adc_data_meas, adc_data_ref, portion_data_adc);
							printf("%s: %s s/n %d: channel %d: %d sec: mean ac tenso value: %f mV/V\n",
								info.ip, info.name, info.serial, 0, ++counter, mean_tenso);
						}
						else {
							float mean_tenso = calculate_tenso_dc(adc_data_meas, adc_data_ref, portion_data_adc);
							printf("%s: %s s/n %d: channel %d: %d sec: mean dc tenso value: %f mV/V\n",
								info.ip, info.name, info.serial, 0, ++counter, mean_tenso);
						}
					}
				}
			}
		}

#if defined(_WIN32)
		Sleep(100);
#else
		nanosleep(&ts, NULL);
#endif
	}

	if (adc_data_meas)
		free(adc_data_meas);
	if (adc_data_ref)
		free(adc_data_ref);

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
