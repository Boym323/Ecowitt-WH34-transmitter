# Changelog

All notable changes to this project are documented in this file.

## 2026-09-21

### Changed
- Replaced the blocking 60-second transmission loop with independent 77-second schedules for each virtual WN34 sensor.
- Spread the six sensor transmissions evenly across the 77-second period to reduce bursty RF traffic.
- Preserved the existing WN34 packet format, sync word, bitrate, deviation, bandwidth and transmit power.
- Changed the simulated battery value to 1.50 V (`0x4B`).
- Added `monitor_speed = 115200` to PlatformIO configuration.

### Fixed
- Added validation for non-finite and out-of-range temperatures before encoding.
- Added validation that sensor IDs fit into the 24-bit WN34 ID field.
- Temperature encoding now rounds to the nearest 0.1 °C instead of truncating.
- Added explicit error handling for radio initialization and configuration.
- Added error reporting for both RF transmission attempts.
- Scheduler deadlines are advanced from the previous deadline to avoid timing drift.
- Scheduler timing is rollover-safe for `millis()`.

### Internal
- Replaced repeated hard-coded sensor count values with `SENSOR_COUNT`.
- Made CRC and checksum functions accept const input buffers.
- Split packet construction and RF transmission into separate functions for easier maintenance and future testing.
