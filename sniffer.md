Let me explain network sniffing in simple terms and guide you through the setup:

A sniffer is like a radio that listens to all network traffic (in this case Zigbee packets) being transmitted through the air, even if it's not meant for that device. Think of it like eavesdropping on all conversations in a room.

For Zigbee sniffing, you'll need:
1. A device to capture packets (your spare ESP32-C6)
2. Software to analyze the packets (Wireshark)
3. A way to connect the captured data to Wireshark (pyspinel)

Here's the step-by-step process:

1. First, flash the sniffer firmware to your spare ESP32-C6:
```bash
cd $IDF_PATH/examples/zigbee/esp_zigbee_sniffer
idf.py set-target esp32c6
idf.py flash
```

2. Find your ESP32-C6's serial port (usually something like `/dev/ttyUSB0` or ttyACM0):
```bash
ls /dev/ttyUSB*
```

3. Start capturing packets with pyspinel:
```bash
spinel-cli.py -u /dev/ttyUSB0 -c "ncpversion" -c "scan" -c "monitor 11"
```
(Use channel 11 since that's a common Zigbee channel)

4. Open Wireshark and:
   - Select the "Loop" interface
   - Add a display filter: `wpan`
   - Start capturing

Now when your window sensor transmits data, you should see the packets in Wireshark. This can help you:
- Verify the sensor is actually sending data
- See what type of messages it's sending
- Debug communication issues
- Understand the protocol better

Would you like me to provide more specific details about any of these steps?