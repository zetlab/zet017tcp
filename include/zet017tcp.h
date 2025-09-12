#ifndef ZET017_TCP_H
#define ZET017_TCP_H

#if defined(_WIN32)
#include <windows.h>
#endif

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
#define ZET017_TCP_API int WINAPI
#else
#define ZET017_TCP_API int
#endif

struct zet017_server;

struct zet017_config
{
	uint32_t sample_rate_adc;
	uint32_t sample_rate_dac;
	uint32_t mask_channel_adc;
	uint32_t mask_icp;
	uint32_t gain[8];
};

struct zet017_info
{
	char ip[16];
	char name[16];
	uint32_t serial;
	char version[32];
};

struct zet017_state
{
	uint16_t connected;
	uint32_t pointer_adc;
	uint32_t buffer_size_adc;
	uint32_t pointer_dac;
};

ZET017_TCP_API zet017_server_create(struct zet017_server** server_ptr);

ZET017_TCP_API zet017_server_free(struct zet017_server** server_ptr);

ZET017_TCP_API zet017_server_add_device(struct zet017_server* server, const char* ip);

ZET017_TCP_API zet017_server_remove_device(struct zet017_server* server, const char* ip);

ZET017_TCP_API zet017_device_get_info(struct zet017_server* server, uint32_t number, struct zet017_info* info);

ZET017_TCP_API zet017_device_get_state(struct zet017_server* server, uint32_t number, struct zet017_state* state);

ZET017_TCP_API zet017_device_get_config(struct zet017_server* server, uint32_t number, struct zet017_config* config);

ZET017_TCP_API zet017_device_set_config(struct zet017_server* server, uint32_t number, struct zet017_config* config);

ZET017_TCP_API zet017_device_start(struct zet017_server* server, uint32_t number);

ZET017_TCP_API zet017_device_stop(struct zet017_server* server, uint32_t number);

ZET017_TCP_API zet017_channel_get_data(
	struct zet017_server* server, uint32_t number, uint32_t channel, uint32_t pointer, float* data, uint32_t size);

#ifdef __cplusplus
}
#endif

#endif // ZET017_TCP_H
