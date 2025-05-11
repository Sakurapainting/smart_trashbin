#include <Wire.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

/*MQTT连接配置*/
/*-----------------------------------------------------*/
// 定义WiFi连接信息
const char* ssid = "skrp";
const char* password = "skrptskrpt";
const char* mqttServer = "6e69a63a03.st1.iotda-device.cn-north-4.myhuaweicloud.com";
const int  mqttPort = 1883;
//以下3个参数可以由HMACSHA256算法生成，为硬件通过MQTT协议接入华为云IoT平台的鉴权依据
const char* clientId = "67fdc5932902516e867178b0_0000_0_0_2025051107";
const char* mqttUser = "67fdc5932902516e867178b0_0000";
const char* mqttPassword = "1e627ebb0eabf0ff64750536ff59309c81dd56c803f834b8d80791e109cc1b40";
WiFiClient espClient; //ESP32WiFi模型定义
PubSubClient client(espClient);
 
const char* topic_properties_report = "$oc/devices/67fdc5932902516e867178b0_0000/sys/properties/report";
 
//接收到命令后上发的响应topic
char* topic_Commands_Response = "$oc/devices/67fdc5932902516e867178b0_0000/sys/properties/report";

#define Iot_link_Body_Format "{\"services\":[{\"service_id\":\"Dev_data\",\"properties\":{%s"  

// 定义引脚
const int LIGHT_SENSOR_PIN = 32;  // 光敏电阻
const int DUAL_COLOR_LED_PIN = 14;  // 双色灯

// 定义光线阈值
const int LIGHT_THRESHOLD = 2000;  // 阈值需要根据实际环境调整

void MQTT_Init()
{
//WiFi网络连接部分
  WiFi.begin(ssid, password); //开启ESP32的WiFi
  while (WiFi.status() != WL_CONNECTED) { //ESP尝试连接到WiFi网络
    delay(3000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to the WiFi network");
 
 
//MQTT服务器连接部分
  client.setServer(mqttServer, mqttPort); //设置连接到MQTT服务器的参数
  client.setKeepAlive (60); //设置心跳时间
  while (!client.connected()) { //尝试与MQTT服务器建立连接
    Serial.println("Connecting to MQTT...");
    if (client.connect(clientId, mqttUser, mqttPassword )) {
      Serial.println("connected");  
    } else {
      Serial.print("failed with state ");
      Serial.print(client.state());
      delay(6000);
    }
  }
//接受平台下发内容的初始化
  //client.setCallback(callback); //可以接受任何平台下发的内容
}

void setup() {
  // 初始化串口通信，用于调试
  Serial.begin(115200);
  
  // 设置引脚模式
  pinMode(LIGHT_SENSOR_PIN, INPUT);  // 光敏电阻为输入模式
  pinMode(DUAL_COLOR_LED_PIN, OUTPUT);  // 双色灯为输出模式
  
  MQTT_Init();

  Serial.println("智能垃圾桶环境光线检测系统初始化完成");
}

void TASK_Capacity_Report(int capacity)
{
 
//以下部分代码调用了ArduinoJson库将属性上报消息打包为JSON格式
//此部分代码可以通过ArduinoJson库的官方网站直接生成
  StaticJsonBuffer<300> JSONbuffer; //定义静态的JSON缓冲区用于存储JSON消息
  JsonObject& root = JSONbuffer.createObject();
  JsonArray& services = root.createNestedArray("services");
  JsonObject& service_1 = services.createNestedObject();
  JsonObject& properties_1_1 = service_1.createNestedObject("properties");
 
  service_1["service_id"] = "A311";
  properties_1_1["light"] = capacity;
 
//  root.prettyPrintTo(Serial);//调试用，将JSON打印到串口
 
//以下将生成好的JSON格式消息格式化输出到字符数组中，便于下面通过PubSubClient库发送到服务器
  char JSONmessageBuffer[100];
  root.printTo(JSONmessageBuffer, sizeof(JSONmessageBuffer));
  
  Serial.println("Sending message to MQTT topic..");
  Serial.println(JSONmessageBuffer);
 
//以下代码将打包好的JSON数据包通过PubSubClient库发送到服务器 
  if (client.publish(topic_properties_report, JSONmessageBuffer) == true) {
    Serial.println("Success sending message");
  } else {
    Serial.println("Error sending message");
  }
//由于本函数是放在loop中不断循环执行，所以添加client.loop()进行保持硬件的活跃度
//避免由于长时间未向服务器发送消息而被服务器踢下线
  client.loop();
  Serial.println("-------------");
  
}

void loop() {
  // 读取光敏电阻的值
  int lightValue = analogRead(LIGHT_SENSOR_PIN);
  
  // 打印光线值，用于调试
  Serial.print("当前光线值: ");
  Serial.println(lightValue);
  
  // 根据光线值判断环境
  if (lightValue < LIGHT_THRESHOLD) {
    // 有光环境
    Serial.println("检测到有光环境");
    digitalWrite(DUAL_COLOR_LED_PIN, LOW);  
  } else {
    // 无光环境
    Serial.println("检测到无光环境");
    digitalWrite(DUAL_COLOR_LED_PIN, HIGH);  
  }
  
  TASK_Capacity_Report(lightValue);
  // 延时一段时间再进行下一次检测
  delay(10000);
}
