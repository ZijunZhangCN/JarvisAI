#include <WiFi.h>
#include <driver/i2s.h>
#include <esp_heap_caps.h>
#include <HTTPClient.h>
#include <WebSocketClient.h>
#include <ArduinoJson.h>
#include <base64.h>
#include <queue>

// WiFi 配置
const char* ssid = "YourWiFi"; // 替换成WiFi
const char* password = "YourWiFiPassword"; // 替换成Wifi密码

// 本地服务器配置
const String serverHost = "192.168.*.*";  // 替换成本地服务器地址
const int serverPort = 8888;              // 替换成本地服务器端口
WiFiClient wifiClient;
WebSocketClient webSocketClient = WebSocketClient(wifiClient, serverHost.c_str(), serverPort);

// I2S配置
#define I2S_BCLK_INMP441 20  
#define I2S_LRC_INMP441 21  
#define I2S_DOUT_INMP441 19
#define I2S_BCLK_MAX98357 17
#define I2S_LRC_MAX98357 18
#define I2S_DOUT_MAX98357 16

#define SAMPLE_RATE 16000  // 16kHz采样率
#define SAMPLE_BITS 16     // 16位采样
#define BUFFER_SIZE 2048   // I2S缓冲区

// I2S配置
i2s_config_t i2s_config_inmp441 = {
  .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
  .sample_rate = SAMPLE_RATE,
  .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
  .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
  .communication_format = I2S_COMM_FORMAT_STAND_I2S,
  .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
  .dma_buf_count = 8,
  .dma_buf_len = 1024
};

i2s_pin_config_t pin_config_inmp441 = {
  .bck_io_num = I2S_BCLK_INMP441,
  .ws_io_num = I2S_LRC_INMP441,
  .data_out_num = I2S_PIN_NO_CHANGE,
  .data_in_num = I2S_DOUT_INMP441,
};

i2s_config_t i2s_config_max98357 = {
  .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
  .sample_rate = SAMPLE_RATE,
  .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
  .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
  .communication_format = I2S_COMM_FORMAT_STAND_I2S,
  .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
  .dma_buf_count = 16,
  .dma_buf_len = 64
};

i2s_pin_config_t pin_config_max98357 = {
  .bck_io_num = I2S_BCLK_MAX98357,
  .ws_io_num = I2S_LRC_MAX98357,
  .data_out_num = I2S_DOUT_MAX98357,
  .data_in_num = I2S_PIN_NO_CHANGE
};

// 录音控制
volatile bool recording = false;
uint8_t* recordBuffer = NULL;  // 将存储在PSRAM
size_t recordLength = 0;
unsigned long lastSoundTime = 0;
const int silenceDuration = 2000;

// Queue
struct AudioMsg {
  size_t bytesRead;
  uint8_t buffer[BUFFER_SIZE] __attribute__((aligned(4)));
  bool isMusic;
};
QueueHandle_t audioQueue = xQueueCreate(1024, sizeof(AudioMsg*));
QueueHandle_t ttsQueue = xQueueCreate(1024, sizeof(char*));

// Mutex
SemaphoreHandle_t i2sMutex = xSemaphoreCreateMutex();
SemaphoreHandle_t httpMutex = xSemaphoreCreateMutex();
HTTPClient* currentHttp = NULL;

// WAV文件头解析结构体
struct WavHeader {
  uint32_t chunkID;
  uint32_t chunkSize;
  uint32_t format;
  uint32_t subchunk1ID;
  uint32_t subchunk1Size;
  uint16_t audioFormat;
  uint16_t numChannels;
  uint32_t sampleRate;
  uint32_t byteRate;
  uint16_t blockAlign;
  uint16_t bitsPerSample;
};

void setup() {
  Serial.begin(115200);
  // 初始化PSRAM
  if (psramInit()) {
    Serial.println("PSRAM Available!");
    // 分配30秒缓冲（约960KB）到PSRAM
    recordBuffer = (uint8_t*)heap_caps_malloc(30 * SAMPLE_RATE * sizeof(int16_t), MALLOC_CAP_SPIRAM);
    if (!recordBuffer) {
      Serial.println("PSRAM Allocation Failed!");
      while (1) delay(100);
    }
  } else {
    Serial.println("No PSRAM Detected!");
    while (1) delay(100);
  }
  scan_networks();
  connect_wifi();
  setup_i2s();
  setup_task();
  Serial2.begin(115200, SERIAL_8N1, 1);  // 波特率 115200，8 数据位，无校验，1 停止位
}

void scan_networks() {
  int n = WiFi.scanNetworks();
  Serial.printf("发现 %d 个网络:\n", n);

  for (int i = 0; i < n; ++i) {
    Serial.printf("%02d: %s (%ddBm) Ch%d %s\n",
                  i + 1,
                  WiFi.SSID(i).c_str(),
                  WiFi.RSSI(i),
                  WiFi.channel(i),
                  WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "开放" : "加密");
  }
}

void connect_wifi() {
  WiFi.disconnect(true);
  delay(1000);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);

  Serial.printf("尝试连接至 %s ...\n", "SSID");
  WiFi.begin(ssid, password);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");

    // 30秒超时机制
    if (millis() - start > 30000) {
      Serial.println("\n超时! 失败原因:");
      print_wifi_fail_reason();
      break;
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n已连接! IP地址: %s\n", WiFi.localIP().toString().c_str());
  }
}

void print_wifi_fail_reason() {
  switch (WiFi.status()) {
    case WL_IDLE_STATUS: Serial.println("WiFi模块未初始化"); break;
    case WL_NO_SSID_AVAIL: Serial.println("SSID不可用"); break;
    case WL_SCAN_COMPLETED: Serial.println("扫描完成"); break;
    case WL_CONNECT_FAILED: Serial.println("密码错误"); break;
    case WL_CONNECTION_LOST: Serial.println("连接丢失"); break;
    case WL_DISCONNECTED: Serial.println("未连接"); break;
    default: Serial.printf("未知错误代码: %d\n", WiFi.status());
  }
}

void setup_i2s() {
  i2s_driver_install(I2S_NUM_0, &i2s_config_inmp441, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config_inmp441);

  i2s_driver_install(I2S_NUM_1, &i2s_config_max98357, 0, NULL);
  i2s_set_pin(I2S_NUM_1, &pin_config_max98357);
}

void setup_task() {
  xTaskCreatePinnedToCore(
    ttsTaskHandler,  // 任务函数
    "TTSTask",       // 任务名称
    8192,            // 堆栈大小
    NULL,            // 参数
    1,               // 优先级
    NULL,            // 任务句柄
    1                // 运行在核心1
  );

  xTaskCreatePinnedToCore(
    audioTaskHandler,  // 任务函数
    "AudioTask",       // 任务名称
    8192,              // 堆栈大小
    NULL,              // 参数
    1,                 // 优先级
    NULL,              // 任务句柄
    0                  // 运行在核心0
  );
}

void loop() {
  // 检查是否有数据从 ASR PRO 传来
  if (Serial2.available()) {
    String receivedData = Serial2.readStringUntil('\n');
    receivedData.trim();
    Serial.print("接收到数据: ");
    Serial.println(receivedData);
    if (receivedData == "start") {
      startRecording();
    }
  }

  if (recording) {
    int16_t i2sBuffer[BUFFER_SIZE];
    size_t bytesRead = 0;
    i2s_read(I2S_NUM_0, i2sBuffer, sizeof(i2sBuffer), &bytesRead, portMAX_DELAY);
    Serial.println("recording...");

    // 检测静音
    if (!isSilence(i2sBuffer, bytesRead / 2)) {
      lastSoundTime = millis();
    } else if (millis() - lastSoundTime > silenceDuration) {
      Serial.printf("stop record, recordLength: %d\n", recordLength);
      recording = false;
      requestTencentASR(recordBuffer, recordLength);
      recordLength = 0;
      return;
    }

    // 存储数据到PSRAM
    if (recordLength + bytesRead < 30 * SAMPLE_RATE * sizeof(int16_t)) {
      memcpy(recordBuffer + recordLength, i2sBuffer, bytesRead);
      recordLength += bytesRead;
    } else {
      Serial.println("Buffer Full!");
      recording = false;
    }
  }

  if (webSocketClient.connected()) {
    if (webSocketClient.parseMessage()) {
      String message;
      while (webSocketClient.available()) {
        char c = webSocketClient.read();
        message += c;
      }
      if (message != " " && message != "") {
        Serial.print("send tts queue msg: ");
        Serial.println(message);
        char* msg = strdup(message.c_str());
        if (xQueueSend(ttsQueue, &msg, portMAX_DELAY) != pdPASS) {
          free(msg);
          Serial.println("tts queue msg send failed");
        }
      }
    }
  }
}

void startRecording() {
  xQueueReset(audioQueue);
  xQueueReset(ttsQueue);
  switch_to_voice_mode();

  heap_caps_free(recordBuffer);
  recordBuffer = (uint8_t*)heap_caps_malloc(30 * SAMPLE_RATE * sizeof(int16_t), MALLOC_CAP_SPIRAM);
  if (!recordBuffer) {
    Serial.println("PSRAM Realloc Failed!");
    return;  // 处理分配失败
  }
  recording = true;
  recordLength = 0;
  lastSoundTime = millis();
}

// 静音检测（基于能量阈值）
bool isSilence(int16_t* samples, size_t count) {
  const int silenceThreshold = 100;  // 根据环境调整
  long sum = 0;

  for (int i = 0; i < count; i++) {
    sum += abs(samples[i]);
  }
  int avg = sum / count;
  return avg < silenceThreshold;
}

void requestTencentASR(uint8_t* data, size_t length) {
  Serial.println("start asr");
  HTTPClient http;

  // 终止当前HTTP连接
  xSemaphoreTake(httpMutex, portMAX_DELAY);
  if (currentHttp != NULL) {
    Serial.println("asr: stop pre http connect");
    currentHttp->end();  // 强制终止连接
  }
  currentHttp = &http;
  xSemaphoreGive(httpMutex);

  String url = "http://"+serverHost+":"+serverPort+"/asr";
  http.begin(url);
  http.setConnectTimeout(3000);  // 连接超时3秒
  http.setTimeout(5000);         // 总超时5秒

  int httpCode = http.POST(data, length);

  if (httpCode == HTTP_CODE_OK) {
    String response = http.getString();
    Serial.println("识别结果: " + response);
    StaticJsonDocument<256> doc;  // 根据响应大小调整缓冲区大小
    // 解析 JSON 响应
    DeserializationError error = deserializeJson(doc, response);
    if (error) {
      Serial.println("JSON 解析失败: " + String(error.c_str()));
    } else {
      // 提取字段
      String text = doc["text"];
      SocketChat(text);
    }
  } else {
    Serial.println("请求失败: " + http.errorToString(httpCode));
  }

  http.end();
  Serial.println("asr http stop");
  xSemaphoreTake(httpMutex, portMAX_DELAY);
  currentHttp = NULL;
  xSemaphoreGive(httpMutex);
}

void SocketChat(const String& recognitionResult) {
  String text = urlEncode(recognitionResult);
  String path = "/ws_chat?input=" + text;

  Serial.println("path: " + path);
  // 连接 WebSocket 服务器
  if (wifiClient.connect(serverHost.c_str(), serverPort)) {
    Serial.println("Connected to WebSocket server");
    // // 发送 WebSocket 握手请求
    if (webSocketClient.begin(path)) {
      Serial.println("WebSocket handshake successful");
    }
  } else {
    Serial.println("Failed to connect to WebSocket server");
  }
}

void ttsTaskHandler(void* pvParameters) {
  while (1) {
    char* msg;
    if (xQueueReceive(ttsQueue, &msg, portMAX_DELAY) == pdTRUE) {
      Serial.print("tts msg: ");
      Serial.println(*msg);
      requestTencentTTS(msg);
      free(msg);
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);  // 避免高频轮询
  }
}

void audioTaskHandler(void* pvParameters) {
  while (1) {
    AudioMsg* msg;
    if (xQueueReceive(audioQueue, &msg, portMAX_DELAY) == pdTRUE) {
      if (msg == NULL || recording) {
        if (msg != NULL) free(msg); 
        Serial.println("msg == NULL || recording");
        continue;
      }

      xSemaphoreTake(i2sMutex, portMAX_DELAY);
      size_t bytesWritten;
      i2s_write(I2S_NUM_1, msg->buffer, msg->bytesRead, &bytesWritten, portMAX_DELAY);
      free(msg);
      msg = NULL;
      xSemaphoreGive(i2sMutex);
    }
  }
}

void requestTencentTTS(String text) {
  Serial.println("start tts");
  bool isMusic = false;
  if (text.startsWith("https")) {
    // 播放远端音乐
    isMusic = true;
    Serial.print("准备播放音乐: ");
    switch_to_music_mode();
  }

  HTTPClient http;
  // 终止当前HTTP连接
  xSemaphoreTake(httpMutex, portMAX_DELAY);
  if (currentHttp != NULL) {
    currentHttp->end();
  }
  currentHttp = &http;
  xSemaphoreGive(httpMutex);

  String url = "http://"+serverHost+":"+serverPort+"/tts?text=" + text;
  http.begin(url);

  const char* headerKeys[] = { "Content-Type", "Content-Length" };
  http.collectHeaders(headerKeys, 2);
  http.setConnectTimeout(3000);        // 连接超时3秒
  http.setTimeout(100000);             // 总超时100秒

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    WiFiClient* stream = http.getStreamPtr();
    uint8_t buffer[BUFFER_SIZE];  // 按固定缓冲区读取
    size_t totalRead = 0;
    int contentLength = http.header("Content-Length").toInt();
    Serial.print("stream contentLength: ");
    Serial.println(contentLength);
    String contentType = http.header("Content-Type");
    if (contentType == "audio/mpeg") {
      while (totalRead < contentLength) {
        if (recording) {
          Serial.println("recording, stop send audio queue");
          break;
        }
        size_t remaining = contentLength - totalRead;
        size_t toRead = min((size_t)BUFFER_SIZE, remaining);
        size_t bytesRead = stream->readBytes(buffer, toRead);
        if (bytesRead > 0) {
          AudioMsg* msg = (AudioMsg*)malloc(sizeof(AudioMsg));
          memcpy(msg->buffer, buffer, bytesRead);
          msg->bytesRead = bytesRead;
          msg->isMusic = isMusic;
          if (xQueueSend(audioQueue, &msg, portMAX_DELAY) != pdPASS) {
            Serial.println("tts queue msg send failed");
            free(msg);
          } else {
            totalRead += bytesRead;
          }
        }
        vTaskDelay(1 / portTICK_PERIOD_MS);  // 让出CPU
      }
    }
  }

  // 清理HTTP连接
  http.end();
  Serial.println("tts http stop");
  // 注销当前HTTPClient
  xSemaphoreTake(httpMutex, portMAX_DELAY);
  currentHttp = NULL;
  xSemaphoreGive(httpMutex);
}

// URL 编码函数
String urlEncode(const String& str) {
  String encoded = "";
  char c;
  for (size_t i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += c;  // 保留字母、数字和特定符号
    } else {
      char buf[4];
      sprintf(buf, "%%%02X", (unsigned char)c);  // 转换为 %XX 格式
      encoded += buf;
    }
  }
  return encoded;
}

// 切换为单声道语音模式
void switch_to_voice_mode() {
  xSemaphoreTake(i2sMutex, portMAX_DELAY);
  i2s_stop(I2S_NUM_1);
  i2s_zero_dma_buffer(I2S_NUM_1);
  delay(100);
  i2s_set_clk(I2S_NUM_1, SAMPLE_RATE, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO);
  i2s_config_max98357.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
  i2s_start(I2S_NUM_1);
  xSemaphoreGive(i2sMutex);
}

// 切换为立体声音
void switch_to_music_mode() {
  xSemaphoreTake(i2sMutex, portMAX_DELAY);
  i2s_stop(I2S_NUM_1);
  i2s_zero_dma_buffer(I2S_NUM_1);
  delay(100);
  i2s_set_clk(I2S_NUM_1, 44100, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_STEREO);
  i2s_config_max98357.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  // i2s_set_dac_mode(I2S_DAC_CHANNEL_DISABLE);  // 禁用DAC，使用外部I2S编解码器
  i2s_start(I2S_NUM_1);
  xSemaphoreGive(i2sMutex);
}

