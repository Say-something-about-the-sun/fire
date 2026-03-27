#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#define MQTT_VERSION MQTT_VERSION_3_1_1

// ==================== WiFi 配置 ====================
/**热点
const char* ssid = "vivoS15e";
const char* password = "11111111";
*/
const char* ssid = "TP-LINK_FF99";
const char* password = "pq18782975992";

// ==================== OneNET MQTT 配置 ====================
const char* mqttServer = "mqtts.heclouds.com";
const int mqttPort = 1883;
const char* productId = "PGs73D47Zf";
const char* deviceId = "stm32f407zgt6";
const char* apiKey = "version=2018-10-31&res=products%2FPGs73D47Zf%2Fdevices%2Fstm32f407zgt6&et=1805469231&method=sha1&sign=snpTD9lxQKVsom7omCk5XnSUCG8%3D";

// MQTT主题：物模型属性上报（重要：已修改为正确的物模型topic）
String topic = "$sys/" + String(productId) + "/" + String(deviceId) + "/thing/property/post";

// MQTT客户端ID、用户名、密码
const char* clientId = deviceId;
const char* mqttUser = productId;
const char* mqttPass = apiKey;

// ==================== 串口配置 ====================
SoftwareSerial stm32Serial(D5, D6);  // RX, TX

WiFiClient espClient;
PubSubClient mqttClient(espClient);

String receivedJson = "";

// 函数声明
void connectWiFi();
void connectMQTT();
void processJsonAndSend(String jsonStr);

// ==================== setup() ====================
void setup() {
  Serial.begin(9600);
  stm32Serial.begin(9600);
  
  Serial.println("ESP8266 MQTT Gateway Starting...");
  Serial.print("Connecting to WiFi");
  
  connectWiFi();
  mqttClient.setServer(mqttServer, mqttPort);
  mqttClient.setCallback(mqttCallback);  // <--- 绑定回调函数


  mqttClient.setBufferSize(2048);
  Serial.println("Setup complete. Waiting for data from STM32...");
}

// ==================== loop() ====================
void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }
  
  if (!mqttClient.connected()) {
    connectMQTT();
  }
  
  mqttClient.loop();
  
  // 接收STM32发来的JSON数据
  while (stm32Serial.available()) {
    char c = stm32Serial.read();
    
    if (c == '\n') {
      if (receivedJson.length() > 0) {
        Serial.print("Received JSON: ");
        Serial.println(receivedJson);
        processJsonAndSend(receivedJson);
        receivedJson = "";
      }
    } else {
      receivedJson += c;
    }

    if (!stm32Serial.available()) {
        delay(2); 
    }



  }
  
  // 从电脑串口转发数据到STM32（调试用）
  if (Serial.available()) {
    char c = Serial.read();
    stm32Serial.write(c);
  }
}

// ==================== 连接WiFi ====================
void connectWiFi() {
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 60) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.print("WiFi connected! IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println();
    Serial.println("WiFi connection failed! Check credentials.");
  }
}

// ==================== 连接MQTT ====================
void connectMQTT() {
  Serial.print("Connecting to OneNET MQTT...");
  
  if (mqttClient.connect(clientId, mqttUser, mqttPass)) {
    Serial.println(" connected!");
    String replyTopic = "$sys/" + String(productId) + "/" + String(deviceId) + "/thing/property/post/reply";
    mqttClient.subscribe(replyTopic.c_str());
    Serial.println("Subscribed to reply topic!");


  } else {
    Serial.print(" failed, rc=");
    Serial.print(mqttClient.state());
    Serial.println(" retry in 5 seconds");
    delay(5000);
  }
}

// ==================== 处理JSON并发送到OneNET（物模型格式）====================
void processJsonAndSend(String jsonStr) {
    // 1. 解析STM32发来的完整JSON
   DynamicJsonDocument doc(1536);
    DeserializationError error = deserializeJson(doc, jsonStr);
    
    if (error) {
        Serial.print("Invalid JSON: ");
        Serial.println(error.c_str());
        return;
    }
    
    // 2. 构造物模型格式 
    // 【修改点】：将 1024 扩大到 2048，防止新增多个嵌套节点后内存池溢出！
    DynamicJsonDocument sendDoc(2048);
    
    // 物模型必须包含id和version
    sendDoc["id"] = String(millis());  // 使用动态ID，避免重复
    sendDoc["version"] = "1.0";
    
    // 创建params对象
    JsonObject params = sendDoc.createNestedObject("params");
    
    // ========== 从STM32的JSON中提取数据 ==========
    
    // 1) 顶层字段
    if (doc.containsKey("device")) {
        JsonObject obj = params.createNestedObject("device");
        obj["value"] = doc["device"].as<String>();
    }
    
    if (doc.containsKey("timestamp")) {
        JsonObject obj = params.createNestedObject("timestamp");
        obj["value"] = doc["timestamp"].as<String>();
    }
    
    if (doc.containsKey("risk_level")) {
        JsonObject obj = params.createNestedObject("risk_level");
        obj["value"] = doc["risk_level"].as<int>();
    }
    
    if (doc.containsKey("confidence")) {
        JsonObject obj = params.createNestedObject("confidence");
        obj["value"] = doc["confidence"].as<float>();
    }
    
    if (doc.containsKey("frame_count")) {
        JsonObject obj = params.createNestedObject("frame_count");
        obj["value"] = doc["frame_count"].as<int>();
    }
    
    if (doc.containsKey("dropped_frames")) {
        JsonObject obj = params.createNestedObject("dropped_frames");
        obj["value"] = doc["dropped_frames"].as<int>();
    }

    // 👇 【新增】：提取根目录的水泵状态
    if (doc.containsKey("pump_status")) {
        JsonObject obj = params.createNestedObject("pump_status");
        obj["value"] = doc["pump_status"].as<int>()== 1 ? true : false;
    }

    // 👇 【本次新增】：提取系统模式 (自动/手动)
    if (doc.containsKey("system_mode")) {
        JsonObject obj = params.createNestedObject("system_mode");
        // 因为模式通常是 0 或 1，建议当做整数 (int) 传给云端
        obj["value"] = doc["system_mode"].as<int>();
    }

    // 👇 【本次新增】：提取主电源跳闸状态
    if (doc.containsKey("main_power_status")) {
        JsonObject obj = params.createNestedObject("main_power_status");
        // 电源的通断属于布尔值 (bool)，为了防止 OneNET 报错，强转为 true/false
        obj["value"] = doc["main_power_status"].as<int>() == 1 ? true : false;
    }
    
    // 2) sensors对象内的字段
    if (doc.containsKey("sensors")) {
        JsonObject sensors = doc["sensors"].as<JsonObject>();
        
        if (sensors.containsKey("flame_adc")) {
            JsonObject obj = params.createNestedObject("flame_adc");
            obj["value"] = sensors["flame_adc"].as<int>();
        }
        
        if (sensors.containsKey("flame_do")) {
            JsonObject obj = params.createNestedObject("flame_do");
            obj["value"] = sensors["flame_do"].as<int>();
        }
        
        if (sensors.containsKey("smoke_adc")) {
            JsonObject obj = params.createNestedObject("smoke_adc");
            obj["value"] = sensors["smoke_adc"].as<int>();
        }
        
        if (sensors.containsKey("smoke_ppm")) {
            JsonObject obj = params.createNestedObject("smoke_ppm");
            obj["value"] = sensors["smoke_ppm"].as<float>();
        }
        
        if (sensors.containsKey("smoke_detected")) {
            JsonObject obj = params.createNestedObject("smoke_detected");
            obj["value"] = sensors["smoke_detected"].as<bool>();
        }
        
        if (sensors.containsKey("temperature")) {
            JsonObject obj = params.createNestedObject("temperature");
            obj["value"] = sensors["temperature"].as<float>();
        }
        
        if (sensors.containsKey("temp_detected")) {
            JsonObject obj = params.createNestedObject("temp_detected");
            obj["value"] = sensors["temp_detected"].as<int>();
        }

        // 👇 【新增】：提取湿度
        if (sensors.containsKey("humidity")) {
            JsonObject obj = params.createNestedObject("humidity");
            obj["value"] = sensors["humidity"].as<float>();
        }

        // 👇 【新增】：提取虚拟电流（注意：这里转成 OneNET 上的 "current" 标识符）
        if (sensors.containsKey("virtual_current")) {
            JsonObject obj = params.createNestedObject("current");
            obj["value"] = sensors["virtual_current"].as<float>();
        }
    }
    
    // 3) fire_detection对象内的字段
    if (doc.containsKey("fire_detection")) {
        JsonObject fireDet = doc["fire_detection"].as<JsonObject>();
        
        if (fireDet.containsKey("esp32_fire_detected")) {
            JsonObject obj = params.createNestedObject("esp32_fire_detected");
            obj["value"] = fireDet["esp32_fire_detected"].as<bool>();
        }
        
        if (fireDet.containsKey("image_fire_detected")) {
            JsonObject obj = params.createNestedObject("image_fire_detected");
            obj["value"] = fireDet["image_fire_detected"].as<bool>();
        }
        
        if (fireDet.containsKey("image_confidence")) {
            JsonObject obj = params.createNestedObject("image_confidence");
            obj["value"] = fireDet["image_confidence"].as<float>();
        }
        
        if (fireDet.containsKey("fire_area")) {
            JsonObject obj = params.createNestedObject("fire_area");
            obj["value"] = fireDet["fire_area"].as<int>();
        }
        
        if (fireDet.containsKey("fire_center_x")) {
            JsonObject obj = params.createNestedObject("fire_center_x");
            obj["value"] = fireDet["fire_center_x"].as<int>();
        }
        
        if (fireDet.containsKey("fire_center_y")) {
            JsonObject obj = params.createNestedObject("fire_center_y");
            obj["value"] = fireDet["fire_center_y"].as<int>();
        }
        
        if (fireDet.containsKey("flicker_detected")) {
            JsonObject obj = params.createNestedObject("flicker_detected");
            obj["value"] = fireDet["flicker_detected"].as<int>();
        }
    }
    
    // 3. 序列化并发送
    String mqttPayload;
    serializeJson(sendDoc, mqttPayload);
    
    Serial.print("Publishing (ThingModel): ");
    Serial.println(mqttPayload);
    
    if (mqttClient.publish(topic.c_str(), mqttPayload.c_str())) {
        Serial.println("Published successfully!");
    } else {
        Serial.println("Publish failed!");
    }
}
// ==================== 接收 OneNET 报错回执 ====================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("\n[OneNET Reply] -> ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println("\n");
}



