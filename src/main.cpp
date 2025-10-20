#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsClient.h>
#include "driver/i2s.h"

// Wi-Fi
const char* WIFI_SSID = "********";
const char* WIFI_PASS = "********";

// WebSocket server
const char* WS_HOST = "********"; // Web server local IP
const uint16_t WS_PORT = 8765;
const char* WS_PATH = "/audio";

// I2S
#define I2S_WS   26
#define I2S_SCK  25
#define I2S_SD   35

// Audio settings
#define SAMPLE_RATE     16000
#define BITS_PER_SAMPLE I2S_BITS_PER_SAMPLE_16BIT
#define CHANNEL_FMT     I2S_CHANNEL_FMT_ONLY_LEFT


static const size_t CHUNK_BYTES = 3200;
static const TickType_t READ_SLICE_MS = 20;         
static const size_t SLICE_BYTES = 640;              

uint8_t chunkBuf[CHUNK_BYTES];
WebSocketsClient webSocket;

// optional: print RMS every ~1 s
#define DEBUG_RMS 0
static uint32_t chunkCounter = 0;

static inline uint16_t compute_rms_16le(const uint8_t* b, size_t nbytes) {
  const int16_t* s = reinterpret_cast<const int16_t*>(b);
  size_t n = nbytes / 2;
  uint64_t acc = 0;
  for (size_t i = 0; i < n; ++i) {
    int32_t v = s[i];
    acc += (uint64_t)(v * v);
  }
  if (!n) return 0;
  double mean = (double)acc / (double)n;
  return (uint16_t)sqrt(mean);
}

void i2s_setup() {
  i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = BITS_PER_SAMPLE,
    .channel_format = CHANNEL_FMT,
#if defined(I2S_COMM_FORMAT_STAND_I2S)
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
#else
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
#endif
    .intr_alloc_flags = 0,
    .dma_buf_count = 4,
    .dma_buf_len = 256,
    .use_apll = false,
#if (ESP_IDF_VERSION_MAJOR >= 4)
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
#endif
  };

  i2s_pin_config_t pins = {
    .bck_io_num = I2S_SCK,
    .ws_io_num  = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num  = I2S_SD
  };

  i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pins);
  i2s_zero_dma_buffer(I2S_NUM_0);
  i2s_set_clk(I2S_NUM_0, SAMPLE_RATE, BITS_PER_SAMPLE, I2S_CHANNEL_MONO);
}

void onWsEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      Serial.println("[WS] Connected");
      { uint8_t hello[4] = {0,1,2,3}; webSocket.sendBIN(hello, sizeof(hello)); }
      break;
    case WStype_DISCONNECTED:
      Serial.println("[WS] Disconnected");
      break;
    case WStype_TEXT:
      Serial.print("[Transcript] ");
      Serial.write(payload, length);
      Serial.println();
      break;
    default:
      break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  i2s_setup();
  Serial.println("I2S setup complete.");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Wi-Fi OK: "); Serial.println(WiFi.localIP());

  webSocket.begin(WS_HOST, WS_PORT, WS_PATH);
  webSocket.onEvent(onWsEvent);
  webSocket.setReconnectInterval(2000);
  webSocket.enableHeartbeat(15000, 3000, 2);
}

void loop() {
  webSocket.loop();
  size_t filled = 0;
  while (filled < CHUNK_BYTES) {
    size_t toRead = min(CHUNK_BYTES - filled, SLICE_BYTES);
    size_t bytes_read = 0;
    esp_err_t err = i2s_read(I2S_NUM_0,
                             chunkBuf + filled,
                             toRead,
                             &bytes_read,
                             pdMS_TO_TICKS(READ_SLICE_MS));

    if (err == ESP_OK && bytes_read > 0) {
      filled += bytes_read;
    }
    webSocket.loop();
    delay(1);
  }


#if DEBUG_RMS
  if ((chunkCounter++ % 10) == 0) { // ~ every 1 s
    uint16_t rms = compute_rms_16le(chunkBuf, CHUNK_BYTES);
    Serial.print("chunk="); Serial.print(CHUNK_BYTES);
    Serial.print(" bytes  rms="); Serial.println(rms);
  }
#endif

  if (webSocket.isConnected()) {
    webSocket.sendBIN(chunkBuf, CHUNK_BYTES);
  }
}
