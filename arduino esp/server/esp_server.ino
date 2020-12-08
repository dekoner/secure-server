#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>

#define DISCORD_SND false

#ifndef STASSID
#define STASSID "vlad plohoy 4elovek"
#define STAPSK  "olegoleg"
#endif

#define SECRET_WEBHOOK "https://discordapp.com/api/webhooks/785251307124031498/RfPd5mQJrt3qrJ5u_Jmb1eEOjjm070mGUTPXRC6Qu96wsNNAxqlveLJvQ1BLKN9-FSID"

#define DBG_OUTPUT_PORT Serial

FS* filesystem = &LittleFS;

const char* ssid = STASSID;
const char* password = STAPSK;

const String webhook = SECRET_WEBHOOK;
const int httpsPort = 443;

ESP8266WebServer server(80);
File fsUploadFile;
//WiFiClient client;


String sens[10][7];
bool secureState = false;
bool alarm = false;

// Fingerprint for demo URL, expires on June 2, 2021, needs to be updated well before this date
const uint8_t fingerprint[20] = {0x03, 0xd3, 0xfe, 0xa6, 0x26, 0x78, 0x69, 0xd1, 0x03, 0xdf, 0x7f, 0x54, 0x38, 0xd0, 0x7e, 0x8a, 0x89, 0x6d, 0xb9, 0x67};


void discord_send(String content) {
  if (!DISCORD_SND) return;
  //std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);

  //client->setFingerprint(fingerprint);
  WiFiClientSecure client;
  client.setFingerprint(fingerprint);

  HTTPClient http;
  DBG_OUTPUT_PORT.print("[HTTP] begin...\n");
  // configure traged server and url
  http.begin(client, webhook); //HTTP
  http.addHeader("Content-Type", "application/json");

  DBG_OUTPUT_PORT.print("[HTTP] POST...\n");
  // start connection and send HTTP header and body
  DBG_OUTPUT_PORT.println(webhook);
  String json = "{\"content\":\"" + content + "\"}";
  DBG_OUTPUT_PORT.println(json);
  int httpCode = http.POST(json);

  // httpCode will be negative on error
  if (httpCode > 0) {
    // HTTP header has been send and Server response header has been handled
    DBG_OUTPUT_PORT.printf("[HTTP] POST... code: %d\n", httpCode);

    // file found at server
    if (httpCode == HTTP_CODE_OK || httpCode == 400) {
      const String& payload = http.getString();
      DBG_OUTPUT_PORT.println("received payload:\n<<");
      DBG_OUTPUT_PORT.println(payload);
      DBG_OUTPUT_PORT.println(">>");
    }
  } else {
    DBG_OUTPUT_PORT.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();
}

void handleRoot() {
  server.send(200, "text/plain", "hello from esp8266!");
}

//format bytes
String formatBytes(size_t bytes) {
  if (bytes < 1024) {
    return String(bytes) + "B";
  } else if (bytes < (1024 * 1024)) {
    return String(bytes / 1024.0) + "KB";
  } else if (bytes < (1024 * 1024 * 1024)) {
    return String(bytes / 1024.0 / 1024.0) + "MB";
  } else {
    return String(bytes / 1024.0 / 1024.0 / 1024.0) + "GB";
  }
}

String getContentType(String filename) {
  if (server.hasArg("download")) {
    return "application/octet-stream";
  } else if (filename.endsWith(".htm")) {
    return "text/html";
  } else if (filename.endsWith(".html")) {
    return "text/html";
  } else if (filename.endsWith(".css")) {
    return "text/css";
  } else if (filename.endsWith(".js")) {
    return "application/javascript";
  } else if (filename.endsWith(".png")) {
    return "image/png";
  } else if (filename.endsWith(".gif")) {
    return "image/gif";
  } else if (filename.endsWith(".jpg")) {
    return "image/jpeg";
  } else if (filename.endsWith(".ico")) {
    return "image/x-icon";
  } else if (filename.endsWith(".xml")) {
    return "text/xml";
  } else if (filename.endsWith(".pdf")) {
    return "application/x-pdf";
  } else if (filename.endsWith(".zip")) {
    return "application/x-zip";
  } else if (filename.endsWith(".gz")) {
    return "application/x-gzip";
  }
  return "text/plain";
}

bool handleFileRead(String path) {
  DBG_OUTPUT_PORT.println("handleFileRead: " + path);
  if (path.endsWith("/")) {
    path += "index.htm";
  }
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if (filesystem->exists(pathWithGz) || filesystem->exists(path)) {
    if (filesystem->exists(pathWithGz)) {
      path += ".gz";
    }
    File file = filesystem->open(path, "r");
    server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

void handleFileUpload() {
  if (server.uri() != "/edit") {
    return;
  }
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (!filename.startsWith("/")) {
      filename = "/" + filename;
    }
    DBG_OUTPUT_PORT.print("handleFileUpload Name: "); DBG_OUTPUT_PORT.println(filename);
    fsUploadFile = filesystem->open(filename, "w");
    filename = String();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    //DBG_OUTPUT_PORT.print("handleFileUpload Data: "); DBG_OUTPUT_PORT.println(upload.currentSize);
    if (fsUploadFile) {
      fsUploadFile.write(upload.buf, upload.currentSize);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (fsUploadFile) {
      fsUploadFile.close();
    }
    DBG_OUTPUT_PORT.print("handleFileUpload Size: "); DBG_OUTPUT_PORT.println(upload.totalSize);
  }
}

void handleFileDelete() {
  if (server.args() == 0) {
    return server.send(500, "text/plain", "BAD ARGS");
  }
  String path = server.arg(0);
  DBG_OUTPUT_PORT.println("handleFileDelete: " + path);
  if (path == "/") {
    return server.send(500, "text/plain", "BAD PATH");
  }
  if (!filesystem->exists(path)) {
    return server.send(404, "text/plain", "FileNotFound");
  }
  filesystem->remove(path);
  server.send(200, "text/plain", "");
  path = String();
}

void handleFileCreate() {
  if (server.args() == 0) {
    return server.send(500, "text/plain", "BAD ARGS");
  }
  String path = server.arg(0);
  DBG_OUTPUT_PORT.println("handleFileCreate: " + path);
  if (path == "/") {
    return server.send(500, "text/plain", "BAD PATH");
  }
  if (filesystem->exists(path)) {
    return server.send(500, "text/plain", "FILE EXISTS");
  }
  File file = filesystem->open(path, "w");
  if (file) {
    file.close();
  } else {
    return server.send(500, "text/plain", "CREATE FAILED");
  }
  server.send(200, "text/plain", "");
  path = String();
}

void handleFileList() {
  if (!server.hasArg("dir")) {
    server.send(500, "text/plain", "BAD ARGS");
    return;
  }

  String path = server.arg("dir");
  DBG_OUTPUT_PORT.println("handleFileList: " + path);
  Dir dir = filesystem->openDir(path);
  path = String();

  String output = "[";
  while (dir.next()) {
    File entry = dir.openFile("r");
    if (output != "[") {
      output += ',';
    }
    bool isDir = false;
    output += "{\"type\":\"";
    output += (isDir) ? "dir" : "file";
    output += "\",\"name\":\"";
    if (entry.name()[0] == '/') {
      output += &(entry.name()[1]);
    } else {
      output += entry.name();
    }
    output += "\"}";
    entry.close();
  }

  output += "]";
  server.send(200, "text/json", output);
}

int findSensor(String id) {
  int i;
  int k = -1;
  for (i = 0; i < 10; i++) {
    if (sens[i][0] == id) {
      k = i;
      i = 10;
    }
  }
  return k;
}

int findEmpty() {
  int k = -1;
  for (int i = 0; i < 10; i++) {
    if (sens[i][0] == "") {
      k = i;
      i = 10;
    }
  }
  return k;
}

int genId() {
  bool fl = false;
  for (byte i = 1; i < 10; i++) {
    fl = false;
    for (byte j = 0; j < 10; j++) {
      String n = String(i);
      if (sens[j][0] == n) {
        fl = true;
      }
    }
    if (!fl) {
      return i;
    }
  }
  return -1;
}

void setup() {
  Serial.begin(115200);
  Serial.println("Booting");
  filesystem->begin();
  {
    Dir dir = filesystem->openDir("/");
    while (dir.next()) {
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      DBG_OUTPUT_PORT.printf("FS File: %s, size: %s\n", fileName.c_str(), formatBytes(fileSize).c_str());
    }
    DBG_OUTPUT_PORT.printf("\n");
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  // ArduinoOTA.setHostname("myesp8266");

  // No authentication by default
  ArduinoOTA.setPassword("____");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/list", HTTP_GET, handleFileList);
  //load editor
  server.on("/edit", HTTP_GET, []() {
    if (!handleFileRead("/edit.htm")) {
      server.send(404, "text/plain", "FileNotFound");
    }
  });
  //create file
  server.on("/edit", HTTP_PUT, handleFileCreate);
  //delete file
  server.on("/edit", HTTP_DELETE, handleFileDelete);
  //first callback is called after the request has ended with all parsed arguments
  //second callback handles file uploads at that location
  server.on("/edit", HTTP_POST, []() {
    server.send(200, "text/plain", "");
  }, handleFileUpload);

  //called when the url is not defined here
  //use it to load content from SPIFFS
  server.onNotFound([]() {
    if (!handleFileRead(server.uri())) {
      server.send(404, "text/plain", "FileNotFound");
    }
  });

  //get heap status, analog input value and all GPIO statuses in one json call
  server.on("/all", HTTP_GET, []() {
    String json = "{";
    json += "\"heap\":" + String(ESP.getFreeHeap());
    json += ", \"analog\":" + String(analogRead(A0));
    json += ", \"gpio\":" + String((uint32_t)(((GPI | GPO) & 0xFFFF) | ((GP16I & 0x01) << 16)));
    json += "}";
    server.send(200, "text/json", json);
    json = String();
  });

  server.on("/api/sensors", HTTP_GET, []() {
    if (server.argName(0) != "id") {
      server.send(500, "text/plain", "BAD ARGS");
    } else {
      int num = findSensor(server.arg(0));
      if (num < 0) {
        server.send(500, "text/plain", "Sensor not found");
      } else {
        String json = "{";
        json += "{\"id\":\"" + sens[num][0] + "\"";
        json += ", \"bat\":\"" + sens[num][1] + "\"";
        json += ", \"sensor\":\"" + sens[num][2] + "\"";
        json += ", \"type\":\"" + sens[num][3] + "\"";
        String buf_str;
        if(sens[num][5] == ""){
          buf_str = "___";
        } else {
          buf_str = sens[num][5];
        }
        json += ", \"name\":\"" + buf_str + "\"}";
        server.send(200, "text/json", json);
        json = String();
      }
    }
  });

  server.on("/api/all", HTTP_GET, []() {
    String json = "{\"sensors\" : [";
    bool fl = false;
    for (byte i = 0; i < 10; i++) {
      if (sens[i][0] != "" && sens[i][0] != "0") {
        if (fl) {
          json += ",";
        };
        json += "{\"id\":\"" + sens[i][0] + "\"";
        json += ", \"bat\":\"" + sens[i][1] + "\"";
        json += ", \"sensor\":\"" + sens[i][2] + "\"";
        json += ", \"type\":\"" + sens[i][3] + "\"";
        String buf_str;
        if(sens[i][5] == ""){
          buf_str = "___";
        } else {
          buf_str = sens[i][5];
        }
        json += ", \"name\":\"" + buf_str + "\"}";
        fl = true;
      }
    }
    json += "]}";
    server.send(200, "text/json", json);
    json = String();
  });

  server.on("/api/onsecure", HTTP_POST, []() {
    secureState = true;
    server.send(200, "text/plain", "secure ON");
    DBG_OUTPUT_PORT.println("secure ON");
    discord_send(String("Охранная система включена"));
  });

  server.on("/api/sendmessage", HTTP_POST, []() {
    discord_send(server.arg(0));
    server.send(200, "text/plain", "send");
    DBG_OUTPUT_PORT.println("message send");
  });

  server.on("/api/offsecure", HTTP_POST, []() {
    secureState = false;
    server.send(200, "text/plain", "secure OFF");
    DBG_OUTPUT_PORT.println("secure OFF");
    discord_send(String("Охранная система выключена"));
  });

  server.on("/api/securestate", HTTP_GET, []() {
    byte b = 0;
    if (secureState == true) {
      b = 1;
    }

    if (alarm) {
      b = 2;
    }

    String json = "{\"secureState\":" + String(b);
    json += "}";
    server.send(200, "text/json", json);
    json = String();
  });

  server.on("/api/sensname", HTTP_POST, []() {
    Serial.println("___");
    if (server.args() == 0) {
      server.send(500, "text/plain", "BAD ARGS");
    } else {
      String input = server.arg(0);
      DBG_OUTPUT_PORT.println(input);
      DynamicJsonDocument json(1024);
      DeserializationError err = deserializeJson(json, input);
      String id;
      String sname;
      if (!err) {
        Serial.println("\nparsed json");

        id = json["id"].as<String>();
        sname = json["name"].as<String>();
      } else {
        Serial.println("failed to load json config");
      }
      int num = findSensor(id);
      if (num >= 0) {
        sens[num][5] = sname;
        DBG_OUTPUT_PORT.print("Set sensor name updated. id: ");
        DBG_OUTPUT_PORT.print(sens[num][0]);
        DBG_OUTPUT_PORT.print(", name: ");
        DBG_OUTPUT_PORT.println(sens[num][5]);
        server.send(200, "text/plain", "OK");
      } else {
        DBG_OUTPUT_PORT.println("Sensor not found");
        server.send(500, "text/plain", "Sensor not found");
      }
    }
  });

  server.on("/api/sensors", HTTP_POST, []() {
    Serial.println("___");
    if (server.args() == 0) {
      server.send(500, "text/plain", "BAD ARGS");
    } else {
      String input = server.arg(0);
      DBG_OUTPUT_PORT.println(input);
      DynamicJsonDocument json(1024);
      DeserializationError err = deserializeJson(json, input);
      String id;
      String bat;
      String sensor;
      String type;
      bool nullId = false;
      if (!err) {
        Serial.println("\nparsed json");

        id = json["id"].as<String>();
        bat = json["bat"].as<String>();
        sensor = json["sensor"].as<String>();
        type = json["type"].as<String>();
      } else {
        Serial.println("failed to load json config");
      }
      if (id == "0") {
        id = String(genId());
        //id = "5";
        nullId = true;
        DBG_OUTPUT_PORT.println("Gen id");
      }
      int num = findSensor(id);
      if (num >= 0) {
        sens[num][1] = bat;
        sens[num][2] = sensor;
        sens[num][3] = type;
        DBG_OUTPUT_PORT.print("Sensor updated. bat: ");
        DBG_OUTPUT_PORT.print(sens[num][1]);
        DBG_OUTPUT_PORT.print(", sens: ");
        DBG_OUTPUT_PORT.println(sens[num][2]);
      } else {
        num = findEmpty();
        if (num != -1) {
          sens[num][0] = id;
          sens[num][1] = bat;
          sens[num][2] = sensor;
          sens[num][3] = type;
          DBG_OUTPUT_PORT.print("Sensor added:");
          DBG_OUTPUT_PORT.println(id);
          discord_send(String("Добавлен датчик. ID: ") + String(id));
        }
      }
      if (nullId) {
        server.send(200, "text/plain", "{\"id\":\"" + String(id) + "\"}");
      } else {
        server.send(200, "text/plain", "OK");
      }
    }
  });

  server.begin();
}

bool lastalarm = false;
void loop() {
  ArduinoOTA.handle();
  server.handleClient();
  if (secureState) {
    for (byte i = 0; i < 10; i++) {
      if (sens[i][0] != "" && sens[i][0] != "0") {
        if (sens[i][4] != "off" && sens[i][3] == "0") {
          bool sec = false;
          if (sens[i][2] == "1") {
            sec = true;
          }
          alarm = alarm || !sec;
        }
      }
    }
  } else {
    alarm = false;
  }
  if ((alarm != lastalarm) && alarm) {
    discord_send("В помещении посторонние!!!");
    DBG_OUTPUT_PORT.println("secure alarm");
  }
  lastalarm = alarm;
}
