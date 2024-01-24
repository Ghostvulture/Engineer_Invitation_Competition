#include <esp_now.h>
#include <WiFi.h>
#include <ESP32Servo.h>
#include <queue>

typedef struct struct_message {
  char cmd[8];
  int arc;
} struct_message;

std::queue<struct_message> messageQueue;
struct_message myData;
Servo myServo;

// 定义一个16字节的加密密钥，要和发射端一致
const uint8_t encryptionKey[16] = {0x01, 0x04, 0x03,0x09, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x02};

void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  struct_message receivedData;
  memcpy(&receivedData, incomingData, sizeof(receivedData));
  messageQueue.push(receivedData);
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  ESP32PWM::allocateTimer(0);
  myServo.setPeriodHertz(20);
  myServo.attach(13, 500, 1500);
  esp_now_register_recv_cb(OnDataRecv);

  // 设置对等体信息
  esp_now_peer_info_t peerInfo;
  memset(&peerInfo, 0, sizeof(esp_now_peer_info_t));
  // 对方设备的MAC地址
  uint8_t remoteMac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};// 需要替换为对方设备的MAC地址
  memcpy(peerInfo.peer_addr, remoteMac, 6); 
  peerInfo.channel = 0; 
  peerInfo.encrypt = true; // 启用加密
  memcpy(peerInfo.lmk, encryptionKey, 16); // 设置加密密钥

  // 添加对等体
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add encrypted peer");
  }
}

void loop() {
  if (!messageQueue.empty()) {
    struct_message data = messageQueue.front();
    messageQueue.pop();
    if (data.arc >= 30 && data.arc <= 300) {
      myServo.write(data.arc);
    }
  }
}
