
#include <WiFi.h>
#include <Preferences.h>
#include <Wire.h>


extern "C" {
  #include "freertos/FreeRTOS.h"
  #include "freertos/timers.h"
}
#include <AsyncMqttClient.h>


/**
 * ESPAsyncWebServer库
 * https://github.com/me-no-dev/ESPAsyncWebServer
 * https://blog.csdn.net/T_infinity/article/details/105447479
 * https://blog.csdn.net/qq_43454310/article/details/114824338
 */
#include <ESPAsyncWebServer.h>
/**
 * SPIFFS这个库不需要安装
 * 综合：https://blog.csdn.net/solar_Lan/article/details/74231360
 * arduino-esp32fs-plugin安装说明：https://github.com/me-no-dev/arduino-esp32fs-plugin
 * 请注意arduino-esp32fs-plugin只会烧录项目下的data目录，arduino文件需要单独烧录
 */
#include <SPIFFS.h>


/**
 * 使用millis()代替delay()
 * https://www.qutaojiao.com/21429.html
 * 嵌入式项目
 * https://randomnerdtutorials.com/
 */

AsyncWebServer server(80);
// AsyncWebSocket ws("/ws");
// https://www.codeleading.com/article/92785144869/
Preferences prefs;

// mqttd终端
AsyncMqttClient mqttClient;
// 定时器
TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;

// 作为客户端连接WiFi路由器的默认设置
String sta_ssid = "优锘科技";
String sta_password = "uinnova123";

//作为本地开启热点后的默认连接配置
String ap_ssid = "ESP32-GATEWAY Access Point";
String ap_password = "123406789";


//是否需要连接路由器（默认不连接）
bool router_connect = false;
// 是否已经连接上路由器
bool connect_router_flag = false;
//是否需要连接mqtt（默认不连接）
bool mqtt_connect = false;
// 是否已经连接上MQTT服务器
bool connect_mqtt_flag = false;

// 利用板载上提供的2号引脚作为指示灯，启动时，常量；启动完毕，熄灭
int run_status_pin = 2;
// 中断引脚（拿金属物体单独接触23号引脚）
int interrupt_pin = 23;

//作为登录本系统后的的默认配置，当前限制用户名不能更改，不允许动态添加用户，只能修改密码
String web_password = "shmily";

// mqtt相关的配置
String MQTT_HOST = "1.15.81.148";
int MQTT_PORT = 1883;

// 作为WiFi热点时的配置
IPAddress ap_ip(192,168,4,1);//AP端IP
IPAddress ap_gateway(192,168,4,1);//AP端网关
IPAddress ap_netmask(255,255,255,0);//AP端子网掩码


// mqtt相关的配置
// 网关编码
String CLIENT_ID = "esp32-001";
// 认证信息表的用户名
String CLIENT_USERNAME  = "iCaNt2344m4KZbZb";
// 认证信息用户表的密码
String CLIENT_PASSWORD = "123456";
extern String MQTT_HOST;
extern int MQTT_PORT;
String MQTT_PUB_TOPIC = "/iot/ack/"+CLIENT_ID;
String MQTT_SUB_TOPIC_TOPIC = "/iot/listener/"+CLIENT_ID;

// 因为app_httpd没有头文件，所以在使用它的时候，提前定义一下
String getContentType(String fileName,AsyncWebServerRequest * request);
void handleNotFound(AsyncWebServerRequest * request);
void handleHomePage(AsyncWebServerRequest * request);
void handLogin(AsyncWebServerRequest * request);
void handGetBoard(AsyncWebServerRequest * request);
void handSetSta(AsyncWebServerRequest * request);
void handSetAp(AsyncWebServerRequest * request);
void handSetUserPassword(AsyncWebServerRequest * request);
void handGetSta(AsyncWebServerRequest * request);
void handGetAp(AsyncWebServerRequest * request);
void handGetMqtt(AsyncWebServerRequest * request);
void handSetMqtt(AsyncWebServerRequest * request);
void handRestart(AsyncWebServerRequest * request);
void onMqttPublish(uint16_t packetId);
void ledCtrl(int pin,bool status);
void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len);
void onSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);


// 与MQTT服务器连接成功后，执行的方法
void onMqttConnect(bool sessionPresent) {
  connect_mqtt_flag = true;
  // 连接成功时订阅主题
  // QoS 0：“最多一次”，消息发布完全依赖底层 TCP/IP 网络。分发的消息可能丢失或重复。例如，这个等级可用于环境传感器数据，单次的数据丢失没关系，因为不久后还会有第二次发送。
  // QoS 1：“至少一次”，确保消息可以到达，但消息可能会重复。
  // QoS 2：“只有一次”，确保消息只到达一次。例如，这个等级可用在一个计费系统中，这里如果消息重复或丢失会导致不正确的收费。
  mqttClient.subscribe(MQTT_SUB_TOPIC_TOPIC.c_str(),2);
  Serial.println("Connected to MQTT.");
  Serial.print("Session present: ");
  Serial.println(sessionPresent);
}

// 将ESP32连接到MQTT服务器
void connectToMqtt() {
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}

// ESP32断开与MQTT连接，它将调用 onMqttDisconnect 在串行监视器中打印该消息的功能。
void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("Disconnected from MQTT.");
  connect_mqtt_flag = false;
  if (WiFi.isConnected()) {
    xTimerStart(mqttReconnectTimer, 0);
  }
}


// web服务器设置
void configWebServer(){
  // serving static content for GET requests on '/' from SPIFFS directory '/'
  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html").setCacheControl("max-age=86400");
  // 打开登录页面
  server.on ("/", handleHomePage);
  // 响应登录接口
  server.on("/api/login.action",HTTP_POST,handLogin);
  // 响应设置连接WIFI路由器的配置
  server.on("/api/sta.action",HTTP_POST,handSetSta);
  // 响应设置无线热点的配置
  server.on("/api/ap.action",HTTP_POST,handSetAp);
  // 响应用户修改密码
  server.on("/api/password.action",HTTP_POST,handSetUserPassword);
  // 响应系统重启
  server.on("/api/restart.action",HTTP_POST,handRestart);
  // 响应获取连接WIFI路由器的配置
  server.on("/api/sta.action",HTTP_GET,handGetSta);
  // 响应获取无线热点的配置
  server.on("/api/ap.action",HTTP_GET,handGetAp);
  // 获取mqtt参数
  server.on("/api/mqtt.action",HTTP_GET,handGetMqtt);
  // 设置mqtt参数
  server.on("/api/mqtt.action",HTTP_POST,handSetMqtt);
  // 获取主页数据
  server.on("/api/home.action",HTTP_GET,handGetBoard);
  server.onNotFound(handleNotFound);
  server.begin();
}

// 采用AP和STA混合模式
void configApAndSta() {
  Serial.println("Connecting to Wi-Fi...");
    // 断开连接（防止已连接）
  WiFi.disconnect();
  // 设置成AP和STA混合模式
  WiFi.mode(WIFI_AP_STA);
  // 设置AP网络参数
  WiFi.softAPConfig(ap_ip,ap_gateway,ap_netmask);
  // 设置AP账号密码
  WiFi.softAP(ap_ssid.c_str(),ap_password.c_str());
  Serial.print("begin connect ssid:");
  Serial.print(sta_ssid.c_str());
  Serial.print(",password:");
  Serial.println(sta_password.c_str());
  // 连接到指定路由器
  WiFi.begin(sta_ssid.c_str(),sta_password.c_str());
  // 设置本地网络参数
  Serial.println("please wait");
  while(WiFi.status()!=WL_CONNECTED){
    delay(1000);
    Serial.print("connect status:");
    Serial.println(WiFi.status());
  }
  // WiFi路由器的ip
  Serial.print("ip:");
  Serial.println(WiFi.localIP());
  // 板子的ip，也就是热点的ip
  Serial.print("apip：");
  Serial.print(WiFi.softAPIP());
  Serial.println();
}

// 采用AP模式
void configAp() {
  Serial.println("Turn On Wi-Fi...");
    // 断开连接（防止已连接）
  WiFi.disconnect();
  // 设置成A模式
  WiFi.mode(WIFI_AP);
  // 设置AP网络参数
  WiFi.softAPConfig(ap_ip,ap_gateway,ap_netmask);
  // 设置AP账号密码
  WiFi.softAP(ap_ssid.c_str(),ap_password.c_str());
  // 板子的ip，也就是热点的ip
  Serial.print("apip：");
  Serial.println(WiFi.softAPIP());
  Serial.print("mac：");
  Serial.println(WiFi.softAPmacAddress().c_str());
  Serial.println();
}



// 负责处理Wi-Fi事件。例如，在与路由器和MQTT代理成功连接后，它将打印ESP32 IP地址。另一方面，如果连接丢失，它将启动计时器并尝试重新连接
void WiFiEvent(WiFiEvent_t event) {
  Serial.printf("[WiFi-event] event: %d\n", event);
  switch(event) {
    case SYSTEM_EVENT_STA_GOT_IP:
      Serial.println("WiFi connected");
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());
      connectToMqtt();
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      Serial.println("WiFi lost connection");
      xTimerStop(mqttReconnectTimer, 0); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
      xTimerStart(wifiReconnectTimer, 0);
      break;
  }
}

void IRAM_ATTR interrupt() {
  ledCtrl(run_status_pin,true);
  // 中断里面不要加串口打印，有阻塞的。Interrupt wdt timeout on
  //Serial.printf("esp32 interrupt\n");
  ap_ssid = "ESP32-GATEWAY Access Point";
  ap_password = "123406789";
  web_password = "shmily";
  router_connect = false;
  // 准备重置系统配置
  prefs.begin("sysconfig");
  prefs.putString("ap_ssid", ap_ssid);
  prefs.putString("ap_password",ap_password);
  prefs.putString("web_password", web_password);
  prefs.putBool("router_connect",router_connect);
  // 关闭当前命名空间
  prefs.end();
  ledCtrl(run_status_pin,false);
}


void setup(){
  Serial.begin(115200);
  SPIFFS.begin(true);
  // 点亮指示灯
  pinMode(run_status_pin,OUTPUT);
  ledCtrl(run_status_pin,true);
  // 准备读取系统配置
  prefs.begin("sysconfig");
  ap_ssid = prefs.getString("ap_ssid", ap_ssid);
  ap_password = prefs.getString("ap_password", ap_password);
  sta_ssid = prefs.getString("sta_ssid", sta_ssid);
  sta_password = prefs.getString("sta_password", sta_password);
  web_password = prefs.getString("web_password", web_password);
  router_connect = prefs.getBool("router_connect",router_connect);
  mqtt_connect = prefs.getBool("mqtt_connect",mqtt_connect);
  MQTT_HOST =  prefs.getString("mqtt_host", MQTT_HOST);
  MQTT_PORT =  prefs.getInt("mqtt_port", MQTT_PORT);
  Serial.print("router_connect:");
  Serial.println(router_connect);
  Serial.print("ap_password:");
  Serial.println(ap_password);

  // 设置中断
  pinMode(interrupt_pin, INPUT_PULLUP);
  attachInterrupt(interrupt_pin, interrupt, FALLING);

  // 关闭当前命名空间
  prefs.end();
  if(router_connect){
    // 混合模式
    wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(configApAndSta));
    if(mqtt_connect){
      // 创建计时器，如果连接断开，该计时器将允许MQTT代理和Wi-Fi连接重新连接。
      mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
      // 分配了一个回调函数，因此当ESP32连接到您的Wi-Fi时，它将执行 WiFiEvent（）
      WiFi.onEvent(WiFiEvent);
      mqttClient.onConnect(onMqttConnect);
      mqttClient.onDisconnect(onMqttDisconnect);
      mqttClient.onPublish(onMqttPublish);
      mqttClient.onMessage(onMqttMessage);
      // 设置连接信息，包括MQTT服务器地址、端口、设备的ID、设备用户名及密码
      mqttClient.setServer(MQTT_HOST.c_str(), MQTT_PORT);
      mqttClient.setClientId(CLIENT_ID.c_str());
      mqttClient.setCredentials(CLIENT_USERNAME.c_str(), CLIENT_PASSWORD.c_str());
    }
    // 网络参数配置
    configApAndSta();
  }else{
    configAp();
  }
  // 设置web服务器
  configWebServer();
  // 设置socket服务器
  //configWebSocket();
  Serial.println("http server started");
}

void loop(){
  if(router_connect && WiFi.status()!=WL_CONNECTED){
    // 需要连接路由器 但没有连上
    ledCtrl(run_status_pin,true);
    connect_router_flag = false;
  }else{
    ledCtrl(run_status_pin,false);
    connect_router_flag = true;
  }
  // 关闭过多的websocket，以便节省资源
  //ws.cleanupClients();
}