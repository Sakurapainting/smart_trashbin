#include <Wire.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>
#include <WebServer.h>   // 添加WebServer库

#define Serpin 25//舵机
#define bottonpin 27//按键
Servo myServo;
int firepin = 35;     
int speakerPin = 26;     
int val;
int val_botton;
int prev_botton_state = HIGH; // 记录上一次按键状态
int servo_position = 0; // 舵机位置状态：0-初始90度, 1-0度, 2-180度
bool button_pressed = false; // 按键按下标志

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

// WebServer相关变量
WebServer server(80);  // 创建WebServer对象，端口80
String lightValueStr = "0";  // 存储光线值的字符串
String fireStatusStr = "无火焰";  // 存储火焰状态的字符串
String binStatusStr = "已关闭";   // 存储垃圾桶状态的字符串

// 添加IP地址变量
String ipAddress = "";
unsigned long lastIPDisplayTime = 0;
const unsigned long IP_DISPLAY_INTERVAL = 30000; // 每30秒显示一次IP地址

// 树莓派通信相关变量
#define RX_PIN 16     // 用于接收树莓派数据的接收引脚
#define TX_PIN 17     // 用于向树莓派发送数据的发送引脚
#define RPI_SERIAL Serial2  // 使用ESP32的第二个硬件串口

// 垃圾类型定义
#define WASTE_TYPE_HARMFUL   0  // 有害垃圾 (00,h)
#define WASTE_TYPE_KITCHEN   1  // 厨余垃圾 (01,k)
#define WASTE_TYPE_OTHER     2  // 其它垃圾 (10,o)
#define WASTE_TYPE_RECYCLING 3  // 可回收垃圾 (11,r)

// 定义不同垃圾类型对应的舵机角度
#define SERVO_ANGLE_HARMFUL    30  // 有害垃圾对应的舵机角度
#define SERVO_ANGLE_KITCHEN    70  // 厨余垃圾对应的舵机角度
#define SERVO_ANGLE_OTHER      110 // 其它垃圾对应的舵机角度
#define SERVO_ANGLE_RECYCLING  150 // 可回收垃圾对应的舵机角度
#define SERVO_ANGLE_DEFAULT    90  // 默认位置

// 垃圾类型状态变量
int currentWasteType = -1;  // -1表示未检测到垃圾类型
String wasteTypeStr = "未识别";  // 存储垃圾类型的字符串

// 其他变量
unsigned long lastTrashDataTime = 0;
const unsigned long TRASH_DATA_INTERVAL = 2000; // 数据上报间隔(毫秒)

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

// HTML页面
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>智能垃圾桶控制系统</title>
  <style>
    body { font-family: Arial; text-align: center; margin:0px auto; padding-top: 30px; }
    .container { display: flex; flex-direction: column; margin: 0 auto; width: 80%; max-width: 500px; }
    .card { background-color: white; box-shadow: 0px 0px 10px 1px rgba(0,0,0,0.1); border-radius: 5px; padding: 20px; margin: 15px 0; }
    .button { background-color: #4CAF50; border: none; color: white; padding: 15px 32px; text-align: center; font-size: 16px; margin: 10px; cursor: pointer; border-radius: 5px; }
    .danger { background-color: #f44336; }
    .normal { background-color: #2196F3; }
    .data { font-size: 24px; font-weight: bold; margin: 10px 0; }
    .status { padding: 10px; border-radius: 5px; margin: 10px 0; }
  </style>
</head>
<body>
  <div class="container">
    <h1>智能垃圾桶远程监控系统</h1>
    
    <div class="card">
      <h2>传感器数据</h2>
      <p>光线值: <span class="data" id="lightValue">%LIGHT%</span></p>
      <p>火焰状态: <span class="data" id="fireStatus">%FIRE%</span></p>
      <p>垃圾桶状态: <span class="data" id="binStatus">%BIN%</span></p>
      <p>垃圾类型: <span class="data" id="wasteType">%WASTE%</span></p>
    </div>
    
    <div class="card">
      <h2>远程控制</h2>
      <button class="button" onclick="controlServo('open')">打开垃圾桶</button>
      <button class="button" onclick="controlServo('close')">关闭垃圾桶</button>
    </div>
  </div>
  
  <script>
    setInterval(function() {
      getData();
    }, 5000); // 每5秒更新一次数据
    
    function getData() {
      var xhr = new XMLHttpRequest();
      xhr.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) {
          var data = JSON.parse(this.responseText);
          document.getElementById("lightValue").innerText = data.light;
          document.getElementById("fireStatus").innerText = data.fire;
          document.getElementById("binStatus").innerText = data.bin;
          document.getElementById("wasteType").innerText = data.waste;
        }
      };
      xhr.open("GET", "/data", true);
      xhr.send();
    }
    
    function controlServo(action) {
      var xhr = new XMLHttpRequest();
      xhr.open("GET", "/servo?action=" + action, true);
      xhr.send();
    }
  </script>
</body>
</html>
)rawliteral";

// 处理根路径请求
void handleRoot() {
  String html = String(index_html);
  html.replace("%LIGHT%", lightValueStr);
  html.replace("%FIRE%", fireStatusStr);
  html.replace("%BIN%", binStatusStr);
  html.replace("%WASTE%", wasteTypeStr);
  server.send(200, "text/html", html);
}

// 处理数据请求
void handleData() {
  String json = "{";
  json += "\"light\":\"" + lightValueStr + "\",";
  json += "\"fire\":\"" + fireStatusStr + "\",";
  json += "\"bin\":\"" + binStatusStr + "\",";
  json += "\"waste\":\"" + wasteTypeStr + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

// 处理舵机控制请求
void handleServo() {
  String action = server.arg("action");
  
  if (action == "open") {
    // 正转打开垃圾桶
    Serial.println("网页控制：舵机正转");
    myServo.write(0);  // 正转
    delay(125);
    // myServo.write(90); // 停止
    binStatusStr = "已打开";
    server.send(200, "text/plain", "垃圾桶已打开");
  } 
  else if (action == "close") {
    // 反转关闭垃圾桶
    Serial.println("网页控制：舵机反转");
    myServo.write(180); // 反转
    delay(125);
    // myServo.write(90);  // 停止
    binStatusStr = "已关闭";
    server.send(200, "text/plain", "垃圾桶已关闭");
  }
  else {
    server.send(400, "text/plain", "无效命令");
  }
}

// 新增函数：将垃圾类型数据上报到华为云
void reportWasteTypeToCloud(int wasteType, String wasteTypeStr) {
  // 此函数是华为云IoT平台接口
  StaticJsonBuffer<300> JSONbuffer;
  JsonObject& root = JSONbuffer.createObject();
  JsonArray& services = root.createNestedArray("services");
  JsonObject& service_1 = services.createNestedObject();
  JsonObject& properties_1_1 = service_1.createNestedObject("properties");
 
  service_1["service_id"] = "A311";
  properties_1_1["light"] = analogRead(LIGHT_SENSOR_PIN); 
  properties_1_1["wasteType"] = wasteType; 
  properties_1_1["wasteTypeStr"] = wasteTypeStr; 
 
  char JSONmessageBuffer[200];
  root.printTo(JSONmessageBuffer, sizeof(JSONmessageBuffer));
  
  Serial.println("Sending waste type data to MQTT topic..");
  Serial.println(JSONmessageBuffer);
 
  if (client.publish(topic_properties_report, JSONmessageBuffer) == true) {
    Serial.println("Success sending waste type message");
  } else {
    Serial.println("Error sending waste type message");
  }
  
  client.loop();
}

// 新增函数：获取垃圾类型的字符串描述
String getWasteTypeString(int type) {
  switch(type) {
    case WASTE_TYPE_HARMFUL:
      return "有害垃圾(H)";
    case WASTE_TYPE_KITCHEN:
      return "厨余垃圾(K)";
    case WASTE_TYPE_OTHER:
      return "其它垃圾(O)";
    case WASTE_TYPE_RECYCLING:
      return "可回收垃圾(R)";
    default:
      return "未识别";
  }
}

// 新增函数：根据垃圾类型控制舵机转到对应角度
void rotateServoByWasteType(int wasteType) {
  int targetAngle = SERVO_ANGLE_DEFAULT;
  
  switch(wasteType) {
    case WASTE_TYPE_HARMFUL:
      targetAngle = SERVO_ANGLE_HARMFUL;
      Serial.println("舵机旋转到有害垃圾位置");
      break;
    case WASTE_TYPE_KITCHEN:
      targetAngle = SERVO_ANGLE_KITCHEN;
      Serial.println("舵机旋转到厨余垃圾位置");
      break;
    case WASTE_TYPE_OTHER:
      targetAngle = SERVO_ANGLE_OTHER;
      Serial.println("舵机旋转到其它垃圾位置");
      break;
    case WASTE_TYPE_RECYCLING:
      targetAngle = SERVO_ANGLE_RECYCLING;
      Serial.println("舵机旋转到可回收垃圾位置");
      break;
    default:
      targetAngle = SERVO_ANGLE_DEFAULT;
      Serial.println("舵机旋转到默认位置");
      break;
  }
  
  // 控制舵机转到目标角度
  myServo.write(targetAngle);
  delay(200); // 最小等待时间，确保舵机开始移动
  
  // 更新垃圾桶状态显示
  binStatusStr = "分类中: " + getWasteTypeString(wasteType);
  
  // 缩短等待时间到1秒
  delay(1000);
  myServo.write(SERVO_ANGLE_DEFAULT);
  Serial.println("舵机回到默认位置");
  binStatusStr = "待机中";
}

void setup() {
  // 初始化串口通信，用于调试
  Serial.begin(115200);
  
  // 初始化与树莓派通信的串口
  RPI_SERIAL.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);
  
  myServo.attach(Serpin);
  myServo.write(90); // 初始位置设为90度
  
  // 设置引脚模式
  pinMode(LIGHT_SENSOR_PIN, INPUT);  // 光敏电阻为输入模式
  pinMode(DUAL_COLOR_LED_PIN, OUTPUT);  // 双色灯为输出模式

  pinMode(firepin, INPUT_PULLUP);  // 启用内部上拉
  pinMode(speakerPin, OUTPUT);
  pinMode(bottonpin,INPUT);
  
  MQTT_Init();

  // 设置WebServer路由
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/servo", handleServo);
  
  // 启动WebServer
  server.begin();
  
  // 获取并保存IP地址
  ipAddress = WiFi.localIP().toString();
  
  // 显著格式输出IP地址
  Serial.println("\n====================================");
  Serial.println("     智能垃圾桶控制系统已启动      ");
  Serial.println("====================================");
  Serial.println("Web服务器已启动，请访问以下地址：");
  Serial.println("http://" + ipAddress);
  Serial.println("树莓派通信已初始化，等待接收垃圾分类数据");
  Serial.println("====================================\n");
  
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
  // 处理Web服务器客户端请求
  server.handleClient();
  
  // 定期在串口显示IP地址，方便用户查看
  unsigned long currentMillis = millis();
  if (currentMillis - lastIPDisplayTime >= IP_DISPLAY_INTERVAL) {
    lastIPDisplayTime = currentMillis;
    Serial.println("\n----------------------------------");
    Serial.println("Web控制面板地址: http://" + ipAddress);
    Serial.println("----------------------------------\n");
  }
  
  // 读取光敏电阻的值
  int lightValue = analogRead(LIGHT_SENSOR_PIN);
  lightValueStr = String(lightValue);
  
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

  // 读取按键状态
  val_botton = digitalRead(bottonpin);
  
  // 按键状态变化检测（下降沿检测 - 从未按下到按下）
  if(val_botton == LOW && prev_botton_state == HIGH) {
    button_pressed = true;
    delay(50); // 简单的消抖延时
  }
  
  // 更新上一次按键状态
  prev_botton_state = val_botton;
  
  // 如果按键被按下，改变舵机位置
  if(button_pressed) {
    button_pressed = false; // 重置按键标志
    
    // 根据当前状态决定下一个位置
    servo_position = (servo_position + 1) % 3;
    
    switch(servo_position) {
      case 0: // 回到中间位置90度
        Serial.println("舵机转到90度");
        myServo.write(90);
        delay(200);
        break;
      case 1: // 转到0度
        Serial.println("舵机转到0度");
        myServo.write(0);
        delay(200);
        break;
      case 2: // 转到180度
        Serial.println("舵机转到180度");
        myServo.write(180);
        delay(200);
        break;
    }
  }
  
  val = digitalRead(firepin);
  if (val == HIGH) {  // 检测到火焰
    Serial.println("检测到火焰！");
    digitalWrite(speakerPin, HIGH);  delay(2000);// 触发蜂鸣器
    fireStatusStr = "检测到火焰！";
  } else {
    Serial.println("无火焰");
    digitalWrite(speakerPin, LOW);
    fireStatusStr = "无火焰";
  }

  // 读取来自树莓派的数据
  if (RPI_SERIAL.available() > 0) {
    String data = RPI_SERIAL.readStringUntil('\n');
    data.trim(); // 移除空白字符
    
    Serial.print("收到树莓派数据：");
    Serial.println(data);
    
    // 解析垃圾类型数据
    if (data.length() == 2) {
      // 假设树莓派发送的是两位代码
      if (data == "00" || data == "h" || data == "H") {
        currentWasteType = WASTE_TYPE_HARMFUL;
        // 有害垃圾警报 - 蜂鸣器发出警报声
        Serial.println("警告：检测到有害垃圾！");
        for (int i = 0; i < 3; i++) {  // 发出三声短促警报
          digitalWrite(speakerPin, HIGH);
          delay(200);
          digitalWrite(speakerPin, LOW);
          delay(100);
        }
      } else if (data == "01" || data == "k" || data == "K") {
        currentWasteType = WASTE_TYPE_KITCHEN;
      } else if (data == "10" || data == "o" || data == "O") {
        currentWasteType = WASTE_TYPE_OTHER;
      } else if (data == "11" || data == "r" || data == "R") {
        currentWasteType = WASTE_TYPE_RECYCLING;
      } else {
        currentWasteType = -1; // 未识别的垃圾类型
      }
      
      // 更新垃圾类型字符串
      wasteTypeStr = getWasteTypeString(currentWasteType);
      Serial.print("当前垃圾类型: ");
      Serial.println(wasteTypeStr);
      
      // 根据垃圾类型控制舵机旋转
      if(currentWasteType != -1) {
        rotateServoByWasteType(currentWasteType);
      }
      
      // 上报数据到华为云
      reportWasteTypeToCloud(currentWasteType, wasteTypeStr);
    }
  }
  
  // 定期更新和上报垃圾类型数据
  currentMillis = millis();
  if (currentMillis - lastTrashDataTime >= TRASH_DATA_INTERVAL && currentWasteType != -1) {
    lastTrashDataTime = currentMillis;
    reportWasteTypeToCloud(currentWasteType, wasteTypeStr);
  }

  // 延时一段时间再进行下一次检测
  delay(300);
}
