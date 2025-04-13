
/**
 * 本案例演示: 连接自定义服务和固定 wifi
 * 注意：配置值要用双引号！
 * 服务端使用：https://github.com/wangzongming/esp-ai/tree/master/example
 **/

#include <esp-ai.h>
ESP_AI esp_ai;


void on_command(String command_id, String data) {
}
void onSessionStatus(String status) {
  // // 处理会话状态
  // if (status == "iat_start") {
  //   face->DoBlink();
  //   face->Behavior.Clear();
  //   face->Behavior.SetEmotion(eEmotions::Squint, 1.0);
  //   face->Update();
  // }
  // if (status == "iat_end") {
  //   face->DoBlink();
  //   face->Behavior.Clear();
  //   face->Behavior.SetEmotion(eEmotions::Happy, 1.0);
  //   face->Behavior.SetEmotion(eEmotions::Surprised, 1.0);
  //   face->Update();
  // }
  // if (status == "tts_real_end") {
  //   face->DoBlink();
  //   face->Behavior.Clear();
  //   face->Behavior.SetEmotion(eEmotions::Normal, 1.0);
  //   face->Update();
  // }
}
void onPosition( String ip, String nation, String province, String city, String latitude, String longitude) {
    // 处理位置信息
    Serial.println("============ 定位成功 ============");
    Serial.println(ip);
    Serial.println(nation);
    Serial.println(province);
    Serial.println(city);
    Serial.println(latitude);
    Serial.println(longitude);
}
void setup() {
  Serial.begin(115200);

  Serial.print("===> 0可用内存: ");
  Serial.print(ESP.getFreeHeap() / 1024);
  Serial.println("KB");

  // [必  填] 是否调试模式， 会输出更多信息
  bool debug = true;
  // [必  填] wifi 配置： { wifi 账号， wifi 密码, "热点名字" } 可不设置，连不上wifi时会打开热点：ESP-AI，连接wifi后打开地址： 192.168.4.1 进行配网(控制台会输出地址，或者在ap回调中也能拿到信息)
  // ESP_AI_wifi_config wifi_config = { "", "", "ESP-AI" };
  // ESP_AI_wifi_config wifi_config = {"test", "12345678", "ESP-AI"};
  ESP_AI_wifi_config wifi_config = { "oldwang", "oldwang520", "ESP-AI" };
  // [可 填] 服务配置： { 服务协议, 服务IP， 服务端口, "[可选] 请求参数" }
  // ESP_AI_server_config server_config = {  };
  ESP_AI_server_config server_config = { "http", "192.168.3.23", 8088 };
  // [必  填] 唤醒方案： { 方案, 语音唤醒用的阈值(本方案忽略即可), 引脚唤醒方案(本方案忽略), 发送的字符串 }
  ESP_AI_wake_up_config wake_up_config = { "asrpro", 1, 10, "start" };
  // ESP_AI_wake_up_config wake_up_config = { "pin_high", 1, 10 };  // 如果按钮按下是低电平，那使用 pin_low 即可
  // ESP_AI_wake_up_config wake_up_config = { "pin_high_listen", 1, 10 };  // 按下对话
  // ESP_AI_wake_up_config wake_up_config = { "edge_impulse", 0.95, 10, "start",  5000, 1000  }; // 内置语音唤醒
  // ESP_AI_wake_up_config wake_up_config = { "diy" }; // 内置语音唤醒
  // ESP_AI_wake_up_config wake_up_config = {}; // 内置语音唤醒
  // wake_up_config.pin = 0;
  // strcpy(wake_up_config.wake_up_scheme, "pin_low");  // 唤醒方案


  /**
 * 语音唤醒方案：
 * edge_impulse：内置语音唤醒方案 (esp32S3板子支持)
 *       asrpro：天问语音模块唤醒
 *     pin_high：引脚高电平唤醒
 *      pin_low：引脚低电平唤醒
 *     pin_high_listen：引脚高电平聆听(按下对话)
 *      pin_low_listen：引脚低电平聆听(按下对话)
 *       serial：串口字符唤醒
 *       custom：自定义，自行调用 esp_ai.wakeUp() 唤醒
 */
  // ESP_AI_wake_up_config wake_up_config = {
  //   "pin_high",  // 唤醒方案
  //   0.95,        // 使用内置语音唤醒时的唤醒阈值 0-1
  //   10,          // 引脚唤醒方案时需要配置的引脚
  //   "start",     // 天问/串口唤醒方案时需要配置的对方发来的字符串
  //   1000         // vad 时间，默认 1500， 安静环境下非常有用。
  // };


  ESP_AI_volume_config volume_config = {
    .input_pin = 7,   // 输入引脚， 默认 7
    .max_val = 4096,  // 最大输出值
    .volume = 0.9,    // 默认音量
    .enable = false   // 启用电位器
  };


  // XIAO ESP32s3 麦克风配置
  ESP_AI_i2s_config_mic i2s_config_mic = { 5, 4, 6 };

  // XIAO ESP32s3 扬声器配置
  ESP_AI_i2s_config_speaker i2s_config_speaker = {
    .bck_io_num =15,  // BCK引脚
    .ws_io_num = 16,   // WS引脚
    .data_in_num = 7  // DATA输入引脚
  }; 

  esp_ai.onPosition(onPosition);
  // // 第三代配置
  // esp_ai.begin({ debug, wifi_config, server_config, wake_up_config, volume_config, i2s_config_mic, i2s_config_speaker });

  // esp32s3 配置
  esp_ai.begin({ debug, wifi_config, server_config, wake_up_config, volume_config });
  // esp_ai.onEvent(on_command); 

  // boot 按钮有问题，必须在 begen 后在从新设置一遍
  // pinMode(0, INPUT_PULLUP);
}

// int btnClick = 0;           // 按钮是否按下
// long lastDebounceTime = 0;  // 最后一次按下按钮的时间
// long debounceDelay = 100;   // 按下多少 ms 才算，防止传感器自身的误差
// int prev_state = 0;         // 输入引脚的先前读数

// 未运行 .begin 和 .loop 时剩余内存：346kb
// 未运行 .begin          时剩余内存：331kb
//    .begin 和 .loop 都运行剩余内存：184kb ---->

// long last_log_time = 0;

void loop() {
  // if (millis() - last_log_time > 3000) {
  //   last_log_time = millis();
  //   Serial.print("===> 可用内存: ");
  //   Serial.print(ESP.getFreeHeap() / 1024);
  //   Serial.println("KB");
  // }
  // Serial.print("   可用内存: ");
  // Serial.print(ESP.getFreeHeap() / 1024);
  // Serial.println("KB");
  // delay(100);

  esp_ai.loop();
}


// #include <Arduino.h>
// #include "driver/i2s.h"

// #define I2S_SAMPLE_RATE 16000                  // 采样率 16kHz
// #define I2S_BITS I2S_BITS_PER_SAMPLE_16BIT     // 16-bit 采样
// #define I2S_CHANNEL I2S_CHANNEL_FMT_ONLY_LEFT  // 单声道
// #define I2S_DMA_BUF_LEN 1024                   // DMA 缓冲区大小
// #define I2S_PORT I2S_NUM_0                     // I2S 端口 0
// // #define I2S_PORT -1  // I2S 端口 0

// // I2S 引脚定义
// #define I2S_BCLK -1  // BCLK 时钟引脚
// #define I2S_WS 42    // WS / LRCLK 引脚
// #define I2S_DATA 41  // 数据输入引脚

// void i2s_setup() {
//   // I2S 配置
//   i2s_config_t i2s_config = {
//     // 会不断重启...
//     .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM),
//     .sample_rate = 16000U,
//     .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
//     //.channel_format       = I2S_CHANNEL_FMT_ONLY_RIGHT,     // Also works
//     .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
//     //.communication_format = I2S_COMM_FORMAT_PCM,            // Also works
//     .communication_format = I2S_COMM_FORMAT_I2S,
//     .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
//     .dma_buf_count = 4,
//     .dma_buf_len = 1024,
//     .use_apll = false,
//     .tx_desc_auto_clear = false,
//     .fixed_mclk = 0
//   };

//   // I2S 引脚配置
//   i2s_pin_config_t pin_config = {
//     .bck_io_num = I2S_PIN_NO_CHANGE,
//     .ws_io_num = 42,
//     .data_out_num = I2S_PIN_NO_CHANGE,
//     .data_in_num = 41
//   };

//   // 安装 I2S 驱动
//   esp_err_t ret = 0;
//   ret = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
//   i2s_set_pin(I2S_PORT, &pin_config);
//   // i2s_set_clk(I2S_PORT, I2S_SAMPLE_RATE, I2S_BITS, I2S_CHANNEL);
//   if (ret != ESP_OK) {
//     Serial.println("[Error] Error in i2s_driver_install");
//   }

//   if (ret != ESP_OK) {
//     Serial.println("[Error] Error in i2s_set_pin");
//   }


//   ret = i2s_zero_dma_buffer(I2S_PORT);

//   if (ret != ESP_OK) {
//     Serial.println("[Error] Error in initializing dma buffer with 0");
//   }

//   Serial.println("I2S Initialized.");
// }

// void setup() {
//   Serial.begin(115200);
//   i2s_setup();
// }

// void loop() {
//   int16_t i2s_buffer[I2S_DMA_BUF_LEN];  // 16-bit PCM 数据缓冲区
//   size_t bytes_read;

//   // 读取 I2S 数据
//   i2s_read(I2S_PORT, i2s_buffer, sizeof(i2s_buffer), &bytes_read, portMAX_DELAY);

//   // 打印部分数据
//   Serial.print("I2S Data: ");
//   for (int i = 0; i < 30; i++) {
//     Serial.print(i2s_buffer[i]);
//     Serial.print(" ");
//   }
//   Serial.println();

//   delay(100);  // 每 100ms 读取一次
// }
