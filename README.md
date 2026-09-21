# Ecowitt-WH34-transmitter

Functional prototype emulating Ecowitt WN34 temperature sensors using a Lolin D32 and RFM69HCW.

Reception has been verified with both `rtl_433` and an Ecowitt GW3000 gateway.

## Current behavior

- Emulates six independent WN34L sensors with unique 24-bit IDs.
- Uses the verified WN34 868.30 MHz packet format.
- Sends each virtual sensor once every 77 seconds.
- Staggers the six sensors across the 77-second period instead of transmitting them as one burst.
- Sends every packet twice with a 30 ms gap.
- Rejects invalid sensor IDs and temperatures outside the supported -40 to +60 °C range.
- Reports radio initialization, configuration and transmission errors over Serial.
- Serial monitor speed: 115200 baud.

The current temperatures are simulated in `src/main.cpp`. They can later be replaced with readings from real sensors such as DS18B20 devices.

See [CHANGELOG.md](CHANGELOG.md) for changes.
