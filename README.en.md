# ZET 017 TCP/IP Communication Library

A cross-platform C library for communicating with ZET 017, ZET 038 and ZET 028 devices over TCP/IP.

## Features

- **Cross-platform support**: Windows and Linux/Unix compatibility
- **Multi-device management**: Handle multiple devices simultaneously
- **Real-time data acquisition**: Stream ADC data with configurable sample rates (2.5-50 kHz)
- **Dual-channel DAC support**: Digital-to-analog conversion capabilities
- **Configuration management**: Set and get device parameters programmatically
- **Thread-safe architecture**: Built with mutexes and condition variables
- **TCP/IP communication**: Connect to devices over network interfaces

## Supported Hardware

ZET 017, ZET 038 and ZET 028 series data acquisition systems with:
- Up to 8 ADC channels with programmable gains (1x, 10x, 100x)
- Up to 2 DAC channels
- Configurable sample rates:
  - ADC: 2.5 kHz, 5 kHz, 25 kHz, 50 kHz
  - DAC: Configurable up to 200 kHz
- ICP (Integrated Circuit Piezoelectric) sensor support
- Digital I/O functionality

## Project Structure

```bash
zet017tcp/
├── include/
│ └── zet017tcp.h # Public API header
├── src/
│ └── zet017tcp.c # Library implementation
├── example/
│ └── example_zet017tcp.c # Usage example
├── CMakeLists.txt # Build configuration
└── README.md
```

## Requirements

- **CMake** (version 3.10 or higher)
- **C compiler** with C99 support
- **Platform libraries**:
  - Windows: Winsock2 (`ws2_32`)
  - Linux/Unix: pthreads

## Building

### Using CMake (Recommended)

```bash
# Create build directory
mkdir build
cd build

# Configure project
cmake ..

# Build library and example
cmake --build .
```

## API Overview

### Core Functions

```c
// Server management
zet017_server_create(struct zet017_server** server_ptr);
zet017_server_free(struct zet017_server** server_ptr);

// Device management
zet017_server_add_device(struct zet017_server* server, const char* ip);
zet017_server_remove_device(struct zet017_server* server, const char* ip);

// Device operations
zet017_device_get_info(struct zet017_server* server, uint32_t number, struct zet017_info* info);
zet017_device_get_state(struct zet017_server* server, uint32_t number, struct zet017_state* state);
zet017_device_get_config(struct zet017_server* server, uint32_t number, struct zet017_config* config);
zet017_device_set_config(struct zet017_server* server, uint32_t number, struct zet017_config* config);
zet017_device_start(struct zet017_server* server, uint32_t number);
zet017_device_stop(struct zet017_server* server, uint32_t number);

// Data acquisition
zet017_channel_get_data(struct zet017_server* server, uint32_t number, uint32_t channel, 
                       uint32_t pointer, float* data, uint32_t size);
```

### Data Structures

```c
struct zet017_config {
    uint32_t sample_rate_adc;    // ADC sample rate in Hz
    uint32_t sample_rate_dac;    // DAC sample rate in Hz
    uint32_t mask_channel_adc;   // Bitmask of enabled ADC channels
    uint32_t mask_icp;           // Bitmask of ICP-enabled channels
    uint32_t gain[8];            // Gain settings for each channel (1, 10, or 100)
};

struct zet017_info {
    char ip[16];                 // Device IP address
    char name[16];               // Device name
    uint32_t serial;             // Serial number
    char version[32];            // Firmware version
};

struct zet017_state {
    uint16_t connected;          // Connection status
    uint32_t pointer_adc;        // Current ADC buffer position
    uint32_t buffer_size_adc;    // Total ADC buffer size
    uint32_t pointer_dac;        // Current DAC buffer position
};
```

## Usage Example

```c
#include "zet017tcp.h"
#include <stdio.h>

int main() {
    struct zet017_server* server = NULL;
    
    // Initialize server
    if (zet017_server_create(&server) != 0) {
        fprintf(stderr, "Failed to create server\n");
        return -1;
    }
    
    // Add device
    if (zet017_server_add_device(server, "192.168.1.100") != 0) {
        fprintf(stderr, "Failed to add device\n");
        zet017_server_free(&server);
        return -1;
    }
    
    // Configure device
    struct zet017_config config = {
        .sample_rate_adc = 25000,
        .sample_rate_dac = 0,
        .mask_channel_adc = 0x0E,  // Enable channels 1, 2, 3
        .mask_icp = 0x02,          // Enable ICP on channel 1
        .gain = {1, 10, 100, 1, 1, 1, 1, 1}
    };
    
    if (zet017_device_set_config(server, 0, &config) != 0) {
        fprintf(stderr, "Configuration failed\n");
    }
    
    // Start acquisition
    if (zet017_device_start(server, 0) != 0) {
        fprintf(stderr, "Failed to start acquisition\n");
    }
    
    // Read data
    float data[1000];
    if (zet017_channel_get_data(server, 0, 0, 0, data, 1000) == 0) {
        printf("Successfully read 1000 samples from channel 0\n");
    }
    
    // Clean up
    zet017_device_stop(server, 0);
    zet017_server_free(&server);
    
    return 0;
}
```

## Error Handling

All functions return 0 on success and negative values on error:

## Network Protocol

The library communicates with devices using three TCP ports:

- **Command port**: 1808 - Device configuration and control
- **ADC data port**: 2320 - Analog-to-digital converter data stream
- **DAC data port**: 3344 - Digital-to-analog converter data stream

## Platform Support

### Windows

- Requires Winsock2 library (ws2_32.lib)
- Visual Studio project files can be generated with CMake

### Linux/Unix

- Requires pthreads library
- Standard socket programming interface

## License

This project is licensed under the MIT License. See the COPYING file for details.
