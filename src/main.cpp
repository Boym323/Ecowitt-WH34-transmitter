#include <RadioLib.h>
#include <SPI.h>
#include <math.h>

// Pins
#define PIN_CS    5
#define PIN_DIO0  4
#define PIN_RST   16

RF69 radio = new Module(PIN_CS, PIN_DIO0, PIN_RST);

constexpr size_t SENSOR_COUNT = 6;
constexpr uint32_t SENSOR_PERIOD_MS = 77000UL;
constexpr uint8_t BATTERY_LEVEL = 0x4B; // 1.50 V (20 mV units)
constexpr float MIN_TEMPERATURE_C = -40.0f;
constexpr float MAX_TEMPERATURE_C = 60.0f;
constexpr float SIMULATION_STEP_C = 0.1f;
constexpr float SIMULATION_MAX_C = 30.0f;
constexpr float SIMULATION_RESET_C = 20.0f;

// Six unique IDs. The gateway will learn them as separate WN34 channels.
uint32_t sensorIDs[SENSOR_COUNT] = {
  0x00A001,
  0x00A002,
  0x00A003,
  0x00A004,
  0x00A005,
  0x00A006
};

// Simulated temperatures. Replace these with real sensor readings later.
float temperatures[SENSOR_COUNT] = {20, 21, 22, 23, 24, 25};

// Each virtual sensor gets its own 77-second schedule.
uint32_t nextTransmitAt[SENSOR_COUNT];

uint8_t calculateCRC(const uint8_t *data, uint8_t len) {
  uint8_t crc = 0x00;

  for (uint8_t i = 0; i < len; i++) {
    crc ^= data[i];

    for (uint8_t j = 0; j < 8; j++) {
      if (crc & 0x80) {
        crc = static_cast<uint8_t>((crc << 1) ^ 0x31);
      } else {
        crc = static_cast<uint8_t>(crc << 1);
      }
    }
  }

  return crc;
}

uint8_t calculateChecksum(const uint8_t *data, uint8_t len) {
  uint8_t sum = 0;

  for (uint8_t i = 0; i < len; i++) {
    sum = static_cast<uint8_t>(sum + data[i]);
  }

  return sum;
}

bool buildWn34Packet(uint32_t id, float tempC, uint8_t battery, uint8_t packet[9]) {
  if ((id & 0xFF000000UL) != 0) {
    Serial.printf("Invalid sensor ID 0x%08lX: WN34 uses a 24-bit ID.\n", static_cast<unsigned long>(id));
    return false;
  }

  if (!isfinite(tempC) || tempC < MIN_TEMPERATURE_C || tempC > MAX_TEMPERATURE_C) {
    Serial.printf("Invalid temperature %.2f C for sensor 0x%06lX.\n",
                  tempC,
                  static_cast<unsigned long>(id));
    return false;
  }

  const uint16_t tempRaw =
      static_cast<uint16_t>(lroundf((tempC + 40.0f) * 10.0f));

  packet[0] = 0x34;
  packet[1] = static_cast<uint8_t>((id >> 16) & 0xFF);
  packet[2] = static_cast<uint8_t>((id >> 8) & 0xFF);
  packet[3] = static_cast<uint8_t>(id & 0xFF);

  constexpr uint8_t subType = 0; // WN34L
  packet[4] = static_cast<uint8_t>((subType << 4) | ((tempRaw >> 8) & 0x0F));
  packet[5] = static_cast<uint8_t>(tempRaw & 0xFF);
  packet[6] = battery;

  packet[7] = calculateCRC(packet, 7);
  packet[8] = calculateChecksum(packet, 8);

  return true;
}

bool transmitPacket(const uint8_t packet[9]) {
  const int16_t firstState = radio.transmit(packet, 9);
  delay(30);
  const int16_t secondState = radio.transmit(packet, 9);

  if (firstState != RADIOLIB_ERR_NONE) {
    Serial.printf("First RF transmission failed: %d\n", firstState);
  }

  if (secondState != RADIOLIB_ERR_NONE) {
    Serial.printf("Second RF transmission failed: %d\n", secondState);
  }

  return firstState == RADIOLIB_ERR_NONE || secondState == RADIOLIB_ERR_NONE;
}

bool sendOneSensor(uint32_t id, float tempC) {
  uint8_t packet[9];

  if (!buildWn34Packet(id, tempC, BATTERY_LEVEL, packet)) {
    return false;
  }

  return transmitPacket(packet);
}

void advanceSimulatedTemperature(size_t index) {
  temperatures[index] += SIMULATION_STEP_C;

  if (temperatures[index] > SIMULATION_MAX_C) {
    temperatures[index] = SIMULATION_RESET_C;
  }
}

void requireRadioSuccess(const char *step, int16_t state) {
  if (state == RADIOLIB_ERR_NONE) {
    return;
  }

  Serial.printf("%s failed with RadioLib error %d.\n", step, state);

  while (true) {
    delay(1000);
  }
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("Starting Ecowitt WN34 multi-sensor transmitter...");

  requireRadioSuccess(
      "Radio init",
      radio.begin(868.30, 17.24, 40.0, 125.0, 13, 32));

  requireRadioSuccess(
      "Fixed packet length configuration",
      radio.fixedPacketLengthMode(9));

  uint8_t syncWord[] = {0x2D, 0xD4};
  requireRadioSuccess(
      "Sync word configuration",
      radio.setSyncWord(syncWord, 2));

  requireRadioSuccess(
      "Output power configuration",
      radio.setOutputPower(13, true));

  const uint32_t now = millis();

  for (size_t i = 0; i < SENSOR_COUNT; i++) {
    // Spread sensors evenly across 77 seconds while keeping each individual
    // sensor on its own exact 77-second recurrence.
    nextTransmitAt[i] =
        now + static_cast<uint32_t>((SENSOR_PERIOD_MS * i) / SENSOR_COUNT);
  }

  Serial.println("Radio init OK.");
}

void loop() {
  const uint32_t now = millis();

  for (size_t i = 0; i < SENSOR_COUNT; i++) {
    if (static_cast<int32_t>(now - nextTransmitAt[i]) < 0) {
      continue;
    }

    Serial.printf(
        "Sensor %u (ID: %06lX) -> %.1f C\n",
        static_cast<unsigned int>(i + 1),
        static_cast<unsigned long>(sensorIDs[i]),
        temperatures[i]);

    const bool sent = sendOneSensor(sensorIDs[i], temperatures[i]);

    if (!sent) {
      Serial.printf("Sensor %u packet was not transmitted successfully.\n",
                    static_cast<unsigned int>(i + 1));
    }

    // Preserve the prototype behavior: every simulated sensor changes by
    // 0.1 C after its scheduled transmission and wraps back to 20 C above 30 C.
    advanceSimulatedTemperature(i);

    // Schedule from the previous deadline rather than from 'now' to avoid drift.
    do {
      nextTransmitAt[i] += SENSOR_PERIOD_MS;
    } while (static_cast<int32_t>(now - nextTransmitAt[i]) >= 0);
  }

  delay(10);
}
