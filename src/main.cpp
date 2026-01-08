#include <RadioLib.h>
#include <SPI.h>

// Piny
#define PIN_CS    5
#define PIN_DIO0  4
#define PIN_RST   16

RF69 radio = new Module(PIN_CS, PIN_DIO0, PIN_RST);

// --- KONFIGURACE 6 SENZORŮ ---
// 6 unikátních ID. Brána si je zapamatuje jako Channel 1 až 6.
// Můžete si je změnit, ale musí být pro každý kanál jiné.
uint32_t sensorIDs[6] = {
  0x00A001, // Senzor 1
  0x00A002, // Senzor 2
  0x00A003, // Senzor 3
  0x00A004, // Senzor 4
  0x00A005, // Senzor 5
  0x00A006  // Senzor 6
};

// Pole pro uložení naměřených teplot (zatím simulované)
float temperatures[6] = {20, 21, 22, 23, 24, 25};

void setup() {
  Serial.begin(115200);
  delay(2000); 
  Serial.println("Startuji multisenzor WN34 (6 kanalu)...");

  // Použijte frekvenci, která se vám osvědčila (např. 868.30 nebo 868.32)
  int state = radio.begin(868.30, 17.24, 40.0, 125.0, 13, 32);

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("Radio Init OK.");
  } else {
    while (true);
  }

  radio.fixedPacketLengthMode(9);
  uint8_t syncWord[] = {0x2D, 0xD4};
  radio.setSyncWord(syncWord, 2);
  radio.setOutputPower(13, true);
}

// Pomocné funkce pro výpočty
uint8_t calculateCRC(uint8_t *data, uint8_t len) {
  uint8_t crc = 0x00;
  for (int i = 0; i < len; i++) {
    crc ^= data[i];
    for (int j = 0; j < 8; j++) {
      if (crc & 0x80) { crc = (crc << 1) ^ 0x31; } 
      else { crc = (crc << 1); }
    }
  }
  return crc;
}

uint8_t calculateChecksum(uint8_t *data, uint8_t len) {
  uint8_t sum = 0;
  for (int i = 0; i < len; i++) {
    sum += data[i];
  }
  return sum;
}

// Funkce pro odeslání jednoho konkrétního senzoru
void sendOneSensor(uint32_t id, float tempC) {
  uint16_t tempRaw = (uint16_t)((tempC + 40.0) * 10.0);
  uint8_t battery = 0x48; // Baterie OK

  uint8_t packet[9];
  packet[0] = 0x34; // Family Code
  packet[1] = (id >> 16) & 0xFF;
  packet[2] = (id >> 8) & 0xFF;
  packet[3] = (id) & 0xFF;
  
  uint8_t subType = 0; // WN34L
  packet[4] = (subType << 4) | ((tempRaw >> 8) & 0x0F);
  packet[5] = tempRaw & 0xFF;
  packet[6] = battery;
  
  packet[7] = calculateCRC(packet, 7);
  packet[8] = calculateChecksum(packet, 8);

  // Odeslání (2x burst pro spolehlivost)
  radio.transmit(packet, 9);
  delay(30);
  radio.transmit(packet, 9);
}

void loop() {
  Serial.println("--- ZACATEK CYKLU MERENI (6 cidel) ---");

  // Zde by v budoucnu bylo čtení reálných čidel (např. DS18B20)
  // temperatures[0] = sensors.getTempCByIndex(0);
  // temperatures[1] = sensors.getTempCByIndex(1);
  // ...

  // Smyčka přes všech 6 senzorů
  for (int i = 0; i < 6; i++) {
    Serial.print("Posilam Senzor ");
    Serial.print(i + 1);
    Serial.print(" (ID: ");
    Serial.print(sensorIDs[i], HEX);
    Serial.print(") -> Teplota: ");
    Serial.println(temperatures[i]);

    sendOneSensor(sensorIDs[i], temperatures[i]);

    // DŮLEŽITÉ: Pauza mezi jednotlivými senzory (2 sekundy).
    // Kdybychom to poslali všechno naráz v jedné milisekundě,
    // brána by to nestihla zpracovat nebo by se pakety srazily.
    delay(2000); 
  }

  Serial.println("Vse odeslano. Dlouha pauza.");
  
  // Simulace změny teploty pro další kolo (abyste viděl, že to žije)
  for(int i=0; i<6; i++) {
    temperatures[i] += 0.1;
    if(temperatures[i] > 30.0) temperatures[i] = 20.0;
  }

  // Interval měření (např. 60 sekund)
  // Odečteme čas strávený vysíláním (6 * 2s = 12s), takže spíme třeba 48s
  delay(48000);
}