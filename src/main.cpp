#include <Arduino.h>

constexpr uint32_t kRtlRxPin = PB1;

constexpr uint32_t kDelay = 968;
constexpr uint32_t kShortDelay = kDelay / 4;
constexpr uint32_t kLongDelay = kDelay / 2;
constexpr uint8_t polarity = 1;
constexpr size_t kMaxBytes = 6;
constexpr size_t kHeaderBits = 10;

constexpr size_t kDeviceAndChannels = 256 * 8;

constexpr float kInvalidTemperature = std::numeric_limits<float>::max();
constexpr float kMaxTemperatureDeltaCelsius = 4.0f;
std::array<float, kDeviceAndChannels> temperatures;

constexpr uint8_t kInvalidHumidity = std::numeric_limits<uint8_t>::max();
constexpr uint8_t kMaxHumidityDelta = 4;
std::array<uint8_t, kDeviceAndChannels> humidities;

void HandlePacket(const uint8_t b[6]) {
  // https://github.com/merbanan/rtl_433/blob/master/src/devices/ambient_weather.c
  uint8_t device_id = b[1];
  bool battery_low = (b[2] & 0x80) != 0;
  uint8_t channel = ((b[2] & 0x70) >> 4) + 1;
  int temperature_raw = ((b[2] & 0x0f) << 8) | b[3];
  float temperature = (temperature_raw - 400) * 0.1f;
  float celsius = (static_cast<double>(temperature) - 32.0f) * 5.0f / 9.0f;
  uint8_t humidity = b[4];

  if (device_id == 0 || channel == 0 || channel > 8) {
    Serial.println(R"({"error": "invalid device ID or channel"})");
    return;
  }
  const uint16_t key = device_id * channel;
  if (celsius <= -30 || celsius >= 50) {
    Serial.println(R"({"error": "temperature is out-of-range"})");
    return;
  }
  if (humidity >= 100) {
    Serial.println(R"({"error": "humidity is out-of-range"})");
    return;
  }
  if (temperatures[key] != kInvalidTemperature &&
      std::abs(temperatures[key] - celsius) > kMaxTemperatureDeltaCelsius) {
    Serial.println(R"({"error": "temperature jumped too much"})");
    return;
  }
  if (humidities[key] != kInvalidHumidity &&
      std::abs(humidities[key] - humidity) > kMaxHumidityDelta) {
    Serial.println(R"({"error": "humidity jumped too much"})");
    return;
  }

  temperatures[key] = celsius;
  humidities[key] = humidity;

  // clang-format off
  // {"device": 17, "channel": 3, "low_battery": true, "temperature_c": 21.94, "humidity": 44}
  // clang-format on
  Serial.print(R"({"device": )");
  Serial.print(device_id);
  Serial.print(R"(, "channel": )");
  Serial.print(channel);
  Serial.print(R"(, "low_battery": )");
  Serial.print(battery_low ? "true" : "false");
  Serial.print(R"(, "temperature_c": )");
  Serial.print(celsius);
  Serial.print(R"(, "humidity": )");
  Serial.print(humidity);
  Serial.println(R"(})");
}

uint8_t buff[7];
uint8_t data_byte = 0;
uint8_t n_bits = 6;
size_t n_bytes = 0;

void AddBit(uint8_t bit_state) {
  data_byte = (data_byte << 1) | bit_state;
  n_bits++;
  if (n_bits == 8) {
    n_bits = 0;
    buff[n_bytes] = data_byte;
    n_bytes++;
  }
  if (n_bytes == kMaxBytes) {
    HandlePacket(buff + 1);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(kRtlRxPin, INPUT);
  std::fill(temperatures.begin(), temperatures.end(), kInvalidTemperature);
  std::fill(humidities.begin(), humidities.end(), kInvalidHumidity);
}

void loop() {
  // Manchester decoding.
  // Ambient Weather F007TH, TFA 30.3208.02, SwitchDocLabs F016TH temperature sensor
  // OOK pulse manchester, zero bit

  n_bits = 6;
  n_bytes = 0;

  uint8_t temp_bit = polarity;
  bool has_error = false;
  bool first_zero = false;
  size_t n_header_hits = 0;

  while (!has_error && (n_bytes < kMaxBytes)) {
    // Search for transition.
    while (digitalRead(kRtlRxPin) != temp_bit);

    delayMicroseconds(kShortDelay);
    if (digitalRead(kRtlRxPin) != temp_bit) {
      has_error = true;
      continue;
    }

    delayMicroseconds(kLongDelay);
    if (digitalRead(kRtlRxPin) == temp_bit) {
      temp_bit = temp_bit ^ 1;
    }

    const uint8_t bit_state = temp_bit ^ polarity;
    if (bit_state == 1) {
      if (!first_zero) {
        n_header_hits++;
      } else {
        AddBit(bit_state);
      }
    } else {
      if (n_header_hits < kHeaderBits) {
        has_error = true;
      } else {
        if (!first_zero) {
          first_zero = true;
          AddBit(bit_state);
        } else {
          AddBit(bit_state);
        }
      }
    }
  }
}
