//#ifndef __MQTTCLIENT_H
//#define __MQTTCLIENT_H

////#ifndef __MQTTCLIENT_H
////#define __MQTTCLIENT_H
//#include "stm32f4xx.h"
//#include "lwipopts.h"

//#include "FreeRTOS.h"
//#include "task.h"
//#include "queue.h"
//#include "semphr.h"

//#define   MSG_MAX_LEN     600
//#define   MSG_TOPIC_LEN   100
//#define   KEEPLIVE_TIME   50
//#define   MQTT_VERSION    4





//        #if    LWIP_DNS
//				#define   HOST_NAME       "alazcyy.iot.gz.baidubce.com"     //服务器域名
//				#else
//				#define   HOST_NAME02       "157.255.71.49"     //服务器IP地址
//				#endif


//				//#define   HOST_IP       "129.204.201.235"
////				#define   HOST_PORT     1883    //由于是TCP连接，端口必须是1883

//				#define   CLIENT_ID02     "cs01"         //随机的id
//				#define   USER_NAME02     "thingidp@alazcyy|cs01|0|MD5"     //用户名
//				#define   PASSWORD02      "c8659419adf6742aa6887f2e57fde1e6"  //秘钥

//				#define   TOPIC02         "$iot/cs01/user/ppt"      //订阅的主题
//				#define   TOPP02          "$iot/cs01/user/ppt"  
//				//#define   TOPIC         "/glmlemaRLed/sxcs01/user/cs01"      //订阅的主题
//				//#define   TOPP          "/glmlemaRLed/sxcs01/user/cs01"  
//				#define   TEST_MESSAGE  "test_message"  //发送测试消息
//		
//        #if    LWIP_DNS
//				#define   HOST_NAME       "iot-06z009y0rsfybop.mqtt.iothub.aliyuncs.com"     //服务器域名
//				#else
//				#define   HOST_NAME       "47.103.191.238"     //服务器IP地址
//				#endif


//				//#define   HOST_IP       "129.204.201.235"
//				#define   HOST_PORT     1883   //由于是TCP连接，端口必须是1883

//				#define   CLIENT_ID     "glmlemaRLed.sxcs01|securemode=2,signmethod=hmacsha256,timestamp=2524608000000|"         //随机的id
//				#define   USER_NAME     "sxcs01&glmlemaRLed"     //用户名
//				#define   PASSWORD      "ea67a89cf6cf231036696cac7df52212e1cc7afe18255d6a211fcd6114f1048c"  //秘钥

//				#define   TOPIC         "/sys/glmlemaRLed/sxcs01/thing/event/property/post_reply"      //订阅的主题
//				#define   TOPP          "/sys/glmlemaRLed/sxcs01/thing/event/property/post"  
//				//#define   TOPIC         "/glmlemaRLed/sxcs01/user/cs01"      //订阅的主题
//				//#define   TOPP          "/glmlemaRLed/sxcs01/user/cs01"  
//				#define   TEST_MESSAGE  "test_message"  //发送测试消息
////#endif
//typedef struct
//{
//  uint8_t  humi_high8bit;                //原始数据：湿度高8位
//  uint8_t  humi_low8bit;                 //原始数据：湿度低8位
//  uint8_t  temp_high8bit;                 //原始数据：温度高8位
//  uint8_t  temp_low8bit;                 //原始数据：温度高8位
//  uint8_t  check_sum;                     //校验和
//  double    humidity;        //实际湿度
//  double    temperature;     //实际温度  
//} DHT11_Data_TypeDef;




//enum QoS 
//{ QOS0 = 0, 
//  QOS1, 
//  QOS2 
//};

//enum MQTT_Connect
//{
//  Connect_OK = 0,
//  Connect_NOK,
//  Connect_NOTACK
//};


////数据交互结构体
//typedef struct __MQTTMessage
//{
//    uint32_t qos;
//    uint8_t retained;
//    uint8_t dup;
//    uint16_t id;
//	  uint8_t type;
//    void *payload;
//    int32_t payloadlen;
//}MQTTMessage;

////用户接收消息结构体
//typedef struct __MQTT_MSG
//{
//	  uint8_t  msgqos;                 //消息质量
//		uint8_t  msg[MSG_MAX_LEN];       //消息
//	  uint32_t msglenth;               //消息长度
//	  uint8_t  topic[MSG_TOPIC_LEN];   //主题    
//	  uint16_t packetid;               //消息ID
//	  uint8_t  valid;                  //标明消息是否有效
//}MQTT_USER_MSG;

////发送消息结构体
//typedef struct
//{
//    int8_t topic[MSG_TOPIC_LEN];
//    int8_t qos;
//    int8_t retained;

//    uint8_t msg[MSG_MAX_LEN];
//    uint8_t msglen;
//} mqtt_recv_msg_t, *p_mqtt_recv_msg_t, mqtt_send_msg_t, *p_mqtt_send_msg_t;


//void mqtt_thread( void *pvParameters);

///************************************************************************
//** 函数名称: my_mqtt_send_pingreq								
//** 函数功能: 发送MQTT心跳包
//** 入口参数: 无
//** 出口参数: >=0:发送成功 <0:发送失败
//** 备    注: 
//************************************************************************/
//int32_t MQTT_PingReq(int32_t sock);

///************************************************************************
//** 函数名称: MQTT_Connect								
//** 函数功能: 登录服务器
//** 入口参数: int32_t sock:网络描述符
//** 出口参数: Connect_OK:登陆成功 其他:登陆失败
//** 备    注: 
//************************************************************************/
//uint8_t MQTT_Connect(void);
//uint8_t MQTT_Connect02(void);

///************************************************************************
//** 函数名称: MQTTSubscribe								
//** 函数功能: 订阅消息
//** 入口参数: int32_t sock：套接字
//**           int8_t *topic：主题
//**           enum QoS pos：消息质量
//** 出口参数: >=0:发送成功 <0:发送失败
//** 备    注: 
//************************************************************************/
//int32_t MQTTSubscribe(int32_t sock,char *topic,enum QoS pos);

///************************************************************************
//** 函数名称: UserMsgCtl						
//** 函数功能: 用户消息处理函数
//** 入口参数: MQTT_USER_MSG  *msg：消息结构体指针
//** 出口参数: 无
//** 备    注: 
//************************************************************************/
//void UserMsgCtl(MQTT_USER_MSG  *msg);

///************************************************************************
//** 函数名称: GetNextPackID						
//** 函数功能: 产生下一个数据包ID
//** 入口参数: 无
//** 出口参数: uint16_t packetid:产生的ID
//** 备    注: 
//************************************************************************/
//uint16_t GetNextPackID(void);

///************************************************************************
//** 函数名称: mqtt_msg_publish						
//** 函数功能: 用户推送消息
//** 入口参数: MQTT_USER_MSG  *msg：消息结构体指针
//** 出口参数: >=0:发送成功 <0:发送失败
//** 备    注: 
//************************************************************************/
////int32_t MQTTMsgPublish(int32_t sock, char *topic, int8_t qos, int8_t retained,uint8_t* msg,uint32_t msg_len);
//int32_t MQTTMsgPublish(int32_t sock, char *topic, int8_t qos, uint8_t* msg);
///************************************************************************
//** 函数名称: ReadPacketTimeout					
//** 函数功能: 阻塞读取MQTT数据
//** 入口参数: int32_t sock:网络描述符
//**           uint8_t *buf:数据缓存区
//**           int32_t buflen:缓冲区大小
//**           uint32_t timeout:超时时间--0-表示直接查询，没有数据立即返回
//** 出口参数: -1：错误,其他--包类型
//** 备    注: 
//************************************************************************/
//int32_t ReadPacketTimeout(int32_t sock,uint8_t *buf,int32_t buflen,uint32_t timeout);

///************************************************************************
//** 函数名称: mqtt_pktype_ctl						
//** 函数功能: 根据包类型进行处理
//** 入口参数: uint8_t packtype:包类型
//** 出口参数: 无
//** 备    注: 
//************************************************************************/
//void mqtt_pktype_ctl(uint8_t packtype,uint8_t *buf,uint32_t buflen);

///************************************************************************
//** 函数名称: WaitForPacket					
//** 函数功能: 等待特定的数据包
//** 入口参数: int32_t sock:网络描述符
//**           uint8_t packettype:包类型
//**           uint8_t times:等待次数
//** 出口参数: >=0:等到了特定的包 <0:没有等到特定的包
//** 备    注: 
//************************************************************************/
//int32_t WaitForPacket(int32_t sock,uint8_t packettype,uint8_t times);


//u8 mqtt_thread_init(void);

//#endif


