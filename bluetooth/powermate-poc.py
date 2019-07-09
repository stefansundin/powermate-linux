#!/usr/bin/python3
import sys, os, time, binascii
import pygatt

addr = None
# addr = '00:12:34:56:78:90' # Put your device address here and uncomment

uuid_read = "9cf53570-ddd9-47f3-ba63-09acefc60415"
uuid_led  = "847d189e-86ee-4bd2-966f-800832b1259d"

led_value = 0xA0

def update_led():
    global led_value
    if led_value > 0xFF:
        led_value = 0xFF
    elif led_value < 0x00:
        led_value = 0x00
    print("LED: %02X" % led_value)
    device.char_write(uuid_led, bytearray([led_value]), False)

def handle_data(handle, value):
    global led_value
    if value[0] == 0x65:
        print("knob quick press")
        os.system("xdotool key XF86AudioPlay")
    elif value[0] == 0x66:
        print("knob released")
    elif value[0] == 0x67:
        print("counter clockwise turn")
        os.system("xdotool key XF86AudioLowerVolume")
        led_value -= 1
        update_led()
    elif value[0] == 0x68:
        print("clockwise turn")
        os.system("xdotool key XF86AudioRaiseVolume")
        led_value += 1
        update_led()
    elif value[0] == 0x69:
        print("depressed counter clockwise turn")
    elif value[0] == 0x70:
        print("depressed clockwise turn")
    elif value[0] == 0x72:
        print("knob down 1 second")
    elif value[0] == 0x73:
        print("knob down 2 seconds")
    elif value[0] == 0x74:
        print("knob down 3 seconds")
    elif value[0] == 0x75:
        print("knob down 4 seconds")
    elif value[0] == 0x76:
        print("knob down 5 seconds")
    elif value[0] == 0x77:
        print("knob down 6 seconds")
    else:
        print("Received data: %s" % binascii.hexlify(value))

try:
    adapter = pygatt.GATTToolBackend()
    adapter.start()

    if addr == None:
        print("No device address hardcoded, scanning.. please wake the powermate..")
        if os.geteuid() != 0:
            sys.exit("Please run as root to scan for devices.")
        devices = adapter.scan()
        powermates = list(filter(lambda d: d['name'] == 'PowerMate Bluetooth', devices))
        if len(powermates) == 0:
            print("Could not find any PowerMate device..")
        else:
            print(powermates)
            print()
            print("Please hardcode this address in the file like the following:")
            print()
            print("addr = '%s'" % powermates[0]['address'])
            print()
            print("And run this file again without root.")
        adapter.stop()
        sys.exit(1)

    if os.geteuid() == 0:
        print("Warning: root is not required!")
        print()

    print("Connecting.. please wake the powermate..")
    device = adapter.connect(addr)
    print("Connected!")
    update_led()
    device.subscribe(uuid_read, callback=handle_data)

    # Is there a better way to wait for pygatt to exit?
    while True:
        time.sleep(60)
finally:
    adapter.stop()
