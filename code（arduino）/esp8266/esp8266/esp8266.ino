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
void mqttCallback(char* topic, byte* payload, unsigned int length);

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
  // 1. 无脑死守 WiFi 链路
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }
  
  // 2. 无脑死守 MQTT 链路 (🚨 移除了原先的心跳判断，只要断了就无限重连)
  if (!mqttClient.connected()) {
    connectMQTT();
  }
  
  mqttClient.loop();
  
  // 3. 极速转发 STM32 数据
  while (stm32Serial.available()) 
  {
    char c = stm32Serial.read();
    
    if (c == '\n') {
      if (receivedJson.length() > 0) {
        Serial.print("Received JSON: ");
        Serial.println(receivedJson);
        processJsonAndSend(receivedJson); // 你的发送函数
        receivedJson = "";
      }
    } else {
      receivedJson += c;
    }

    if (!stm32Serial.available()) {
        delay(2); 
    }
  }

  // 4. 从电脑串口转发数据到STM32（调试用）
  if (Serial.available()) {
    char c = Serial.read();
    stm32Serial.write(c);
  }
}

// ==================== 连接WiFi ====================
void connectWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 60) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\r\nWiFi connected! IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\r\nWiFi connection failed!");
  }
}

// ==================== 连接MQTT ====================
void connectMQTT() {
  Serial.print("Connecting to OneNET MQTT...");
  
  if (mqttClient.connect(clientId, mqttUser, mqttPass)) {
    Serial.println(" connected!");
    
    // 订阅云端指令通道 (保持不变)
    String setTopic = "$sys/" + String(productId) + "/" + String(deviceId) + "/thing/property/set";
    mqttClient.subscribe(setTopic.c_str());
    Serial.println("Subscribed to SET command topic!");

  } else {
    Serial.print(" failed, rc=");
    Serial.print(mqttClient.state());
    Serial.println(" retry in 5 seconds");
    delay(5000);
  }
}

// ==================== 处理JSON并发送到OneNET（物模型格式）====================
void processJsonAndSend(String jsonStr) {

  // 在干活之前，先看看自己是不是断网了。如果断网，立刻向 STM32 喊救命！
    if (WiFi.status() != WL_CONNECTED || !mqttClient.connected()) {
        Serial.println("Network disconnected! Cannot send.");
        stm32Serial.println("WIFI_ERR"); // 👉 告诉 STM32：我网断了，快切备用链路！
        return; 
    }


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
        stm32Serial.println("UPLOAD_OK");  // 👉 告诉 STM32：完美送达，不用担心！
    } else {
        Serial.println("Publish failed!");
        stm32Serial.println("UPLOAD_ERR"); // 👉 告诉 STM32：发送失败，可能被 OneNET 限流或拒收！
    }
}
// ==================== 接收 OneNET 报错回执 ====================


void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // 1. 先把你原来的 payload 转换成完整的 String 字符串
  String msg = "";
  for (int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }
  
  // 打印所有收到的 Topic 和内容 (完美保留了你原来的调试功能)
  Serial.print("\n[OneNET Received] Topic: ");
  Serial.println(topic);
  Serial.print("[OneNET Payload] -> ");
  Serial.println(msg);

  // 2. 定义 OneNET 下发控制指令的专属 Topic
  String set_topic = "$sys/PGs73D47Zf/stm32f407zgt6/thing/property/set";

  // 3. 快递分拣：判断这是不是控制指令？
if (String(topic) == set_topic) {
    // 分配 JSON 解析内存池
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, msg);
    
    if (!error) {
        // 📦 只要收到 params 包裹，就把它拆开
        if (doc.containsKey("params")) {
            JsonObject params = doc["params"]; // 声明一次 params 即可
            
            // 🎯 提取动作一：水泵控制指令
            if (params.containsKey("pump_status")) {
                bool pump_cmd = params["pump_status"];
                if (pump_cmd == true) {
                    Serial.println("CMD:PUMP:1");      
                    stm32Serial.println("CMD:PUMP:1"); 
                } else {
                    Serial.println("CMD:PUMP:0");      
                    stm32Serial.println("CMD:PUMP:0"); 
                }
            }
            
            // 🎯 提取动作二：模式切换指令 (必须放在 params 还在存活的范围内)
            if (params.containsKey("system_mode")) {
                int mode_cmd = params["system_mode"];
                if (mode_cmd == 1) {
                    Serial.println("CMD:MODE:1");      
                    stm32Serial.println("CMD:MODE:1"); // 发给 STM32 (切为手动)
                } else {
                    Serial.println("CMD:MODE:0");      
                    stm32Serial.println("CMD:MODE:0"); // 发给 STM32 (恢复自动)
                }
            }
        } // params 包裹处理完毕

        // 4. 【必须动作】给 OneNET 回复签收确认单 (ACK)
        String msg_id = doc["id"].as<String>();
        String reply_topic = "$sys/PGs73D47Zf/stm32f407zgt6/thing/property/set_reply";
        String reply_payload = "{\"id\":\"" + msg_id + "\",\"code\":200,\"msg\":\"success\"}";
        
        mqttClient.publish(reply_topic.c_str(), reply_payload.c_str());
        Serial.println("[Cloud CMD] -> 已成功向 OneNET 回复 ACK 确认包！");
    } else {
        Serial.println("[Cloud CMD Error] -> JSON 解析失败!");
    }
}
  else {
      // 如果不是 set_topic，那大概率就是你原来的 reply 回执。
      // 因为上面第1步已经打印过了，这里不需要做任何处理，静静地看着就好。
  }
}





