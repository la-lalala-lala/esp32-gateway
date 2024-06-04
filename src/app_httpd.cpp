#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <AsyncMqttClient.h>

// 通用的esp32后台管理

// mqttd终端
extern AsyncMqttClient mqttClient;

// 作为客户端连接WiFi路由器的默认设置
extern String sta_ssid;
extern String sta_password;

//作为本地开启热点后的默认连接配置
extern String ap_ssid;
extern String ap_password;

//是否需要连接路由器（默认不连接）
extern bool router_connect;
// 是否已经连接上路由器
extern bool connect_router_flag;
//是否需要连接mqtt（默认不连接）
extern bool mqtt_connect;
// 是否已经连接上MQTT服务器
extern bool connect_mqtt_flag;

// 利用板载上提供的2号引脚作为指示灯，启动时，常量；启动完毕，熄灭
extern int run_status_pin;
// 中断引脚（拿金属物体单独接触23号引脚）
extern int interrupt_pin;

//作为登录本系统后的的默认配置，当前限制用户名不能更改，不允许动态添加用户，只能修改密码
String web_user = "shmily";
extern String web_password;

// mqtt相关的配置
extern String MQTT_HOST;
extern int MQTT_PORT;


extern Preferences prefs;

/**
 * 根据文件后缀获取html协议的返回内容类型
 */
String getContentType(String fileName,AsyncWebServerRequest * request){
  if(request->hasParam("download")){
    return "application/octet-stream";
  }else if(fileName.endsWith(".htm")){
    return "text/html";
  }else if(fileName.endsWith(".html")){
    return "text/html";
  }else if(fileName.endsWith(".css")){
    return "text/css";
  }else if(fileName.endsWith(".js")){
    return "text/javascript";
  }else if(fileName.endsWith(".png")){
    return "image/png";
  }else if(fileName.endsWith(".gif")){
    return "image/gif";
  }else if(fileName.endsWith(".jpg")){
    return "image/jpeg";
  }else if(fileName.endsWith(".ico")){
    return "image/x-icon";
  }else if(fileName.endsWith(".xml")){
    return "text/xml";
  }else if(fileName.endsWith(".pdf")){
    return "application/x-pdf";
  }else if(fileName.endsWith(".zip")){
    return "application/x-zip";
  }else if(fileName.endsWith(".gz")){
    return "application/x-gzip";
  }else{
    return "text/plain";
  }
}

/* NotFound处理 
 * 用于处理没有注册的请求地址 
 * 一般是处理一些页面请求 
 */
void handleNotFound(AsyncWebServerRequest * request){
  String path = request->url();
  Serial.print("load url:");
  Serial.println(path);
  String contentType = getContentType(path,request);
  String pathWithGz = path + ".gz";
  if(SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)){
    if(SPIFFS.exists(pathWithGz)){
      path += ".gz";
    }
    //Send index.htm as text
    request->send(SPIFFS, path, contentType);
    return;
  }
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += request->url();
  message += "\nMethod: ";
  message += (request->method() == HTTP_GET ) ? "GET" : "POST";
  request->send(404, "text/plain", message);
}

/**
 * 请求打开网关主页（登录页面）
 */
void handleHomePage(AsyncWebServerRequest * request) {
  //Send index.htm as text
  request->send(SPIFFS, "/index.htm", "text/plain");
  return;
}

/**
 * 登录接口
 */
void handLogin(AsyncWebServerRequest * request){
  AsyncWebServerResponse *response;
  if(request->hasArg("name") && request->hasArg("password")) {
    String name = request->arg("name");
    String password = request->arg("password");
    if(web_user == name && web_password == password){
      //authenticate = true;
      response = request->beginResponse(200, "text/json", "{\"code\":0,\"msg\":\"登录成功\"}");
    }else{
      response = request->beginResponse(200, "text/json", "{\"code\":-1,\"msg\":\"用户名或密码错误\"}");
    }
  }else{
      response = request->beginResponse(200, "text/json", "{\"code\":-2,\"msg\":\"缺少参数\"}");
  }
  response->addHeader("Access-Control-Allow-Origin", "*");
  request->send(response);
}

/**
 * 获取主页信息
 */
void handGetBoard(AsyncWebServerRequest * request){
  StaticJsonDocument<256> doc;
  JsonObject result = doc.to<JsonObject>();
  result["code"] = 0;
  result["msg"] = "成功";
  JsonObject _data = result.createNestedObject("data");
  _data["connect_mqtt_flag"] = connect_mqtt_flag;
  _data["connect_router_flag"] = connect_router_flag;
  _data["getAutoReconnect"] = WiFi.getAutoReconnect();
  _data["getMode"] = WiFi.getMode();
  _data["softAPgetStationNum"] = WiFi.softAPgetStationNum();
  _data["status"] = WiFi.status();
  _data["getStatusBits"] = WiFi.getStatusBits();
  _data["SSID"] = WiFi.SSID();
  _data["sta_ssid"] = sta_ssid;
  _data["ap_ssid"] = ap_ssid;
  String resultStr;
  serializeJson(result, resultStr);
  result.clear();
  AsyncWebServerResponse *response = request->beginResponse(200, "text/json", resultStr);
  response->addHeader("Access-Control-Allow-Origin", "*");
  request->send(response);
}

/**
 * 设置连接WIFI路由器的配置
 */
void handSetSta(AsyncWebServerRequest * request){
  AsyncWebServerResponse *response;
  if(request->hasArg("ssid") && request->hasArg("password") && request->hasArg("flag")) {
    sta_ssid = request->arg("ssid");
    sta_password = request->arg("password");
    // 准备读取系统配置
    prefs.begin("sysconfig");
    prefs.putString("sta_ssid", sta_ssid);
    prefs.putString("sta_password", sta_password);
    String _flag = request->arg("flag");
    router_connect = (_flag.toInt()==0)?false:true;
    prefs.putBool("router_connect", router_connect);
    // 关闭当前命名空间
    prefs.end();
    response = request->beginResponse(200, "text/json", "{\"code\":0,\"msg\":\"设置成功\"}");
  }else{
    response = request->beginResponse(200, "text/json", "{\"code\":-1,\"msg\":\"缺少参数\"}");
  }
  response->addHeader("Access-Control-Allow-Origin", "*");
  request->send(response);
}

/**
 * 设置无线热点的配置
 */
void handSetAp(AsyncWebServerRequest * request){
  AsyncWebServerResponse *response;
  if(request->hasArg("ssid") && request->hasArg("password")) {
    ap_ssid = request->arg("ssid");
    ap_password = request->arg("password");
    // 准备读取系统配置
    prefs.begin("sysconfig");
    prefs.putString("ap_ssid", ap_ssid);
    prefs.putString("ap_password", ap_password);
    // 关闭当前命名空间
    prefs.end();
    response = request->beginResponse(200, "text/json", "{\"code\":0,\"msg\":\"设置成功\"}");
  }else{
    response = request->beginResponse(200, "text/json", "{\"code\":-1,\"msg\":\"缺少参数\"}");
  }
  response->addHeader("Access-Control-Allow-Origin", "*");
  request->send(response);
}

/**
 * 设置用户密码
 */
void handSetUserPassword(AsyncWebServerRequest * request){
  AsyncWebServerResponse *response;
  if(request->hasArg("password")) {
    web_password = request->arg("password");
    // 准备读取系统配置
    prefs.begin("sysconfig");
    prefs.putString("web_password", web_password);
    // 关闭当前命名空间
    prefs.end();
    response = request->beginResponse(200, "text/json", "{\"code\":0,\"msg\":\"设置成功\"}");
  }else{
    response = request->beginResponse(200, "text/json", "{\"code\":-1,\"msg\":\"缺少参数\"}");
  }
  response->addHeader("Access-Control-Allow-Origin", "*");
  request->send(response);
}


/**
 * 获取sta设置信息
 */
void handGetSta(AsyncWebServerRequest * request){
  StaticJsonDocument<512> doc;
  JsonObject result = doc.to<JsonObject>();
  result["code"] = 0;
  result["msg"] = "成功";
  JsonObject _data = result.createNestedObject("data");
  _data["localIP"] = WiFi.localIP();
  _data["localIPv6"] = WiFi.localIPv6();
  _data["gatewayIP"] = WiFi.gatewayIP();
  _data["dnsIP"] = WiFi.dnsIP();
  _data["macAddress"] = WiFi.macAddress();
  _data["networkID"] = WiFi.networkID();
  _data["connect_router_flag"] = connect_router_flag;
  _data["getHostname"] = WiFi.getHostname();
  _data["sta_ssid"] = sta_ssid;
  _data["sta_password"] = sta_password;
  _data["router_connect"] = router_connect;
  String resultStr;
  serializeJson(result, resultStr);
  doc.clear();
  AsyncWebServerResponse *response = request->beginResponse(200, "text/json", resultStr);
  response->addHeader("Access-Control-Allow-Origin", "*");
  request->send(response);
}

/**
 * 获取ap设置
 */
void handGetAp(AsyncWebServerRequest * request){
  StaticJsonDocument<512> doc;
  JsonObject result = doc.to<JsonObject>();
  result["code"] = 0;
  result["msg"] = "成功";
  JsonObject _data = result.createNestedObject("data");
  _data["softAPgetHostname"] = WiFi.softAPgetHostname();
  _data["softAPIP"] = WiFi.softAPIP();
  _data["SSID"] = WiFi.SSID();
  _data["softAPIPv6"] = WiFi.softAPIPv6();
  _data["softAPBroadcastIP"] = WiFi.softAPBroadcastIP();
  _data["softAPmacAddress"] = WiFi.softAPmacAddress();
  _data["softAPNetworkID"] = WiFi.softAPNetworkID();
  _data["softAPgetStationNum"] = WiFi.softAPgetStationNum();
  _data["ap_ssid"] = ap_ssid;
  _data["ap_password"] = ap_password;
  result["code"] = 0;
  result["msg"] = "成功";
  String resultStr;
  serializeJson(result, resultStr);
  // 清空
  result.clear();
  AsyncWebServerResponse *response = request->beginResponse(200, "text/json", resultStr);
  response->addHeader("Access-Control-Allow-Origin", "*");
  request->send(response);
}

/**
 * 获取mqtt参数配置
 * 包括服务器ip，端口，当前连接状态，是否需要连接
 */
void handGetMqtt(AsyncWebServerRequest * request){
  AsyncWebServerResponse *response = request->beginResponse(200, "text/json", "{\"code\":0,\"msg\":\"成功\",\"data\":{\"host\":\""+MQTT_HOST+"\",\"port\":\""+MQTT_PORT+"\",\"status\":\""+connect_mqtt_flag+"\",\"flag\":\""+mqtt_connect+"\"}}");
  response->addHeader("Access-Control-Allow-Origin", "*");
  request->send(response);
}

/**
 * 设置mqtt参数
 */
void handSetMqtt(AsyncWebServerRequest * request){
  AsyncWebServerResponse *response;
  if(request->hasArg("host") && request->hasArg("port") && request->hasArg("flag")) {
    MQTT_HOST = request->arg("host");
    MQTT_PORT = request->arg("port").toInt();
    String _flag = request->arg("flag");
    mqtt_connect = (_flag.toInt()==0)?false:true;
    // 准备读取系统配置
    prefs.begin("sysconfig");
    prefs.putBool("mqtt_connect",mqtt_connect);
    prefs.putString("mqtt_host", MQTT_HOST);
    prefs.putInt("mqtt_port", MQTT_PORT);
    // 关闭当前命名空间
    prefs.end();
    response = request->beginResponse(200, "text/json", "{\"code\":0,\"msg\":\"设置成功\"}");
  }else{
    response = request->beginResponse(200, "text/json", "{\"code\":-1,\"msg\":\"缺少参数\"}");
  }
  response->addHeader("Access-Control-Allow-Origin", "*");
  request->send(response);
}

/**
 * 系统重启
 */
void handRestart(AsyncWebServerRequest * request){
  AsyncWebServerResponse *response = request->beginResponse(200, "text/json", "{\"code\":0,\"msg\":\"正在重启\"}");
  request->send(response);
  //复位esp32
  ESP.restart();
}

// 当您将消息发布到MQTT主题时， onMqttPublish（）函数被调用。它在串行监视器中打印数据包ID
void onMqttPublish(uint16_t packetId) {
  Serial.print("Publish acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}

// 控制led
void ledCtrl(int pin,bool status){
  if(status){
    digitalWrite(pin,HIGH);
  }else{
    digitalWrite(pin,LOW);
  }
}


// 回调监听mqtt
void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total){
  Serial.print("Message arrived [");
  // 打印主题
  Serial.print(topic);
  Serial.print("] ");
  Serial.printf("Message: %s \n", payload);
  Serial.println();
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    doc.clear();
    return;
  }
  // 获取功能模式
  String model = doc["model"].as<String>();
  if(NULL != model && 0 == model.compareTo("led")){
    bool status = doc["status"].as<bool>();
    // {"model":"led","status":true}
    // 收到数据时的指示灯
    // ledCtrl(test_status_pin,status);
  }
  doc.clear();
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    if (strcmp((char*)data, "toggle") == 0) {
      //ws.textAll("你的消息");
    }
  }
}

void onSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      // 连接
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      // 断开连接
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      // 接收数据
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

//void configWebSocket() {
//  ws.onEvent(onSocketEvent);
//  server.addHandler(&ws);
//}