# ZET 017 TCP/IP Python Interface Documentation

## Overview

This documentation covers two Python files that provide an interface to work with ZET 017, ZET 038 and ZET 028 devices via TCP/IP:

- ```zet017tcp.py``` - The main wrapper library for the ZET 017 TCP library
- ```zet017_main.py``` - An example application demonstrating usage of the library

## File: zet017tcp.py

### Purpose

This file provides a Python wrapper for the ZET 017 TCP library, allowing Python applications to communicate with ZET 017, ZET 038 and ZET 028 devices over TCP/IP.

### Key Components

#### Data Structures

- ```zet017_config```: Device configuration structure
  - ```sample_rate_adc```: ADC sample rate
  - ```sample_rate_dac```: DAC sample rate
  - ```mask_channel_adc```: ADC channel mask
  - ```mask_icp```: ICP mask
  - ```gain```: Gain values for channels
- ```zet017_info```: Device information structure
  - ```ip```: Device IP address
  - ```name```: Device name
  - ```serial```: Serial number
  - ```version```: Firmware version
- ```zet017_state```: Device state structure
  - ```is_connected```: Connection status
  - ```reconnect```: Reconnect count
  - ```pointer_adc```: ADC buffer pointer
  - ```buffer_size_adc```: ADC buffer size
  - ```pointer_dac```: DAC buffer pointer
  - ```buffer_size_dac```: DAC buffer size

  #### Main Class: zet017tcp

  #### Initialization

```python
def __init__(self, library_path=None)
```

Automatically detects the appropriate library file (.dll on Windows, .so on Linux) and loads it.

#### Key Methods

- ```init()```: Initialize the ZET 017 server
- ```cleanup()```: Clean up the server resources
- ```add_device(ip_address)```: Add a device to the server
- ```get_device_state(device_number)```: Get device state
- ```get_device_info(device_number)```: Get device information
- ```get_device_config(device_number)```: Get device configuration
- ```set_device_config(device_number, config)```: Set device configuration
- ```start_device(device_number, dac)```: Start data acquisition
- ```stop_device(device_number)```: Stop data acquisition
- ```get_channel_data(device_number, channel, pointer, data, size)```: Get data from a specific ADC channel
- ```put_channel_data(device_number, channel, pointer, data, size)```: Put data to a specific DAC channel

## File: zet017_main.py

### Purpose

This file provides a complete example of how to use the zet017tcp library to communicate with a ZET 017 device.

### Key Features

1. **Signal Handling**: Proper handling of SIGINT (Ctrl+C) for graceful shutdown
2. **Device Discovery**: Automatically detects when devices connect/disconnect
3. **Configuration Management**: Sets up device parameters including sample rates and gains
4. **Data Acquisition**: Continuously reads data from specified channels
5. **Data Processing**: Calculates mean values from acquired data

### Main Workflow

1. Initialize the ZET 017 server
2. Add a device by IP address
3. Monitor device connection status
4. Configure device parameters when connected
5. Start data acquisition
6. Continuously read and process data
7. Handle graceful shutdown on interrupt

### Configuration Parameters

- ```sample_rate_adc = 25000```: ADC sample rate (25 kHz)
- ```mask_channel_adc = 0x0e```: ADC channel mask
- ```mask_icp = 0x02```: ICP mask
- ```channel = 3```: Channel to monitor
- ```gain = [1, 1, 1, 100, 1, 1, 1, 1]```: Gain values for each channel

## Usage Example

```python
# Initialize the library
zet017 = zet017tcp()

# Initialize server
if not zet017.init():
    print("Failed to initialize server")
    exit(-1)

# Add device
if not zet017.add_device("192.168.1.100"):
    print("Failed to add device")
    exit(-2)

# Get device info
info = zet017.get_device_info(0)
print(f"Device: {info['name']}, Serial: {info['serial']}")

# Configure device
config = zet017.get_device_config(0)
config['sample_rate_adc'] = 25000
config['mask_channel_adc'] = 0x0e
zet017.set_device_config(0, config)

# Start acquisition
zet017.start_device(0, 0)

# Clean up
zet017.cleanup()
```

## Error Handling

The library provides proper error handling through return values:

- True/False for boolean operations
- None for failed information retrieval
- Exception raising for critical errors

## Platform Support

- **Windows**: Uses zet017tcp.dll
- **Linux**: Uses libzet017tcp.so

The library automatically detects the platform and attempts to find the appropriate library file in common locations.

## Dependencies

- Python 3.x
- ctypes library (standard library)
- ZET 017 TCP library (zet017tcp.dll or libzet017tcp.so)

This interface provides a comprehensive Python solution for working with ZET 017, ZET 038 and ZET 028  devices over TCP/IP, making it easier to integrate these devices into Python-based data acquisition and processing systems.
