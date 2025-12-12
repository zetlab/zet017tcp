import signal
import time
import sys
import math
from ctypes import (c_float, c_double, Structure)
from zet017tcp import zet017tcp

device_ip = "192.168.1.100"
running = False

class signal_data(Structure):
    _fields_ = [
        ("sine_ampl", c_double),
        ("sine_freq", c_double),
        ("sine_phase", c_double),
        ("sine_dphase", c_double)
    ]

def signal_handler(sig, frame):
    global running
    if sig == signal.SIGINT:
        running = False

def calculate_mean(data, size):
    if size == 0:
        return 0.0

    total = sum(data[:size])
    return total / size

def generate_signal(data, size, sig_data):
    for i in range(size):
        data[i] = sig_data.sine_ampl * math.sin(sig_data.sine_phase)
        sig_data.sine_phase += sig_data.sine_dphase
        if sig_data.sine_phase >= 2. * math.pi:
            sig_data.sine_phase -= 2. * math.pi

def main():
    global running

    print("start: example of working with ZET 017 device via TCP/IP")

    running = True
    signal.signal(signal.SIGINT, signal_handler)

    zet017 = None

    try:
        zet017 = zet017tcp()
    except Exception as e:
        print("end: create zet017 server object error")
        return -1

    result = zet017.init()
    if not result:
        print("end: create zet017 server object error")
        return -1

    result = zet017.add_device(device_ip)
    if not result:
        print("end: add device {device_ip} error")
        return -2

    number = 0
    configured = False
    counter = 0

    sample_rate_adc = 25000
    portion_data_adc = sample_rate_adc
    mask_channel_adc = 0x0e
    mask_icp = 0x02
    channel = 3
    gain = [1, 1, 1, 100, 1, 1, 1, 1]
    pointer_adc = 0
    adc_data = (c_float * portion_data_adc)()

    sample_rate_dac = 50000
    portion_data_dac = int(sample_rate_dac / 10)
    advance_data_dac = int(sample_rate_dac / 2)
    channel_dac = 0
    pointer_dac = sample_rate_dac + portion_data_dac
    dac_data = (c_float * advance_data_dac)()

    sig_data = signal_data()
    sig_data.sine_ampl = 1.
    sig_data.sine_freq = 1011.213
    sig_data.sine_phase = 0.
    sig_data.sine_dphase = sig_data.sine_freq / sample_rate_dac * 2. * math.pi

    state = state_prev = {
        'connected': False,
        'reconnect' : 0,
        'pointer_adc': 0,
        'buffer_size_adc': 0,
        'pointer_dac': 0,
        'buffer_size_dac': 0
    }

    try:
        while running:
            state = zet017.get_device_state(number)
            if state:
                if state['connected'] != state_prev.get('connected', False) or state['reconnect'] != state_prev.get('reconnect', 0):
                    info = zet017.get_device_info(number)
                    if info:
                        if state['connected']:
                            print("%s: connected device %s s/n %d (ver. %s)" % (info['ip'], info['name'], info['serial'], info['version']))
                        else:
                            print("%s: disconnected device %s s/n %d" % (info['ip'], info['name'], info['serial']))

                    configured = False
                    counter = 0
                    pointer_adc = 0
                    pointer_dac = sample_rate_dac + portion_data_dac
                    sig_data.sine_phase = 0
                state_prev = state

            if state['connected']:
                if not configured:
                    config = zet017.get_device_config(number)
                    if config:
                        config['sample_rate_adc'] = sample_rate_adc
                        config['mask_channel_adc'] = mask_channel_adc
                        config['mask_icp'] = mask_icp
                        config['sample_rate_dac'] = sample_rate_dac
                        for i in range(8):
                            config['gain'][i] = gain[i]

                        if zet017.set_device_config(number, config):
                            print("%s: %s s/n %d: device configured" % (info['ip'], info['name'], info['serial']))
                            if zet017.start_device(number, 1):
                                print("%s: %s s/n %d: device started" % (info['ip'],info['name'], info['serial']))
                                configured = True

                size = 0
                if state['pointer_adc'] > pointer_adc:
                    size = state['pointer_adc']  - pointer_adc
                elif state['pointer_adc'] < pointer_adc:
                    size = state['buffer_size_adc'] + state['pointer_adc'] - pointer_adc

                if size >= portion_data_adc:
                    pointer_adc += portion_data_adc
                    if pointer_adc >= state['buffer_size_adc']:
                        pointer_adc -= state['buffer_size_adc']
                    if zet017.get_channel_data(number, channel, pointer_adc, adc_data, portion_data_adc):
                        mean = calculate_mean(adc_data, portion_data_adc)
                        counter += 1
                        print("%s: %s s/n %d: channel %d: %d sec: mean value %.6f V" % (
                            info['ip'], info['name'], info['serial'], channel, counter, mean))

                while True:
                    if pointer_dac >= state['pointer_dac']:
                        size = pointer_dac - state['pointer_dac']
                    elif pointer_dac < state['pointer_dac']:
                        size = pointer_dac + state['buffer_size_dac'] - state['pointer_dac']
                    if size < portion_data_dac + advance_data_dac:
                        pointer_dac += portion_data_dac
                        if pointer_dac >= state['buffer_size_dac']:
                            pointer_dac -= state['buffer_size_dac']

                        generate_signal(dac_data, portion_data_dac, sig_data)
                        zet017.put_channel_data(number, channel_dac, pointer_dac, dac_data, portion_data_dac)
                    else:
                        break

            time.sleep(0.1)
    except KeyboardInterrupt:
        pass

    if state['connected'] and configured:
        if zet017.stop_device(number):
            print("%s: %s s/n %d: device stopped" % (info['ip'], info['name'], info['serial']))
        else:
            print("%s: %s s/n %d: stop device error" % (info['ip'], info['name'], info['serial']))

    if not zet017.cleanup():
        print("zet017 server object free error")

    print("end: example of working with ZET 017 device via TCP/IP")

    return 0

if __name__ == "__main__":
    sys.exit(main())
