#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// 基于ESP32S3 Dev Module的火情监测系统
// 功能：读取火焰传感器数据，接收STM32发送的时间，整合后传输给STM32

// 定义引脚
#define FLAME_SENSOR_AO 15  // 火焰传感器引脚（模拟输入）
#define FLAME_SENSOR_DO 14  // 火焰传感器数字输出引脚
#define TX_PIN 1           // 串口发送引脚（连接到STM32的RX）
#define RX_PIN 3           // 串口接收引脚（连接到STM32的TX）

// 定义串口
HardwareSerial SerialSTM32(1);  // 使用串口1与STM32通信

// 传感器阈值
#define FLAME_THRESHOLD 1000  // 火焰传感器阈值

// WiFi配置
const char* ssid = "vivoS15e";
const char* password = "11111111";

// NTP配置
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "cn.pool.ntp.org", 8 * 3600, 60000);

// 数据结构
struct SensorData {
  int flameValue;     // 火焰传感器模拟值
  bool flameDigital;   // 火焰传感器数字值
  bool fireDetected;   // 是否检测到火焰
};

SensorData sensorData;

// 时间相关变量
String currentTime = "";  // 当前时间（YYYY-MM-DD HH:MM:SS）
bool timeReceived = false; // 是否接收到时间
unsigned long lastTimeSync = 0;  // 上次时间同步时间
const unsigned long TIME_SYNC_INTERVAL = 3600000; // 1小时（毫秒）

// 接收缓冲区
char rxBuffer[64];
int rxIndex = 0;

void setup() {
  // 初始化串口0（用于调试）
  Serial.begin(115200);
  
  // 初始化串口1（用于与STM32通信）
  SerialSTM32.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);
  
  // 初始化传感器引脚
  pinMode(FLAME_SENSOR_AO, INPUT);  // 火焰传感器模拟输入
  pinMode(FLAME_SENSOR_DO, INPUT);   // 火焰传感器数字输入
  
  Serial.println("========================================");
  Serial.println("  ESP32 Fire Monitoring System");
  Serial.println("========================================\r\n");
  
  // 连接WiFi
  Serial.println("[WiFi] Connecting to WiFi...");
  Serial.print("[WiFi] SSID: ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\r\n[WiFi] Connected!");
    Serial.print("[WiFi] IP address: ");
    Serial.println(WiFi.localIP());
    
    // 初始化NTP客户端
    timeClient.begin();
    
    // 首次时间同步
    syncNTPTime();
    
    // 发送时间到STM32
    sendTimeToSTM32();
  } else {
    Serial.println("\r\n[WiFi] Failed to connect!");
    Serial.println("[WiFi] Will use time from STM32");
  }
  
  Serial.println("\r\n[Main] System ready, starting main loop...\r\n");
  
  // 清空接收缓冲区
  memset(rxBuffer, 0, sizeof(rxBuffer));
  rxIndex = 0;
}

void loop() {
  // 定期同步NTP时间（每小时一次）
  if (millis() - lastTimeSync >= TIME_SYNC_INTERVAL) {
    if (WiFi.status() == WL_CONNECTED) {
      syncNTPTime();
      sendTimeToSTM32();
    }
    lastTimeSync = millis();
  }
  
  // 接收STM32发送的时间
  receiveTimeFromSTM32();
  
  // 读取传感器数据
  readSensors();
  
  // 处理数据
  processData();
  
  // 传输数据给STM32
  sendDataToSTM32();
  
  // 等待STM32的确认信号
  waitForSTM32Response();
  
  // 打印调试信息
  printDebugInfo();
  
  // 延时
  delay(1000);
}

// 同步NTP时间
void syncNTPTime() {
  Serial.println("[NTP] Syncing time...");
  
  if (timeClient.update()) {
    unsigned long epochTime = timeClient.getEpochTime();
    
    struct tm *timeinfo;
    timeinfo = localtime(&epochTime);
    
    char timeStr[20];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", timeinfo);
    
    Serial.print("[NTP] Current time: ");
    Serial.println(timeStr);
    
    currentTime = String(timeStr);
    timeReceived = true;
    
    lastTimeSync = millis();
  } else {
    Serial.println("[NTP] Failed to sync time");
  }
}

// 发送时间到STM32
void sendTimeToSTM32() {
  if (currentTime.length() > 0) {
    String timeMsg = "TIME:" + currentTime + "\r\n";
    SerialSTM32.print(timeMsg);
    Serial.print("[STM32] Sent time: ");
    Serial.print(timeMsg);
  }
}

// 读取传感器数据
void readSensors() {
  // 读取火焰传感器数据
  sensorData.flameValue = analogRead(FLAME_SENSOR_AO);
  sensorData.flameDigital = digitalRead(FLAME_SENSOR_DO);
}

// 处理数据
void processData() {
  // 检测火焰（同时使用模拟值和数字值）
  if (sensorData.flameValue > FLAME_THRESHOLD || !sensorData.flameDigital) {
    sensorData.fireDetected = true;
  } else {
    sensorData.fireDetected = false;
  }
}

// 传输数据给STM32
void sendDataToSTM32() {
  // 发送格式："FLAME_AO:xxx,FLAME_DO:0/1,FIRE:0/1\n"
  SerialSTM32.print("FLAME_AO:");
  SerialSTM32.print(sensorData.flameValue);
  SerialSTM32.print(",FLAME_DO:");
  SerialSTM32.print(sensorData.flameDigital ? 1 : 0);
  SerialSTM32.print(",FIRE:");
  SerialSTM32.println(sensorData.fireDetected ? 1 : 0);
  
  Serial.print("[STM32] Sent: FLAME_AO:");
  Serial.print(sensorData.flameValue);
  Serial.print(",FLAME_DO:");
  Serial.print(sensorData.flameDigital ? 1 : 0);
  Serial.print(",FIRE:");
  Serial.println(sensorData.fireDetected ? 1 : 0);
}

// 等待STM32的确认信号
void waitForSTM32Response() {
  unsigned long startTime = millis();
  bool receivedOK = false;
  
  // 等待最多2秒
  while (millis() - startTime < 2000) {
    if (SerialSTM32.available()) {
      char c = SerialSTM32.read();
      
      // 将接收到的字符存入缓冲区
      if (rxIndex < sizeof(rxBuffer) - 1) {
        rxBuffer[rxIndex++] = c;
      }
      
      // 检查是否收到"OK\r\n"
      if (rxIndex >= 4 && 
          rxBuffer[rxIndex-4] == 'O' && 
          rxBuffer[rxIndex-3] == 'K' && 
          rxBuffer[rxIndex-2] == '\r' && 
          rxBuffer[rxIndex-1] == '\n') {
        receivedOK = true;
        Serial.println("[STM32] Received OK");
        break;
      }
    }
  }
  
  if (!receivedOK) {
    Serial.println("[Warning] No OK received from STM32");
  }
  
  // 清空接收缓冲区
  memset(rxBuffer, 0, sizeof(rxBuffer));
  rxIndex = 0;
}

// 接收STM32发送的时间
void receiveTimeFromSTM32() {
  if (SerialSTM32.available()) {
    char c = SerialSTM32.read();
    
    // 将接收到的字符存入缓冲区
    if (rxIndex < sizeof(rxBuffer) - 1) {
      rxBuffer[rxIndex++] = c;
    }
    
    // 检查是否收到完整的时间格式：TIME:YYYY-MM-DD HH:MM:SS\n
    if (c == '\n' && rxIndex > 20) {
      // 检查是否以"TIME:"开头
      if (rxBuffer[0] == 'T' && 
          rxBuffer[1] == 'I' && 
          rxBuffer[2] == 'M' && 
          rxBuffer[3] == 'E' && 
          rxBuffer[4] == ':') {
        
        // 提取时间字符串（跳过"TIME:"前缀）
        currentTime = String(rxBuffer + 5);
        
        // 移除末尾的换行符
        currentTime.trim();
        
        timeReceived = true;
        
        Serial.print("[STM32] Received time: ");
        Serial.println(currentTime);
      }
      
      // 清空接收缓冲区
      memset(rxBuffer, 0, sizeof(rxBuffer));
      rxIndex = 0;
    }
  }
}

// 打印调试信息
void printDebugInfo() {
  Serial.print("[Flame] AO: ");
  Serial.print(sensorData.flameValue);
  Serial.print(" | DO: ");
  Serial.print(sensorData.flameDigital ? "HIGH" : "LOW");
  Serial.print(" | Fire: ");
  Serial.print(sensorData.fireDetected ? "YES" : "NO");
  
  if (timeReceived) {
    Serial.print(" | Time: ");
    Serial.print(currentTime);
  }
  
  Serial.println();
}