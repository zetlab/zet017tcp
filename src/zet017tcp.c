#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define ZET017_TCP_WINDOWS
#include <winsock2.h>
#include <ws2tcpip.h>
#define socket_t SOCKET
#define close_socket(s) closesocket(s)
typedef HANDLE thread_t;
typedef CRITICAL_SECTION mutex_t;
typedef CONDITION_VARIABLE cond_t;
#define THREAD_RETURN DWORD WINAPI
#else
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#define socket_t int
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define close_socket(s) close(s)
typedef pthread_t thread_t;
typedef pthread_mutex_t mutex_t;
typedef pthread_cond_t cond_t;
#define THREAD_RETURN void*
#endif

#include "zet017tcp.h"

#define MAX_IP_LENGTH 16

#define ZET017_CMD_PORT 1808
#define ZET017_ADC_PORT 2320
#define ZET017_DAC_PORT 3344

#define ZET017_CMD_GET_INFO 0x0000
#define ZET017_CMD_PUT_INFO 0x0012
#define ZET017_CMD_READ_CORRECTION 0x0513

#define ZET017_PACKET_SIZE 1024
#define ZET017_MAX_FLUSH_SIZE 2048

#define ZET017_MAX_SAMPLE_RATE_ADC 50000
#define ZET017_MAX_CHANNELS_ADC 8
#define ZET017_MAX_GAINS_ADC 4
#define ZET017_MAX_SAMPLE_SIZE_ADC sizeof(int32_t)
#define ZET017_MAX_ADC_BUFFER_SIZE (ZET017_MAX_SAMPLE_RATE_ADC * ZET017_MAX_CHANNELS_ADC* ZET017_MAX_SAMPLE_SIZE_ADC)
#define ZET017_ADC_GR_BUFFER_SIZE (1 * 2 * 3 * 2 * 5 * 1 * 7 * 2 * sizeof(int32_t))
#define ZET017_ADC_BUFFER_SIZE (ZET017_MAX_ADC_BUFFER_SIZE / ZET017_ADC_GR_BUFFER_SIZE + 1) * ZET017_ADC_GR_BUFFER_SIZE

#define ZET017_MAX_SAMPLE_RATE_DAC 200000
#define ZET017_MAX_CHANNELS_DAC 2
#define ZET017_MAX_SAMPLE_SIZE_DAC sizeof(int32_t)
#define ZET017_MAX_DAC_BUFFER_SIZE (ZET017_MAX_SAMPLE_RATE_DAC * ZET017_MAX_CHANNELS_DAC* ZET017_MAX_SAMPLE_SIZE_DAC)
#define ZET017_DAC_BUFFER_SIZE ZET017_MAX_DAC_BUFFER_SIZE * 4

enum zet017_command {
	zet017_set_config = 0,
	zet017_start,
	zet017_stop,
};

enum zet017_command_state {
	zet017_command_idle = 0,
	zet017_command_requested,
	zet017_command_processing,
	zet017_command_completed,
};

struct zet017_device_info {
	uint16_t command;				//0x000: код команды (0x0000 — GetInfo)
	uint8_t reserve_1[2];
	int16_t start_adc;				//0x004: управление АЦП (1, 0, 1)
	int16_t start_dac;				//0x006: управление ЦАП (1, 0, 1)
	uint8_t reserve_2[6];
	uint16_t quantity_channel_adc;	//0x00e: общее количество каналов АЦП (4, 8)
	uint16_t quantity_channel_dac;	//0x010: общее количество каналов ЦАП (1)
	uint8_t type_data_adc;			//0x012: тип данных АЦП (0 — int16_t, 1 — long)
	uint8_t type_data_dac;			//0x013: тип данных ЦАП (0 — int16_t)
	uint32_t mask_channel_adc;		//0x014: маска активных каналов АЦП (0x00..0xFF)
	uint32_t mask_channel_dac;		//0x018: маска активных каналов ЦАП (0x00..0x01)
	uint32_t mask_icp;				//0x01c: маска ICP каналов АЦП (0x00..0xFF)
	uint8_t reserve_3[4];
	uint16_t work_channel_adc;		//0x024: количество активных каналов АЦП (1..8)
	uint16_t work_channel_dac;		//0x026: количество активных канало ЦАП (0..1)
	uint16_t amplify_code[8];		//0x028: коды коэффициентов усиления канало АЦП (0: КУ1, 1: КУ10, 2: КУ100)
	uint8_t reserve_4[112];
	uint16_t atten[4];				//0x0a8: коды аттенюатора ЦАП (0x00..0xFFFF)
	uint8_t reserve_5[10];
	uint16_t mode_adc;				//0x0ba: режим работы АЦП (0: по умолчанию (25 кГц), 1: 50 кГц, 2: 25 кГц, 3: 5 кГц, 4: 2.5 кГц)
	uint8_t reserve_6[2];
	uint16_t rate_dac;				//0x0be: режим работы ЦАП (400: 200 кГц, 800: 100 кГц, 1600: 50 кГц, 3200: 25 кГц)
	uint16_t size_packet_adc;		//0x0c0: размер пакета данных АЦП в словах
	uint8_t reserve_7[22];
	uint32_t digital_input;			//0x0d8: маска состояния входов цифрового порта
	uint32_t digital_output;		//0x0dc: маска состояния выходов цифрового порта
	uint8_t reserve_8[12];
	char version_dsp[32];			//0x0ec: строка с версией устройства
	char device_name[16];			//0x10c: строка с названием устройства
	uint8_t reserve_9[16];
	uint32_t serial;				//0x12c: серийный номер устройства
	uint8_t reserve_10[12];
	uint32_t digital_output_enable;	//0x13c: маска разрешения выхода цифрового порта
	float resolution_adc_def;		//0x140: номинальный вес младшего разряда АЦП
	uint8_t reserve_11[4];
	float resolution_dac_def;		//0x148: номинальный вес младшего разряда ЦАП
	uint8_t reserve_12[4];
	float resolution_adc[16];		//0x150: откалиброванный вес младшего разряда АЦП
	uint8_t reserve_13[38];
	uint16_t atten_speed;			//0x1b6: скорость нарастания аттенюатора (0 - отключено, т.е. мгновенно)
	uint8_t reserve_14[24];
	float resolution_dac[4];		//0x1d0: откалиброванный вес младшего разряда ЦАП
	uint8_t reserve_15[8];
	uint16_t quantity_channel_virt;
	uint8_t reserve_16[22];
};

struct zet017_correction_info {
	float amplify[ZET017_MAX_CHANNELS_ADC][ZET017_MAX_GAINS_ADC];
	float offset_adc[ZET017_MAX_CHANNELS_ADC][ZET017_MAX_GAINS_ADC];
	float reduction[ZET017_MAX_CHANNELS_DAC];
	float offset_dac[ZET017_MAX_CHANNELS_DAC];
};

struct zet017_command_info {
	uint16_t command;
	uint16_t error;
	uint32_t size;
	union {
		uint16_t u16[(1024 - 8) / 2];
		uint8_t u8[1024 - 8];
	} data;
};

union zet017_packet {
	uint8_t raw[ZET017_PACKET_SIZE];
	struct zet017_device_info info;
	struct zet017_command_info cmd;
};

struct zet017_command_data {
	union zet017_packet data;
	enum zet017_command command;
	enum zet017_command_state state;
	int result;
	mutex_t mutex;
	cond_t cond;
};

struct zet017_adc_data {
	uint8_t buffer[ZET017_ADC_BUFFER_SIZE];
	uint32_t pointer;
	uint32_t channel_mask;
	uint16_t channel_quantity;
	uint16_t sample_size;
	uint16_t amplify_code[ZET017_MAX_CHANNELS_ADC];

	float resolution[ZET017_MAX_CHANNELS_ADC][ZET017_MAX_GAINS_ADC];

	mutex_t mutex;
};

struct zet017_dac_data {
	uint8_t buffer[ZET017_DAC_BUFFER_SIZE];
	uint32_t pointer;
	uint32_t channel_mask;
	uint16_t channel_quantity;
	uint16_t sample_size;

	float resolution[ZET017_MAX_CHANNELS_DAC];

	mutex_t mutex;
};

struct zet017_adc_dac_data {
	uint32_t sample_rate_adc;
	uint16_t sample_size_adc;
	uint16_t work_channel_adc;
	uint64_t adc_count;
	uint32_t sample_rate_dac;
	uint16_t work_channel_dac;
	uint16_t sample_size_dac;
	uint64_t dac_count;
};

struct zet017_device {
	char ip[MAX_IP_LENGTH];
	socket_t cmd_socket;
	socket_t adc_socket;
	socket_t dac_socket;
	socket_t wakeup_socket[2];

	thread_t work_thread;
	uint16_t running;

	uint16_t is_connected;
	uint64_t reconnect;
	uint32_t timestamp;
	struct zet017_device_info device_info;
	struct zet017_adc_dac_data adc_dac_data;

	struct zet017_state state;
	mutex_t state_mutex;

	struct zet017_info info;
	mutex_t info_mutex;

	struct zet017_config config;
	mutex_t config_mutex;

	struct zet017_command_data command;

	struct zet017_adc_data adc_data;
	struct zet017_dac_data dac_data;

	struct zet017_correction_info correction;

	struct zet017_device* next;
};

struct zet017_server {
	struct zet017_device* devices;
	size_t device_count;
	mutex_t devices_mutex;
};

static int mutex_init(mutex_t* mutex) {
#if defined(ZET017_TCP_WINDOWS)
	InitializeCriticalSection(mutex);
#else
	if (pthread_mutex_init(mutex, NULL) != 0)
		return -1;
#endif
	return 0;
}

static void mutex_destroy(mutex_t* mutex) {
#if defined(ZET017_TCP_WINDOWS)
	DeleteCriticalSection(mutex);
#else
	pthread_mutex_destroy(mutex);
#endif
}

static void mutex_lock(mutex_t* mutex) {
#if defined(ZET017_TCP_WINDOWS)
	EnterCriticalSection(mutex);
#else
	pthread_mutex_lock(mutex);
#endif
}

static void mutex_unlock(mutex_t* mutex) {
#if defined(ZET017_TCP_WINDOWS)
	LeaveCriticalSection(mutex);
#else
	pthread_mutex_unlock(mutex);
#endif
}

static int cond_init(cond_t* cond) {
#if defined(ZET017_TCP_WINDOWS)
	InitializeConditionVariable(cond);
#else
	if (pthread_cond_init(cond, NULL) != 0)
		return -1;
#endif
	return 0;
}

static void cond_destroy(cond_t* cond) {
#if !defined(ZET017_TCP_WINDOWS)
	pthread_cond_destroy(cond);
#endif
}

static void cond_wait(cond_t* cond, mutex_t* mutex) {
#if defined(ZET017_TCP_WINDOWS)
	SleepConditionVariableCS(cond, mutex, INFINITE);
#else
	pthread_cond_wait(cond, mutex);
#endif
}

static void cond_signal(cond_t* cond) {
#if defined(ZET017_TCP_WINDOWS)
	WakeConditionVariable(cond);
#else
	pthread_cond_signal(cond);
#endif
}

static int network_init(void) {
#if defined(ZET017_TCP_WINDOWS)
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		return -1;
#endif
	return 0;
}

static void network_cleanup(void) {
#if defined(ZET017_TCP_WINDOWS)
	WSACleanup();
#endif
}

static uint32_t zet017_get_timestamp(void) {
#if defined(ZET017_TCP_WINDOWS)
	return GetTickCount();
#else
	struct timespec ts;
	uint32_t ms;

	if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
		ms = (uint32_t)(ts.tv_sec * 1000);
		ms += (uint32_t)(ts.tv_nsec + 500000) / 1000000;
	}
	else
		ms = 0;

	return ms;
#endif
}

static uint32_t zet017_get_sample_rate_adc(uint16_t mode_adc) {
	switch (mode_adc) {
	case 1:
		return 50000;
	case 3:
		return 5000;
	case 4:
		return 2500;
	default:
		break;
	}

	return 25000;
}

static uint16_t zet017_get_mode_adc(uint32_t sample_rate_adc) {
	switch (sample_rate_adc) {
	case 50000:
		return 1;
	case 25000:
		return 2;
	case 5000:
		return 3;
	case 2500:
		return 4;
	default:
		break;
	}

	return 0;
}

static uint32_t zet017_get_sample_rate_dac(uint16_t rate_dac) {
	return rate_dac ? 80000000 / rate_dac : 0;
}

static uint16_t zet017_get_rate_dac(uint32_t sample_rate_dac) {
	return sample_rate_dac ? 80000000 / sample_rate_dac : 0;
}

static uint32_t zet017_get_gain(uint16_t amplify_code) {
	switch (amplify_code) {
	case 0:
		return 1;
	case 1:
		return 10;
	case 2:
		return 100;
	default: 
		break;
	}

	return 0;
}

static uint32_t zet017_get_amplify_code(uint32_t gain) {
	switch (gain) {
	case 1:
		return 0;
	case 10:
		return 1;
	case 100:
		return 2;
	default:
		break;
	}

	return 0;
}

static void zet017_set_size_packet_adc(struct zet017_device_info* info) {
	uint16_t work_channel_adc = 0;
	for (uint16_t i = 0; i < info->quantity_channel_adc; ++i) {
		uint16_t j = 1 << (info->quantity_channel_adc == 4 ? i * 2 + 1 : i);
		if (info->mask_channel_adc & j)
			++work_channel_adc;
	}
	uint32_t sample_size = (uint32_t)(info->type_data_adc == 0 ? sizeof(int16_t) : sizeof(int32_t));
	uint32_t max_bytes_count = ZET017_PACKET_SIZE - sizeof(uint64_t);
	uint32_t max_samples_count = max_bytes_count / sample_size;
	uint32_t max_frames_count = max_samples_count / work_channel_adc;
	uint32_t sample_rate_adc = zet017_get_sample_rate_adc(info->mode_adc);
	for (;;) {
		uint32_t count = sample_rate_adc / max_frames_count;
		if (count >= 10 || max_frames_count == 0)
			break;
		max_frames_count /= 2;
	}
	if (max_frames_count == 0)
		max_frames_count = 1;

	uint32_t size_packet_adc = max_frames_count * work_channel_adc * sample_size / 2;
	info->size_packet_adc = size_packet_adc;
}

static struct zet017_device* zet017_get_device(struct zet017_server* server, uint32_t number) {
	if (server && server->device_count > number) {
		struct zet017_device* device = server->devices;
		while (device != NULL) {
			if (number == 0)
				return device;

			--number;
			device = device->next;
		}
	}

	return NULL;
}

static socket_t zet017_socket_connect(const char* ip, unsigned short port) {
	socket_t sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
	if (sock == INVALID_SOCKET)
		return INVALID_SOCKET;

	for (;;) {
		struct sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0)
			break;
		addr.sin_port = htons(port);

#if defined(ZET017_TCP_WINDOWS)
		u_long mode = 1;
		if (ioctlsocket(sock, FIONBIO, &mode) != 0)
#else
		int mode = 1;
		if (ioctl(sock, FIONBIO, &mode) < 0)
#endif
			break;

		int r = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
		if (r == SOCKET_ERROR) {
#if defined(ZET017_TCP_WINDOWS)
			int r = WSAGetLastError();
			if (r != WSAEWOULDBLOCK && r != WSAEINPROGRESS)
#else
			if (errno != EINPROGRESS)
#endif
				break;
		}

		return sock;
	}

	close_socket(sock);

	return INVALID_SOCKET;
}

static int zet017_socket_wait_connect(struct zet017_device* device, socket_t* sock) {
	fd_set rfds;
	FD_ZERO(&rfds);
	FD_SET(device->wakeup_socket[1], &rfds);

	fd_set wfds;
	FD_ZERO(&wfds);
	FD_SET(*sock, &wfds);

	int nfds = (int)(*sock > device->wakeup_socket[1] ? *sock : device->wakeup_socket[1]);

	struct timeval tv;
	tv.tv_sec = 10;
	tv.tv_usec = 0;

	int r = select(nfds + 1, &rfds, &wfds, NULL, &tv);
	if (r != -1 && r != 0) {
		if (FD_ISSET(*sock, &wfds)) {
			int optval = 0;
			int optlen = sizeof(optval);
			r = getsockopt(*sock, SOL_SOCKET, SO_ERROR, (char*)&optval, &optlen);
			if (r == 0 && optval == 0) {
				optlen = sizeof(optval);
				optval = 1;
				(void)setsockopt(*sock, SOL_SOCKET, SO_KEEPALIVE, (const char*)&optval, optlen);

#if defined(TCP_KEEPALIVE)
				optval = 20;
				(void)setsockopt(*sock, IPPROTO_TCP, TCP_KEEPALIVE, (const char*)&optval, optlen);
#elif defined(TCP_KEEPIDLE)
				optval = 20;
				(void)setsockopt(*sock, IPPROTO_TCP, TCP_KEEPIDLE, (const char*)&optval, optlen);
#endif

#if defined(TCP_KEEPINTVL)
				optval = 1;
				(void)setsockopt(*sock, IPPROTO_TCP, TCP_KEEPINTVL, (const char*)&optval, optlen);
#endif

#if defined(TCP_KEEPCNT)
				optval = 10;
				(void)setsockopt(*sock, IPPROTO_TCP, TCP_KEEPCNT, (const char*)&optval, optlen);
#endif

				return 0;
			}
		}
		if (FD_ISSET(device->wakeup_socket[1], &rfds)) {
			char buf;
			recv(device->wakeup_socket[1], &buf, 1, 0);
		}
	}

	return -1;
}

static int zet017_socket_handshake(struct zet017_device* device, socket_t* sock) {
	uint32_t flush_size = 0;
	char flush_data[ZET017_MAX_FLUSH_SIZE + sizeof(flush_size)];
	int flush_data_ptr = 0;

	for (;;) {
		fd_set rfds;
		FD_ZERO(&rfds);
		FD_SET(device->wakeup_socket[1], &rfds);
		FD_SET(*sock, &rfds);

		int nfds = (int)(*sock > device->wakeup_socket[1] ? *sock : device->wakeup_socket[1]);

		struct timeval tv;
		tv.tv_sec = 10;
		tv.tv_usec = 0;

		int r = select(nfds + 1, &rfds, NULL, NULL, &tv);
		if (r == -1 || r == 0)
			break;

		if (FD_ISSET(device->wakeup_socket[1], &rfds)) {
			char buf;
			recv(device->wakeup_socket[1], &buf, 1, 0);
			break;
		}

		if (FD_ISSET(*sock, &rfds)) {
			int len = sizeof(flush_data) - flush_data_ptr;
			r = recv(*sock, flush_data + flush_data_ptr, len, 0);
			if (r <= 0)
				break;

			flush_data_ptr += r;
			if (flush_data_ptr >= sizeof(flush_size)) {
				flush_size = *(uint32_t*)(flush_data);
				if (flush_data_ptr - sizeof(flush_size) == flush_size)
					return 0;
			}
		}
	}

	return -1;
}

static int zet017_socket_cmd_connect(struct zet017_device* device) {
	for (;;) {
		device->cmd_socket = zet017_socket_connect(device->ip, ZET017_CMD_PORT);
		if (INVALID_SOCKET == device->cmd_socket)
			break;

		if (zet017_socket_wait_connect(device, &device->cmd_socket) != 0)
			break;

		if (zet017_socket_handshake(device, &device->cmd_socket) != 0)
			break;

		return 0;
	}

	return -1;
}

static int zet017_socket_adc_connect(struct zet017_device* device) {
	for (;;) {
		device->adc_socket = zet017_socket_connect(device->ip, ZET017_ADC_PORT);
		if (INVALID_SOCKET == device->adc_socket)
			break;

		if (zet017_socket_wait_connect(device, &device->adc_socket) != 0)
			break;

		if (zet017_socket_handshake(device, &device->adc_socket) != 0)
			break;

		return 0;
	}

	return -1;
}

static int zet017_socket_dac_connect(struct zet017_device* device) {
	for (;;) {
		device->dac_socket = zet017_socket_connect(device->ip, ZET017_DAC_PORT);
		if (INVALID_SOCKET == device->dac_socket)
			break;

		if (zet017_socket_wait_connect(device, &device->dac_socket) != 0)
			break;

		if (zet017_socket_handshake(device, &device->dac_socket) != 0)
			break;

		return 0;
	}

	return -1;
}

static void zet017_wakeup_socket_cleanup(struct zet017_device* device) {
	if (device->wakeup_socket[0] != INVALID_SOCKET) {
		close_socket(device->wakeup_socket[0]);
		device->wakeup_socket[0] = INVALID_SOCKET;
	}
	if (device->wakeup_socket[1] != INVALID_SOCKET) {
		close_socket(device->wakeup_socket[1]);
		device->wakeup_socket[1] = INVALID_SOCKET;
	}
}

static int zet017_wakeup_socket_init(struct zet017_device* device) {
#if defined(ZET017_TCP_WINDOWS)
	socket_t listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listener == INVALID_SOCKET)
		return -1;

	for (;;) {
		int optval = 1;
		int optlen = sizeof(optval);
		(void)setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, optlen);

		struct sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		addr.sin_port = 0;
		if (bind(listener, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
			break;

		memset(&addr, 0, sizeof(addr));
		socklen_t addrlen = sizeof(addr);
		if (getsockname(listener, (struct sockaddr*)&addr, &addrlen) == SOCKET_ERROR)
			break;

		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		if (listen(listener, 1) == SOCKET_ERROR)
			break;

		device->wakeup_socket[0] = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (device->wakeup_socket[0] == INVALID_SOCKET)
			break;

		if (connect(device->wakeup_socket[0], (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
			break;

		device->wakeup_socket[1] = accept(listener, NULL, NULL);
		if (device->wakeup_socket[1] == INVALID_SOCKET)
			break;

		closesocket(listener);

		return 0;
	}
	closesocket(listener);
#else
	if (socketpair(AF_LOCAL, SOCK_STREAM, 0, device->wakeup_socket) == 0)
		return 0;
#endif

	zet017_wakeup_socket_cleanup(device);

	return -2;
}

static void zet017_device_wakeup(struct zet017_device* device) {
	char buf = 'x';
	send(device->wakeup_socket[0], &buf, sizeof(buf), 0);
}

static int zet017_device_process_wakeup(struct zet017_device* device) {
	for (;;) {
		fd_set rfds;
		FD_ZERO(&rfds);
		FD_SET(device->wakeup_socket[1], &rfds);

		struct timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 0;

		int r = select((int)device->wakeup_socket[1] + 1, &rfds, NULL, NULL, &tv);
		if (r == -1)
			return -1;

		if (r == 0)
			return 0;

		if (FD_ISSET(device->wakeup_socket[1], &rfds)) {
			char buf;
			recv(device->wakeup_socket[1], &buf, 1, 0);
		}
	}

	return 0;
}

static void zet017_device_close(struct zet017_device* device) {
	zet017_wakeup_socket_cleanup(device);
	if (device->cmd_socket != INVALID_SOCKET) {
		close_socket(device->cmd_socket);
		device->cmd_socket = INVALID_SOCKET;
	}
	if (device->adc_socket != INVALID_SOCKET) {
		close_socket(device->adc_socket);
		device->adc_socket = INVALID_SOCKET;
	}
	if (device->dac_socket != INVALID_SOCKET) {
		close_socket(device->dac_socket);
		device->dac_socket = INVALID_SOCKET;
	}

	device->is_connected = 0;
}

static void zet017_device_destroy(struct zet017_device* device) {
	device->running = 0;
	zet017_device_wakeup(device);
#if defined(ZET017_TCP_WINDOWS)
	WaitForSingleObject(device->work_thread, INFINITE);
	CloseHandle(device->work_thread);
#else
	pthread_join(device->work_thread, NULL);
#endif
	zet017_device_close(device);
	mutex_destroy(&device->state_mutex);
	mutex_destroy(&device->info_mutex);
	mutex_destroy(&device->config_mutex);
	mutex_destroy(&device->adc_data.mutex);
	mutex_destroy(&device->dac_data.mutex);
	mutex_destroy(&device->command.mutex);
	cond_destroy(&device->command.cond);

	free(device);
}

static int zet017_device_wait_stop(struct zet017_device* device, union zet017_packet* packet) {
	int counter = 0;
	for (;;) {
		fd_set rfds;
		FD_ZERO(&rfds);
		FD_SET(device->wakeup_socket[1], &rfds);
		FD_SET(device->adc_socket, &rfds);

		int nfds = (int)(device->adc_socket > device->wakeup_socket[1] ? device->adc_socket : device->wakeup_socket[1]);

		struct timeval tv;
		tv.tv_sec = 2;
		tv.tv_usec = 0;

		int r = select(nfds + 1, &rfds, NULL, NULL, &tv);
		if (r == -1 || r == 0) {
			zet017_device_close(device);
			break;
		}

		if (r > 0) {
			if (FD_ISSET(device->adc_socket, &rfds)) {
				r = recv(device->adc_socket, packet->raw, sizeof(*packet), 0);
				if (r <= 0) {
					zet017_device_close(device);
					break;
				}
				if (r == sizeof(*packet)) {
					for (int i = 0; i < r; ++i) {
						if (packet->raw[i] != 0)
							break;

						if (i == r - 1)
							return 0;
					}
				}
				if (++counter > 10) {
					zet017_device_close(device);
					break;
				}
			}

			if (FD_ISSET(device->wakeup_socket[1], &rfds)) {
				char buf;
				if (recv(device->wakeup_socket[1], &buf, 1, 0) <= 0) {
					zet017_device_close(device);
					break;
				}
			}
		}
	}

	return -1;
}

static int zet017_device_process_command(struct zet017_device* device, union zet017_packet* packet) {
	fd_set rfds;
	FD_ZERO(&rfds);
	FD_SET(device->wakeup_socket[1], &rfds);

	fd_set wfds;
	FD_ZERO(&wfds);
	FD_SET(device->cmd_socket, &wfds);

	int nfds = (int)(device->cmd_socket > device->wakeup_socket[1] ? device->cmd_socket : device->wakeup_socket[1]);

	struct timeval tv;
	tv.tv_sec = 10;
	tv.tv_usec = 0;

	int r = select(nfds + 1, &rfds, &wfds, NULL, &tv);
	if (r == -1 || r == 0)
		return -1;

	if (FD_ISSET(device->wakeup_socket[1], &rfds)) {
		char buf;
		recv(device->wakeup_socket[1], &buf, 1, 0);
		return -2;
	}

	if (FD_ISSET(device->cmd_socket, &wfds)) {
		r = send(device->cmd_socket, packet->raw, sizeof(*packet), 0);
		if (r != sizeof(*packet))
			return -2;
	}
	else
		return -3;

	int data_ptr = 0;
	for (;;) {
		FD_ZERO(&rfds);
		FD_SET(device->wakeup_socket[1], &rfds);
		FD_SET(device->cmd_socket, &rfds);

		r = select(nfds + 1, &rfds, NULL, NULL, &tv);
		if (r == -1 || r == 0)
			break;

		if (FD_ISSET(device->wakeup_socket[1], &rfds)) {
			char buf;
			recv(device->wakeup_socket[1], &buf, 1, 0);
			break;
		}

		if (FD_ISSET(device->cmd_socket, &rfds)) {
			int len = sizeof(*packet) - data_ptr;
			r = recv(device->cmd_socket, packet->raw + data_ptr, len, 0);
			if (r <= 0)
				break;

			data_ptr += r;
			if (data_ptr == sizeof(*packet))
				return 0;
		}
	}

	return -4;
}

static void zet017_device_update_info(struct zet017_device* device, union zet017_packet* packet) {
	memcpy(&device->device_info, &packet->info, sizeof(struct zet017_device_info));

	device->adc_dac_data.sample_rate_adc = zet017_get_sample_rate_adc(device->device_info.mode_adc);
	device->adc_dac_data.work_channel_adc = device->device_info.work_channel_adc;
	device->adc_dac_data.sample_size_adc =
		(uint16_t)(device->device_info.type_data_adc == 0 ? sizeof(int16_t) : sizeof(int32_t));
	device->adc_dac_data.sample_rate_dac = zet017_get_sample_rate_dac(device->device_info.rate_dac);
	device->adc_dac_data.work_channel_dac = device->device_info.work_channel_dac;
	device->adc_dac_data.sample_size_dac =
		(uint16_t)(device->device_info.type_data_dac == 0 ? sizeof(int16_t) : sizeof(int32_t));

	mutex_lock(&device->info_mutex);
	strcpy(device->info.name, device->device_info.device_name);
	device->info.serial = device->device_info.serial;
	strcpy(device->info.version, device->device_info.version_dsp);
	mutex_unlock(&device->info_mutex);

	mutex_lock(&device->config_mutex);
	device->config.sample_rate_adc = zet017_get_sample_rate_adc(device->device_info.mode_adc);
	device->config.sample_rate_dac = zet017_get_sample_rate_dac(device->device_info.rate_dac);
	device->config.mask_channel_adc = device->device_info.mask_channel_adc;
	device->config.mask_icp = device->device_info.mask_icp;
	for (uint32_t i = 0; i < 8; ++i)
		device->config.gain[i] = zet017_get_gain(device->device_info.amplify_code[i]);
	if (device->device_info.quantity_channel_adc == 4) {
		device->config.mask_channel_adc =
			((device->device_info.mask_channel_adc & 0x02) >> 1) +
			((device->device_info.mask_channel_adc & 0x08) >> 2) +
			((device->device_info.mask_channel_adc & 0x20) >> 3) +
			((device->device_info.mask_channel_adc & 0x80) >> 4);
		device->config.mask_icp =
			((device->device_info.mask_icp & 0x02) >> 1) +
			((device->device_info.mask_icp & 0x08) >> 2) +
			((device->device_info.mask_icp & 0x20) >> 3) +
			((device->device_info.mask_icp & 0x80) >> 4);
		for (uint32_t i = 0; i < 4; ++i)
			device->config.gain[i] = zet017_get_gain(device->device_info.amplify_code[i * 2 + 1]);
	}
	mutex_unlock(&device->config_mutex);

	mutex_lock(&device->state_mutex);
	uint32_t sample_size = (uint32_t)(device->device_info.type_data_adc == 0 ? sizeof(int16_t) : sizeof(int32_t));
	device->state.buffer_size_adc = ZET017_ADC_BUFFER_SIZE / sample_size / device->device_info.work_channel_adc;
	sample_size = (uint32_t)(device->device_info.type_data_dac == 0 ? sizeof(int16_t) : sizeof(int32_t));
	device->state.buffer_size_dac = ZET017_DAC_BUFFER_SIZE / sample_size;
	if (device->device_info.work_channel_dac != 0)
		device->state.buffer_size_dac /= device->device_info.work_channel_dac;
	mutex_unlock(&device->state_mutex);
}

static void zet017_device_update_adc_dac_info(struct zet017_device* device) {
	mutex_lock(&device->adc_data.mutex);

	device->adc_data.channel_quantity = device->device_info.work_channel_adc;
	device->adc_data.channel_mask = device->device_info.mask_channel_adc;
	device->adc_data.sample_size = device->device_info.type_data_adc == 0 ? sizeof(int16_t) : sizeof(int32_t);
	memcpy(device->adc_data.amplify_code, device->device_info.amplify_code, sizeof(device->device_info.amplify_code));
	if (device->device_info.quantity_channel_adc == 4) {
		device->adc_data.channel_mask =
			((device->device_info.mask_channel_adc & 0x02) >> 1) +
			((device->device_info.mask_channel_adc & 0x08) >> 2) +
			((device->device_info.mask_channel_adc & 0x20) >> 3) +
			((device->device_info.mask_channel_adc & 0x80) >> 4);
		for (uint32_t i = 0; i < 4; ++i)
			device->adc_data.amplify_code[i] = device->device_info.amplify_code[i * 2 + 1];
	}

	uint16_t quantity_channel_adc = device->device_info.quantity_channel_adc - device->device_info.quantity_channel_virt;
	for (uint32_t i = 0; i < quantity_channel_adc; ++i) {
		uint32_t* dummy = (uint32_t*)device->correction.amplify[i];
		if (*dummy == 0) {
			float resolution = device->device_info.resolution_adc_def;
			dummy = (uint32_t*)(device->device_info.resolution_adc + i);
			if (quantity_channel_adc == 4)
				dummy = (uint32_t*)(device->device_info.resolution_adc + i + i + 1);
			if (*dummy != 0)
				resolution = *(float*)dummy;
			device->adc_data.resolution[i][0] = resolution;
			device->adc_data.resolution[i][1] = resolution / 10.f;
			device->adc_data.resolution[i][2] = resolution / 100.f;
		}
		else {
			device->adc_data.resolution[i][0] = *(float*)dummy;
			device->adc_data.resolution[i][1] = device->adc_data.resolution[i][0] / device->correction.amplify[i][1];
			device->adc_data.resolution[i][2] = device->adc_data.resolution[i][0] / device->correction.amplify[i][2];
		}
	}

	mutex_unlock(&device->adc_data.mutex);

	mutex_lock(&device->dac_data.mutex);

	device->dac_data.channel_quantity = device->device_info.work_channel_dac;
	device->dac_data.channel_mask = device->device_info.mask_channel_dac;
	device->dac_data.sample_size = device->device_info.type_data_dac == 0 ? sizeof(int16_t) : sizeof(int32_t);

	uint16_t quantity_channel_dac = device->device_info.quantity_channel_dac;
	for (uint32_t i = 0; i < quantity_channel_dac; ++i) {
		uint32_t* dummy = (uint32_t*)(device->correction.reduction + i);
		if (*dummy == 0) {
			float resolution = device->device_info.resolution_dac_def;
			dummy = (uint32_t*)(device->device_info.resolution_dac + i);
			if (*dummy != 0)
				resolution = *(float*)dummy;
			device->dac_data.resolution[i] = resolution;
		}
		else
			device->dac_data.resolution[i] = *(float*)dummy;
	}

	mutex_unlock(&device->dac_data.mutex);
}

static int zet017_device_get_info_cmd(struct zet017_device* device, union zet017_packet* packet) {
	memset(packet, 0x0, sizeof(*packet));
	packet->info.command = ZET017_CMD_GET_INFO;
	if (zet017_device_process_command(device, packet) != 0)
		return -1;

	zet017_device_update_info(device, packet);

	return 0;
}

static int zet017_device_put_info_cmd(struct zet017_device* device, union zet017_packet* packet) {
	packet->info.command = ZET017_CMD_PUT_INFO;
	if (0 != zet017_device_process_command(device, packet))
		return -1;
	
	zet017_device_update_info(device, packet);

	return 0;
}

static int zet017_device_start_cmd(struct zet017_device* device, union zet017_packet* packet) {
	packet->info.command = ZET017_CMD_PUT_INFO;
	if (0 != zet017_device_process_command(device, packet))
		return -1;

	memset(device->adc_data.buffer, 0x0, ZET017_ADC_BUFFER_SIZE);
	device->adc_data.pointer = 0;

	memset(device->dac_data.buffer, 0x0, ZET017_DAC_BUFFER_SIZE);
	device->dac_data.pointer = 0;

	device->adc_dac_data.adc_count = 0;
	device->adc_dac_data.dac_count = 0;

	zet017_device_update_info(device, packet);

	return 0;
}

static int zet017_device_stop_cmd(struct zet017_device* device, union zet017_packet* packet) {
	for (;;) {
		if (device->device_info.start_adc == 0)
			return 0;

		memcpy(&packet->info, &device->device_info, sizeof(struct zet017_device_info));
		packet->info.command = ZET017_CMD_PUT_INFO;
		packet->info.start_adc = -1;
		if (packet->info.start_dac != 0)
			packet->info.start_dac = -1;
		if (0 != zet017_device_process_command(device, packet))
			break;

		if (0 != zet017_device_wait_stop(device, packet))
			break;

		memcpy(&packet->info, &device->device_info, sizeof(struct zet017_device_info));
		packet->info.command = ZET017_CMD_PUT_INFO;
		packet->info.start_adc = 0;
		packet->info.start_dac = 0;
		if (0 != zet017_device_process_command(device, packet))
			break;

		zet017_device_update_info(device, packet);

		return 0;
	}

	return -1;
}

static int zet017_device_read_correction_cmd(struct zet017_device* device, union zet017_packet* packet) {
	for (;;) {
		memset(&packet->cmd, 0x0, sizeof(struct zet017_command_info));
		packet->cmd.command = ZET017_CMD_READ_CORRECTION;
		packet->cmd.error = 1;
		packet->cmd.size = sizeof(struct zet017_correction_info);
		if (0 != zet017_device_process_command(device, packet))
			break;

		if (packet->cmd.command == ZET017_CMD_READ_CORRECTION)
			memcpy(&device->correction, packet->cmd.data.u8, sizeof(struct zet017_correction_info));
		else
			memset(&device->correction, 0x0, sizeof(struct zet017_correction_info));

		return 0;
	}

	return -1;
}

static int zet017_device_connect(struct zet017_device* device) {
	for (;;) {
		if (zet017_wakeup_socket_init(device) != 0)
			break;

		if (zet017_socket_cmd_connect(device) != 0)
			break;

		if (zet017_socket_adc_connect(device) != 0)
			break;

		if (zet017_socket_dac_connect(device) != 0)
			break;

		memset(device->adc_data.buffer, 0x0, ZET017_ADC_BUFFER_SIZE);
		device->adc_data.pointer = 0;

		memset(device->dac_data.buffer, 0x0, ZET017_DAC_BUFFER_SIZE);
		device->dac_data.pointer = 0;

		device->adc_dac_data.adc_count = 0;
		device->adc_dac_data.dac_count = 0;

		return 0;
	}

	zet017_device_close(device);

	return -1;
}

static int zet017_device_init(struct zet017_device* device, union zet017_packet* packet) {
	for (;;) {
		if (zet017_device_get_info_cmd(device, packet) != 0)
			break;

		packet->info.start_adc = packet->info.start_dac = 0;
		zet017_set_size_packet_adc(&packet->info);

		if (zet017_device_put_info_cmd(device, packet) != 0)
			break;

		if (zet017_device_read_correction_cmd(device, packet) != 0)
			break;

		zet017_device_update_adc_dac_info(device);

		device->timestamp = zet017_get_timestamp();

		return 0;
	}

	zet017_device_close(device);

	return -1;
}

static void zet017_process_adc_dac(struct zet017_device* device, union zet017_packet* packet) {
	fd_set rfds;
	FD_ZERO(&rfds);
	FD_SET(device->wakeup_socket[1], &rfds);
	FD_SET(device->adc_socket, &rfds);
	FD_SET(device->dac_socket, &rfds);

	int nfds = (int)(device->adc_socket > device->wakeup_socket[1] ? device->adc_socket : device->wakeup_socket[1]);
	if (nfds < (int)device->dac_socket)
		nfds = (int)device->dac_socket;

	int dac = 0;
	fd_set wfds;
	FD_ZERO(&wfds);
	if (device->device_info.start_dac) {
		uint64_t dac_count =
			device->adc_dac_data.adc_count * device->adc_dac_data.sample_rate_dac / device->adc_dac_data.sample_rate_adc;
		if (device->adc_dac_data.dac_count < dac_count + device->adc_dac_data.sample_rate_dac / 5)
			dac = 1;
	}
	if (dac != 0)
		FD_SET(device->dac_socket, &wfds);

	struct timeval tv;
	tv.tv_sec = 10;
	tv.tv_usec = 0;

	int r = select(nfds + 1, &rfds, dac ? &wfds : NULL, NULL, &tv);
	if (r == -1) {
		zet017_device_close(device);
		return;
	}

	if (r > 0) {
		if (FD_ISSET(device->adc_socket, &rfds)) {
			r = recv(device->adc_socket, packet->raw, sizeof(*packet), 0);
			if (r <= 0) {
				zet017_device_close(device);
				return;
			}
			if (r == sizeof(*packet)) {
				mutex_lock(&device->adc_data.mutex);

				uint32_t size = device->device_info.size_packet_adc * 2;
				device->adc_dac_data.adc_count +=
					size / device->adc_dac_data.work_channel_adc / device->adc_dac_data.sample_size_adc;

				if (size <= ZET017_ADC_BUFFER_SIZE - device->adc_data.pointer) {
					memcpy(device->adc_data.buffer + device->adc_data.pointer, packet->raw, size);
					device->adc_data.pointer += size;
					if (device->adc_data.pointer >= ZET017_ADC_BUFFER_SIZE)
						device->adc_data.pointer -= ZET017_ADC_BUFFER_SIZE;
				}
				else {
					size = ZET017_ADC_BUFFER_SIZE - device->adc_data.pointer;
					memcpy(device->adc_data.buffer + device->adc_data.pointer, packet->raw, size);
					uint32_t offset = size;
					size = device->device_info.size_packet_adc * 2 - offset;
					memcpy(device->adc_data.buffer, packet->raw + offset, size);
					device->adc_data.pointer = size;
				}

				mutex_unlock(&device->adc_data.mutex);
			}
		}

		if (FD_ISSET(device->dac_socket, &rfds)) {
			r = recv(device->dac_socket, packet->raw, sizeof(*packet), 0);
			if (r <= 0) {
				zet017_device_close(device);
				return;
			}
		}

		if (dac != 0) {
			if (FD_ISSET(device->dac_socket, &wfds)) {
				uint32_t size = sizeof(*packet);

				mutex_lock(&device->dac_data.mutex);

				if (size <= ZET017_DAC_BUFFER_SIZE - device->dac_data.pointer) {
					memcpy(packet->raw, device->dac_data.buffer + device->dac_data.pointer, size);
					memset(device->dac_data.buffer + device->dac_data.pointer, 0, size);
					device->dac_data.pointer += size;
					if (device->dac_data.pointer >= ZET017_DAC_BUFFER_SIZE)
						device->dac_data.pointer -= ZET017_DAC_BUFFER_SIZE;
				}
				else {
					size = ZET017_DAC_BUFFER_SIZE - device->dac_data.pointer;
					memcpy(packet->raw, device->dac_data.buffer + device->dac_data.pointer, size);
					memset(device->dac_data.buffer + device->dac_data.pointer, 0, size);
					uint32_t offset = size;
					size = sizeof(*packet) - offset;
					memcpy(packet->raw + offset, device->dac_data.buffer, size);
					device->dac_data.pointer = size;
				}

				mutex_unlock(&device->dac_data.mutex);

				r = send(device->dac_socket, packet->raw, sizeof(*packet), 0);
				if (r != sizeof(*packet)) {
					zet017_device_close(device);
					return;
				}
				device->adc_dac_data.dac_count += 
					sizeof(*packet) / device->adc_dac_data.work_channel_dac / device->adc_dac_data.sample_size_dac;
			}
		}

		if (FD_ISSET(device->wakeup_socket[1], &rfds)) {
			char buf;
			if (recv(device->wakeup_socket[1], &buf, 1, 0) <= 0) {
				zet017_device_close(device);
				return;
			}
		}
	}
}

static void zet017_update_state(struct zet017_device* device, union zet017_packet* packet) {
	uint32_t timestamp = zet017_get_timestamp();
	if (timestamp - device->timestamp > 60000) {
		device->timestamp = timestamp;
		if (zet017_device_get_info_cmd(device, packet) != 0)
		{
			zet017_device_close(device);
			return;
		}
	}

	mutex_lock(&device->state_mutex);

	device->state.is_connected = device->is_connected;
	device->state.reconnect = device->reconnect;
	device->state.pointer_adc = device->adc_data.pointer / device->device_info.work_channel_adc;
	if (device->device_info.type_data_adc == 0)
		device->state.pointer_adc /= sizeof(int16_t);
	if (device->device_info.type_data_adc == 1)
		device->state.pointer_adc /= sizeof(int32_t);
	if (device->device_info.work_channel_dac != 0)
		device->state.pointer_dac = device->dac_data.pointer / device->device_info.work_channel_dac;
	else
		device->state.pointer_dac = 0;
	if (device->device_info.type_data_dac == 0)
		device->state.pointer_dac /= sizeof(int16_t);
	if (device->device_info.type_data_dac == 1)
		device->state.pointer_dac /= sizeof(int32_t);

	mutex_unlock(&device->state_mutex);
}

static void zet017_process_command(struct zet017_device* device) {
	mutex_lock(&device->command.mutex);

	if (device->command.state == zet017_command_idle) {
		mutex_unlock(&device->command.mutex);
		return;
	}

	device->command.state = zet017_command_processing;
	device->command.result = -1;
	if (zet017_device_process_wakeup(device) == 0) {
		switch (device->command.command) {
		case zet017_set_config:
			device->command.result = zet017_device_put_info_cmd(device, &device->command.data);
			zet017_device_update_adc_dac_info(device);
			break;
		case zet017_start:
			device->command.result = zet017_device_start_cmd(device, &device->command.data);
			zet017_device_update_adc_dac_info(device);
			break;
		case zet017_stop:
			device->command.result = zet017_device_stop_cmd(device, &device->command.data);
			break;
		default:
			break;
		}
	}
	device->command.state = zet017_command_completed;
	if (device->command.result != 0)
		zet017_device_close(device);

	cond_signal(&device->command.cond);
	mutex_unlock(&device->command.mutex);
}

static THREAD_RETURN zet017_device_thread_func(void* arg) {
	struct zet017_device* device = (struct zet017_device*)arg;
	union zet017_packet packet;
#if !defined(ZET017_TCP_WINDOWS)
	struct timespec ts = { 0, 100000000 };
#endif

	while (device->running) {
		if (device->is_connected)
			zet017_process_adc_dac(device, &packet);
		else {
			if (zet017_device_connect(device) == 0) {
				if (zet017_device_init(device, &packet) == 0) {
					device->is_connected = 1;
					++device->reconnect;
				}
			}

			if (!device->is_connected) {
#if defined(ZET017_TCP_WINDOWS)
				Sleep(100);
#else
				nanosleep(&ts, NULL);
#endif
				continue;
			}
		}

		zet017_process_command(device);

		zet017_update_state(device, &packet);
	}

#if defined(ZET017_TCP_WINDOWS)
	return 0;
#else
	return NULL;
#endif
}

ZET017_TCP_API zet017_server_create(struct zet017_server** server_ptr) {
	if (!server_ptr)
		return -1;

	if (0 != network_init())
		return -2;

	struct zet017_server* server = malloc(sizeof(struct zet017_server));
	if (!server) {
		network_cleanup();
		return -3;
	}

	memset(server, 0, sizeof(struct zet017_server));
	server->devices = NULL;
	server->device_count = 0;
	if (0 != mutex_init(&server->devices_mutex)) {
		free(server);
		network_cleanup();
		return -4;
	}

	*server_ptr = server;

	return 0;
}

ZET017_TCP_API zet017_server_free(struct zet017_server** server_ptr) {
	if (!server_ptr)
		return -1;

	struct zet017_server* server = *server_ptr;

	if (!server)
		return -2;

	mutex_lock(&server->devices_mutex);
	struct zet017_device* current = server->devices;
	while (current != NULL) {
		struct zet017_device* next = current->next;
		zet017_device_destroy(current);
		current = next;
	}
	server->devices = NULL;
	server->device_count = 0;
	mutex_unlock(&server->devices_mutex);
	mutex_destroy(&server->devices_mutex);

	free(server);
	*server_ptr = NULL;

	network_cleanup();

	return 0;
}

ZET017_TCP_API zet017_server_add_device(struct zet017_server* server, const char* ip) {
	if (!server)
		return -1;

	if (!ip)
		return -2;

	mutex_lock(&server->devices_mutex);

	struct zet017_device* existing = server->devices;
	while (existing != NULL) {
		if (strcmp(existing->ip, ip) == 0) {
			mutex_unlock(&server->devices_mutex);
			return -3;
		}
		existing = existing->next;
	}

	struct zet017_device* device = malloc(sizeof(struct zet017_device));
	if (!device) {
		mutex_unlock(&server->devices_mutex);
		return -4;
	}

	for (;;) {
		memset(device, 0, sizeof(struct zet017_device));
		strncpy(device->ip, ip, MAX_IP_LENGTH - 1);
		device->ip[MAX_IP_LENGTH - 1] = '\0';
		strncpy(device->info.ip, ip, MAX_IP_LENGTH - 1);
		device->info.ip[MAX_IP_LENGTH - 1] = '\0';
		device->cmd_socket = device->adc_socket = device->dac_socket = INVALID_SOCKET;
		device->wakeup_socket[0] = device->wakeup_socket[1] = INVALID_SOCKET;
		device->is_connected = 0;
		if (0 != mutex_init(&device->state_mutex))
			break;
		if (0 != mutex_init(&device->info_mutex))
			break;
		if (0 != mutex_init(&device->config_mutex))
			break;
		if (0 != mutex_init(&device->command.mutex))
			break;
		if (0 != mutex_init(&device->adc_data.mutex))
			break;
		if (0 != mutex_init(&device->dac_data.mutex))
			break;
		if (0 != cond_init(&device->command.cond))
			break;
		device->command.state = zet017_command_idle;

		device->running = 1;
#if defined(ZET017_TCP_WINDOWS)
		device->work_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)zet017_device_thread_func, device, 0, NULL);
		if (device->work_thread == NULL)
#else
		if (0 != pthread_create(&device->work_thread, NULL, zet017_device_thread_func, device))
#endif
			break;
		if (server->devices == NULL)
			server->devices = device;
		else {
			for (struct zet017_device* d = server->devices; d != NULL; d = d->next) {
				if (d->next == NULL) {
					d->next = device;
					break;
				}
			}
		}
		++server->device_count;

		mutex_unlock(&server->devices_mutex);

		return 0;
	}

	zet017_device_destroy(device);

	mutex_unlock(&server->devices_mutex);

	return -5;
}

ZET017_TCP_API zet017_server_remove_device(struct zet017_server* server, const char* ip) {
	if (!server)
		return -1;

	if (!ip)
		return -2;

	mutex_lock(&server->devices_mutex);

	struct zet017_device* current = server->devices;
	struct zet017_device* prev = NULL;
	while (current != NULL) {
		if (strcmp(current->ip, ip) == 0) {
			if (prev == NULL)
				server->devices = current->next;
			else
				prev->next = current->next;

			zet017_device_destroy(current);
			--server->device_count;

			mutex_unlock(&server->devices_mutex);
			
			return 0;
		}

		prev = current;
		current = current->next;
	}

	mutex_unlock(&server->devices_mutex);

	return -1;
}

ZET017_TCP_API zet017_device_get_info(struct zet017_server* server, uint32_t number, struct zet017_info* info) {
	if (!info)
		return -1;

	struct zet017_device* device = zet017_get_device(server, number);
	if (device == NULL)
		return -2;

	mutex_lock(&device->info_mutex);
	memcpy(info, &device->info, sizeof(struct zet017_info));
	mutex_unlock(&device->info_mutex);

	return 0;
}

ZET017_TCP_API zet017_device_get_state(struct zet017_server* server, uint32_t number, struct zet017_state* state) {
	if (!state)
		return -1;

	struct zet017_device* device = zet017_get_device(server, number);
	if (device == NULL)
		return -2;

	mutex_lock(&device->state_mutex);

	memcpy(state, &device->state, sizeof(struct zet017_state));

	mutex_unlock(&device->state_mutex);

	return 0;
}

ZET017_TCP_API zet017_device_get_config(struct zet017_server* server, uint32_t number, struct zet017_config* config) {
	if (!config)
		return -1;

	struct zet017_device* device = zet017_get_device(server, number);
	if (device == NULL)
		return -2;

	mutex_lock(&device->config_mutex);
	memcpy(config, &device->config, sizeof(struct zet017_config));
	mutex_unlock(&device->config_mutex);

	return 0;
}

ZET017_TCP_API zet017_device_set_config(
	struct zet017_server* server, uint32_t number, const struct zet017_config* config) {
	struct zet017_device* device = zet017_get_device(server, number);
	if (device == NULL)
		return -1;

	mutex_lock(&device->state_mutex);
	uint16_t is_connected = device->state.is_connected;
	mutex_unlock(&device->state_mutex);
	if (!is_connected)
		return -2;

	if (!config)
		return -3;

	mutex_lock(&device->command.mutex);

	memcpy(&device->command.data.info, &device->device_info, sizeof(struct zet017_device_info));
	device->command.data.info.mode_adc = zet017_get_mode_adc(config->sample_rate_adc);
	device->command.data.info.rate_dac = zet017_get_rate_dac(config->sample_rate_dac);
	device->command.data.info.mask_channel_adc = config->mask_channel_adc;
	device->command.data.info.mask_icp = config->mask_icp;
	for (uint32_t i = 0; i < 8; ++i)
		device->command.data.info.amplify_code[i] = zet017_get_amplify_code(config->gain[i]);
	if (device->command.data.info.quantity_channel_adc == 4) {
		device->command.data.info.mask_channel_adc = 
			((config->mask_channel_adc & 0x1) << 1) +
			((config->mask_channel_adc & 0x2) << 2) +
			((config->mask_channel_adc & 0x4) << 3) +
			((config->mask_channel_adc & 0x8) << 4);
		device->command.data.info.mask_icp =
			((config->mask_icp & 0x1) << 1) +
			((config->mask_icp & 0x2) << 2) +
			((config->mask_icp & 0x4) << 3) +
			((config->mask_icp & 0x8) << 4);
		for (uint32_t i = 0; i < 8; ++i)
			device->command.data.info.amplify_code[i] = zet017_get_amplify_code(config->gain[i / 2]);
	}
	zet017_set_size_packet_adc(&device->command.data.info);

	device->command.command = zet017_set_config;
	device->command.result = 0;
	device->command.state = zet017_command_requested;
	zet017_device_wakeup(device);

	while (device->command.state != zet017_command_completed)
		cond_wait(&device->command.cond, &device->command.mutex);

	device->command.state = zet017_command_idle;
	int r = device->command.result;

	mutex_unlock(&device->command.mutex);

	return r;
}

ZET017_TCP_API zet017_device_start(struct zet017_server* server, uint32_t number, uint32_t dac) {
	struct zet017_device* device = zet017_get_device(server, number);
	if (device == NULL)
		return -1;

	mutex_lock(&device->state_mutex);
	uint16_t is_connected = device->state.is_connected;
	mutex_unlock(&device->state_mutex);
	if (!is_connected)
		return -2;

	mutex_lock(&device->command.mutex);

	if (device->device_info.start_adc) {
		mutex_unlock(&device->command.mutex);
		return 0;
	}
	memcpy(&device->command.data.info, &device->device_info, sizeof(struct zet017_device_info));
	device->command.data.info.start_adc = 1;
	device->command.data.info.start_dac = (int16_t)dac;
	memset(&device->command.data.info.atten, 0xff, sizeof(device->command.data.info.atten));
	device->command.data.info.atten_speed = 0;

	device->command.command = zet017_start;
	device->command.result = 0;
	device->command.state = zet017_command_requested;
	zet017_device_wakeup(device);

	while (device->command.state != zet017_command_completed)
		cond_wait(&device->command.cond, &device->command.mutex);

	device->command.state = zet017_command_idle;
	int r = device->command.result;

	mutex_unlock(&device->command.mutex);

	return r;
}

ZET017_TCP_API zet017_device_stop(struct zet017_server* server, uint32_t number) {
	struct zet017_device* device = zet017_get_device(server, number);
	if (device == NULL)
		return -1;

	mutex_lock(&device->state_mutex);
	uint16_t is_connected = device->state.is_connected;
	mutex_unlock(&device->state_mutex);
	if (!is_connected)
		return -2;

	mutex_lock(&device->command.mutex);

	device->command.command = zet017_stop;
	device->command.result = 0;
	device->command.state = zet017_command_requested;
	zet017_device_wakeup(device);

	while (device->command.state != zet017_command_completed)
		cond_wait(&device->command.cond, &device->command.mutex);

	device->command.state = zet017_command_idle;
	int r = device->command.result;

	mutex_unlock(&device->command.mutex);

	return 0;
}

ZET017_TCP_API zet017_channel_get_data(
	struct zet017_server* server, uint32_t number, uint32_t channel, uint32_t pointer, float* data, uint32_t size) {
	struct zet017_device* device = zet017_get_device(server, number);
	if (device == NULL)
		return -1;

	if (channel >= ZET017_MAX_CHANNELS_ADC)
		return -2;

	mutex_lock(&device->state_mutex);
	uint16_t is_connected = device->state.is_connected;
	mutex_unlock(&device->state_mutex);
	if (!is_connected)
		return -3;

	if (data == NULL)
		return -4;

	mutex_lock(&device->adc_data.mutex);

	if (!(device->adc_data.channel_mask & (1 << channel))) {
		mutex_unlock(&device->adc_data.mutex);
		return -5;
	}

	uint32_t step = device->adc_data.sample_size * device->adc_data.channel_quantity;
	uint32_t channel_size = ZET017_ADC_BUFFER_SIZE / step;
	if (pointer >= channel_size || size > channel_size) {
		mutex_unlock(&device->adc_data.mutex);
		return -6;
	}

	uint32_t offset = 0;
	for (uint32_t i = 0; i < channel; ++i) {
		if (device->adc_data.channel_mask & (1 << i))
			offset += device->adc_data.sample_size;
	}

	uint32_t p = pointer;
	if (p >= size)
		p -= size;
	else
		p = p + channel_size - size;
	p *= step;
	p += offset;
	for (uint32_t i = 0; i < size; ++i, p += step) {
		if (p >= ZET017_ADC_BUFFER_SIZE)
			p -= ZET017_ADC_BUFFER_SIZE;

		if (device->adc_data.sample_size == sizeof(int16_t))
			data[i] = (float)(*(int16_t*)(device->adc_data.buffer + p));
		else if (device->adc_data.sample_size == sizeof(int32_t))
			data[i] = (float)(*(int32_t*)(device->adc_data.buffer + p));
		data[i] *= device->adc_data.resolution[channel][device->adc_data.amplify_code[channel]];
	}

	mutex_unlock(&device->adc_data.mutex);

	return 0;
}

ZET017_TCP_API zet017_channel_put_data(
	struct zet017_server* server, uint32_t number, uint32_t channel, uint32_t pointer, float* data, uint32_t size) {
	struct zet017_device* device = zet017_get_device(server, number);
	if (device == NULL)
		return -1;

	if (channel >= ZET017_MAX_CHANNELS_DAC)
		return -2;

	mutex_lock(&device->state_mutex);
	uint16_t is_connected = device->state.is_connected;
	mutex_unlock(&device->state_mutex);
	if (!is_connected)
		return -3;

	if (data == NULL)
		return -4;

	mutex_lock(&device->dac_data.mutex);

	if (!(device->dac_data.channel_mask & (1 << channel))) {
		mutex_unlock(&device->dac_data.mutex);
		return -5;
	}

	uint32_t step = device->dac_data.sample_size * device->dac_data.channel_quantity;
	uint32_t channel_size = ZET017_DAC_BUFFER_SIZE / step;
	if (pointer >= channel_size || size > channel_size) {
		mutex_unlock(&device->dac_data.mutex);
		return -6;
	}

	uint32_t offset = 0;
	for (uint32_t i = 0; i < channel; ++i) {
		if (device->dac_data.channel_mask & (1 << i))
			offset += device->dac_data.sample_size;
	}

	uint32_t p = pointer;
	if (p >= size)
		p -= size;
	else
		p = p + channel_size - size;
	p *= step;
	p += offset;
	for (uint32_t i = 0; i < size; ++i, p += step) {
		if (p >= ZET017_DAC_BUFFER_SIZE)
			p -= ZET017_DAC_BUFFER_SIZE;

		if (device->dac_data.sample_size == sizeof(int16_t))
			*(int16_t*)(device->dac_data.buffer + p) = (int16_t)(data[i] / device->dac_data.resolution[channel]);
		else if (device->dac_data.sample_size == sizeof(int32_t))
			*(int32_t*)(device->dac_data.buffer + p) = (int32_t)(data[i] / device->dac_data.resolution[channel]);
	}

	mutex_unlock(&device->dac_data.mutex);

	return 0;
}
