
import os
import sys
from ctypes import (
    c_char, c_char_p, c_uint16, c_int, c_uint32, c_float, c_uint64,
    Structure, POINTER, byref, CDLL
)

# Structures
class zet017_config(Structure):
    _fields_ = [
        ("sample_rate_adc", c_uint32),
        ("sample_rate_dac", c_uint32),
        ("mask_channel_adc", c_uint32),
        ("mask_icp", c_uint32),
        ("gain", c_uint32 * 8)
    ]

class zet017_info(Structure):
    _fields_ = [
        ("ip", c_char * 16),
        ("name", c_char * 16),
        ("serial", c_uint32),
        ("version", c_char * 32)
    ]

class zet017_state(Structure):
    _fields_ = [
        ("connected", c_uint16),
        ("reconnect", c_uint64),
        ("pointer_adc", c_uint32),
        ("buffer_size_adc", c_uint32),
        ("pointer_dac", c_uint32),
        ("buffer_size_dac", c_uint32)
    ]

# Opaque pointer for server
class zet017_server(Structure):
    pass

class zet017tcp:
    """Python wrapper for ZET 017 TCP library"""

    def __init__(self, library_path=None):
        self._server = None

        if sys.platform == "win32":
            lib_name = "zet017tcp.dll"
        else:
            lib_name = "libzet017tcp.so"

        # Use provided path or try to find the library
        if library_path:
            self.lib_path = library_path
        else:
            # Try common library locations
            possible_paths = [
                os.path.join(os.getcwd(), lib_name),
                os.path.join(os.getcwd(), "build", lib_name),
                os.path.join(os.getcwd(), "lib", lib_name),
            ]

            for path in possible_paths:
                if os.path.exists(path):
                    self.lib_path = path
                    break
            else:
                raise FileNotFoundError("Could not find {lib_name} in common locations")

        # Load the library
        try:
            self.lib = CDLL(self.lib_path)
        except Exception as e:
            raise RuntimeError("Failed to load library {self.lib_path}: {e}")

        # Set up function prototypes
        self._setup_function_prototypes()

    def __enter__(self):
        """Context manager entry"""
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        """Context manager exit - cleanup"""
        self.cleanup()

    def _setup_function_prototypes(self):
        """Setup function prototypes with correct argument and return types"""
        # zet017_server_create function
        self.lib.zet017_server_create.argtypes = [POINTER(POINTER(zet017_server))]
        self.lib.zet017_server_create.restype = c_int

        # zet017_server_free function
        self.lib.zet017_server_free.argtypes = [POINTER(POINTER(zet017_server))]
        self.lib.zet017_server_free.restype = c_int

        # zet017_server_add_device function
        self.lib.zet017_server_add_device.argtypes = [POINTER(zet017_server), c_char_p]
        self.lib.zet017_server_add_device.restype = c_int

        # zet017_device_get_state function
        self.lib.zet017_device_get_state.argtypes = [POINTER(zet017_server), c_uint32, POINTER(zet017_state)]
        self.lib.zet017_device_get_state.restype = c_int

        # zet017_device_get_info function
        self.lib.zet017_device_get_info.argtypes = [POINTER(zet017_server), c_uint32, POINTER(zet017_info)]
        self.lib.zet017_device_get_info.restype = c_int

        # zet017_device_get_config function
        self.lib.zet017_device_get_config.argtypes = [POINTER(zet017_server), c_uint32, POINTER(zet017_config)]
        self.lib.zet017_device_get_config.restype = c_int

        # zet017_device_set_config function
        self.lib.zet017_device_set_config.argtypes = [POINTER(zet017_server), c_uint32, POINTER(zet017_config)]
        self.lib.zet017_device_set_config.restype = c_int

        # zet017_device_start function
        self.lib.zet017_device_start.argtypes = [POINTER(zet017_server), c_uint32, c_uint32]
        self.lib.zet017_device_start.restype = c_int

        # zet017_device_stop function
        self.lib.zet017_device_stop.argtypes = [POINTER(zet017_server), c_uint32]
        self.lib.zet017_device_stop.restype = c_int

        # zet017_channel_get_data function
        self.lib.zet017_channel_get_data.argtypes = [
            POINTER(zet017_server), c_uint32, c_uint32, c_uint32, POINTER(c_float), c_uint32
        ]
        self.lib.zet017_channel_get_data.restype = c_int

         # zet017_channel_put_data function
        self.lib.zet017_channel_put_data.argtypes = [
            POINTER(zet017_server), c_uint32, c_uint32, c_uint32, POINTER(c_float), c_uint32
        ]
        self.lib.zet017_channel_put_data.restype = c_int

    def init(self):
        """Initialize the ZET 017 server"""
        server_ptr = POINTER(zet017_server)()
        result = self.lib.zet017_server_create(byref(server_ptr))
        if result == 0:
            self._server = server_ptr
            return True
        return False

    def cleanup(self):
        """Clean up the ZET 017 server"""
        if self._server:
            result = self.lib.zet017_server_free(byref(self._server))
            self._server = None
            return result == 0
        return True

    def add_device(self, ip_address):
        """Add a device to the server"""
        if not self._server:
            raise RuntimeError("Server not initialized")

        ip_bytes = ip_address.encode('utf-8')
        result = self.lib.zet017_server_add_device(self._server, ip_bytes)
        if result == 0:
            return True
        return False

    def get_device_state(self, device_number):
        """Get the state of a device"""
        if not self._server:
            raise RuntimeError("Server not initialized")

        state = zet017_state()
        result =  self.lib.zet017_device_get_state(self._server, device_number, byref(state))
        if result == 0:
            return {
                'connected': bool(state.connected),
                'reconnect': state.reconnect,
                'pointer_adc': state.pointer_adc,
                'buffer_size_adc': state.buffer_size_adc,
                'pointer_dac': state.pointer_dac,
                'buffer_size_dac': state.buffer_size_dac
            }
        return None

    def get_device_info(self, device_number):
        """Get information about a device"""
        if not self._server:
            raise RuntimeError("Server not initialized")

        info = zet017_info()
        result = self.lib.zet017_device_get_info(self._server, device_number, byref(info))
        if result == 0:
            return {
                'ip': info.ip.decode('utf-8').strip('\x00'),
                'name': info.name.decode('utf-8').strip('\x00'),
                'serial': info.serial,
                'version': info.version.decode('utf-8').strip('\x00')
            }
        return None

    def get_device_config(self, device_number):
        """Get the configuration of a device"""
        if not self._server:
            raise RuntimeError("Server not initialized")

        config = zet017_config()
        result = self.lib.zet017_device_get_config(self._server, device_number, byref(config))
        if result == 0:
            return {
                'sample_rate_adc': config.sample_rate_adc,
                'sample_rate_dac': config.sample_rate_dac,
                'mask_channel_adc': config.mask_channel_adc,
                'mask_icp': config.mask_icp,
                'gain': [config.gain[i] for i in range(8)]
            }
        return None

    def set_device_config(self, device_number, config):
        """Set the configuration of a device"""
        if not self._server:
            raise RuntimeError("Server not initialized")

        cfg = zet017_config()
        cfg.sample_rate_adc = config['sample_rate_adc']
        cfg.sample_rate_dac = config['sample_rate_dac']
        cfg.mask_channel_adc = config['mask_channel_adc']
        cfg.mask_icp = config['mask_icp']
        gains = config['gain']
        for i in range(min(8, len(gains))):
            cfg.gain[i] = gains[i]

        result = self.lib.zet017_device_set_config(self._server, device_number, byref(cfg))
        return result == 0

    def start_device(self, device_number, dac):
        """Start data acquisition on a device"""
        if not self._server:
            raise RuntimeError("Server not initialized")

        result = self.lib.zet017_device_start(self._server, device_number, dac)
        return result == 0

    def stop_device(self, device_number):
        """Stop data acquisition on a device"""
        if not self._server:
            raise RuntimeError("Server not initialized")

        result = self.lib.zet017_device_stop(self._server, device_number)
        return result == 0

    def get_channel_data(self, device_number, channel, pointer, data, size):
        """Get data from a specific channel"""
        if not self._server:
            raise RuntimeError("Server not initialized")

        result = self.lib.zet017_channel_get_data(
            self._server, device_number, channel, pointer, data, size
        )
        return result == 0

    def put_channel_data(self, device_number, channel, pointer, data, size):
        """Put data to a specific channel"""
        if not self._server:
            raise RuntimeError("Server not initialized")

        result = self.lib.zet017_channel_put_data(
            self._server, device_number, channel, pointer, data, size
        )
        return result == 0
