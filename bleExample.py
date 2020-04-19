#!/usr/bin/env python3

import time 
import binascii
from bluepy import btle

SCAN_TIMEOUT = 5
LOCAL_NAME_TYPE = 9

class ScanDelegate(btle.DefaultDelegate):
    def __init__(self):
        btle.DefaultDelegate.__init__(self)

    def handleDiscovery(self, dev, isNewDev, isNewData):
        if isNewDev:
            print("Discovered device", dev.addr)
        elif isNewData:
            print("Received new data from", dev.addr)

def scanForName(applicationName, timeout):
    scanner = btle.Scanner().withDelegate(ScanDelegate())
    devices = scanner.scan(timeout)
    print(len(devices))
    devicesOfName = []
    for dev in devices:
        currentName = dev.getValueText(LOCAL_NAME_TYPE) 
        if currentName == applicationName:
            devicesOfName.append(dev)
            print(applicationName + " %s (%s), RSSI=%d dB found!" % (dev.addr, dev.addrType, dev.rssi))
            for (adtype, desc, value) in dev.getScanData():
                print("  %s = %s" % (desc, value))
        else:
            print("Device %s (%s), RSSI=%d dB ignored." % (dev.addr, dev.addrType, dev.rssi))
    print(len(devicesOfName))
    return devicesOfName

if __name__ == "__main__":
    brewControllerUuid = btle.UUID("19B10000-E8F2-537E-4F6C-D104768A1214")
    devices = scanForName("brewController", SCAN_TIMEOUT)
    deviceHandles = []
    # Connect the brewControllers 
    for dev in devices:
        deviceHandles.append(btle.Peripheral(dev.addr))
        print("Services...")
        for svc in deviceHandles[-1].services:
            print(str(svc))
    time.sleep(5)
    # toggle LED for all devices found
    for handle in deviceHandles:
        ledService = handle.getServiceByUUID(brewControllerUuid)
        characteristics = ledService.getCharacteristics("19B10001-E8F2-537E-4F6C-D104768A1214")
        print(len(characteristics))
        statusCharacteristic = characteristics[0]
        ledStatus = statusCharacteristic.read()
        print(ledStatus)
        characteristics = ledService.getCharacteristics("19B10002-E8F2-537E-4F6C-D104768A1214")
        print(len(characteristics))
        switchCharacteristic = characteristics[0]
        if int.from_bytes(ledStatus, byteorder = "big"):
            time.sleep(1)
            print("false")
            switchCharacteristic.write(bytes("\x00", encoding='utf8'))
        else:
            time.sleep(1)
            print("true")
            switchCharacteristic.write(bytes("\x01", encoding='utf8'))
    time.sleep(5)
