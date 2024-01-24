#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
Adafruit_ADS1115 ads;  // 默认I2C地址

#define VRSw 32  // 第一个遥感模块的sw按钮    

// 广播MAC地址
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
// 机械手发送数据
typedef struct hand_message {
  int T;
  byte dev;
  float base;
  float shoulder;
  float elbow;
  float hand;
  byte cmd;
  char megs[210];
} hand_message;
hand_message handData;

// 机械爪发送数据
typedef struct finger_message {
  char cmd[8];
  int arc;  //角度
} finger_message;
finger_message fingerData;



esp_now_peer_info_t peerInfo;  //espnow参数

//把摇杆模拟信号改为-8至8的整数
int mapToRange(int x, int in_min, int in_max,int mid) {
  int out_min=-11;  
  int out_max=11;  
  
  //返回-8到8之间的数值，不动返回0
  if(x>mid){ 
    return(  out_max*(x-mid)/(in_max-mid));
  }
  else{
    return( out_min*(mid-x)/(mid-in_min));
  }
}

void setup() {
  pinMode(VRSw,INPUT_PULLUP);
  Serial.begin(115200);
  analogReadResolution(8);  // 设置模拟读取分辨率为8位，读入的数值范围0-255
  // 初始化WiFi
  WiFi.mode(WIFI_STA);
  delay(500);
  // 初始化ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  Serial.println("ESP-NOW ok");
  // 设置ESP-NOW发送完成回调
  esp_now_register_send_cb(OnDataSent);
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }
  //初始化模拟接收模块
  ads.begin();  
}
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  //Serial.println(status == ESPNOW_SEND_SUCCESS ? "Success" : "Fail");
}
float arm[5] = {0, 0, 1.570796, 3.141593, 30};//-----机械手的初始位置
//======下面是机械手最大限制范围
float armLimit[5][2] = {
    {-3.14, 3.14},
    {-1.57, 1.57},
    {-1.11, 3.14},
    {1.08, 3.14},
    {30, 330}
};
float getControl[5];

void send1(float arm0,float arm1,float arm2,float arm3){  //发送给机械臂
  handData.T =305;
  handData.dev = 0; // 设备编号0
  handData.base = arm0;  //底座
  handData.shoulder = arm1;  //大臂
  handData.elbow = arm2;  //小臂
  handData.hand = arm3;  //腕
  handData.cmd = 0;  //led灯是否打开
  strcpy(handData.megs, "move");
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &handData, sizeof(handData));
  if (result == ESP_OK) {
    Serial.print("Sent ok ");
    Serial.print(handData.megs);
    Serial.print(handData.T);
    Serial.print("  ");
    Serial.print(handData.base);
    Serial.print("  ");
    Serial.print(handData.shoulder);
    Serial.print("  ");
    Serial.print(handData.elbow);
    Serial.print("  ");
    Serial.print(handData.hand);
    Serial.println("  ");
  } else {
    Serial.println("Send Error");
  }
}

//发送给机械爪
void send2(float arm4) { 
  strcpy(fingerData.cmd, "catch");
  fingerData.arc = arm4;
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &fingerData, sizeof(fingerData));
  if (result == ESP_OK) {
    Serial.print(fingerData.cmd);
    Serial.print(arm4);
    Serial.print("  ");
    Serial.println("Sent ok");
  } else {
    Serial.println("Send Error");
  }  
}


float arc=4;//delay秒内移动最大角度
float arcRate=arc*3.14/180; //转换成弧度制
void loop() {
  int yValueSw = digitalRead(VRSw);  // 读取第一个遥感模块的按钮情况
  
  if(yValueSw==0)//回原始位置，
  {
    Serial.println("000");
    //send({"T":305,"dev":0,"b":arm[0],"s":arm[1],"e":arm[2],"h":arm[3],"cmd":0,"megs":"move"}); //发送给机械臂
    arm[0] = 0;
    arm[1] = 0;
    arm[2]=1.57;
    arm[3]=3.14;
    arm[4]=30;
    send1(arm[0],arm[1],arm[2],arm[3]); 
    send2(arm[4]); 
  }
  else
  {
    getControl[0] = mapToRange(ads.readADC_SingleEnded(0), 0, 26900,13663);  // 读取第一个遥感模块的值
    getControl[1] = mapToRange(ads.readADC_SingleEnded(1), 0, 26900,13270);  // 读取第二个遥感模块的值
    getControl[2] = mapToRange(ads.readADC_SingleEnded(2), 0, 26900,13383);  // 读取第三个遥感模块的值
    getControl[3] = mapToRange(ads.readADC_SingleEnded(3), 0, 26900,12975);  // 读取第四个遥感模块的值
    /*Serial.print(getControl[0]);
    Serial.print("  ");
    Serial.print(getControl[1]);
    Serial.print("  ");
    Serial.print(getControl[2]);
    Serial.print("  ");
    Serial.print(getControl[3]);
    Serial.println("  ");*/

    // 判断是否所有遥感模块都有输入
    if(getControl[0] != 0 || getControl[1] != 0 || getControl[2] != 0  ) {
        for(int i = 0; i < 3; i++) {
            arm[i] += getControl[i] * arcRate/10; //返回-10 .. 10之间
            arm[i] = constrain(arm[i], armLimit[i][0], armLimit[i][1]);// 限制关节角度在设定范围内
        }
        //getControl[3]=constrain(1.57/2+(1.57-getControl[1]-getControl[2]), armLimit[3][0], armLimit[3][1]); // 腕要水平,腕=90度+(180度-大臂角度-小臂角度)
        send1(arm[0],arm[1],arm[2],arm[3]);

    }
    else if( getControl[3] != 0){
      //arm[4] += getControl[3] * arc/10;
      if (getControl[3] < -5) {  // 假设阈值为-5，向左
      arm[4] = 30;
    } else if (getControl[3] > 5) {  // 假设阈值为5，向右
      arm[4] = 100;
    }//0124
      arm[4] = constrain(arm[4], armLimit[4][0], armLimit[4][1]);// 限制关节角度在设定范围内
      Serial.println(arm[4]);
      send2(arm[4]);
    }
    delay(50); //延时0.2秒，这个时候最多移动2度，然后再检测
  }
}
