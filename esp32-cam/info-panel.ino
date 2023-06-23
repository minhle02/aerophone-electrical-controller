/*
 * Reference from:
 * https://github.com/RuiSantosdotme/ESP32-CAM-Arduino-IDE/tree/master/ESP32-CAM-Take-Photo-and-Display-Web-Server
 * https://github.com/RuiSantosdotme/ESP32-CAM-Arduino-IDE/blob/master/ESP32-CAM-Video-Streaming/ESP32-CAM-Access-Point-AP-Video-Streaming.ino
 * https://raw.githubusercontent.com/RuiSantosdotme/Random-Nerd-Tutorials/master/Projects/ESP32/ESP32_Server_Sent_Events.ino
 * https://forum.arduino.cc/t/esp32-cam-with-error-in-monitor/921166/21
*/

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include "esp_camera.h"
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "soc/soc.h"           // Disable brownour problems
#include "soc/rtc_cntl_reg.h"  // Disable brownour problems
#include "driver/rtc_io.h"
#include <StringArray.h>
#include <SPIFFS.h>
#include <FS.h>

// Replace with your network credentials
const char* ssid = "ESP32 send data";
const char* password = "123456789";

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Create an Event Source on /events
AsyncEventSource events("/events");

//Camera
boolean takeNewPhoto = false;

// Photo File Name to save in SPIFFS
#define FILE_PHOTO "/photo.jpg"

// OV2640 camera module pins (CAMERA_MODEL_AI_THINKER)
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// Timer variables
unsigned long lastTime = 0;  
unsigned long timerDelay = 3000;

char* notification;
const int BATTERY_ERROR_PIN = 12;
const int ALTITUDE_ERROR_PIN = 2;

// Initialize WiFi
void initWiFi() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, password);
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);
}

String processor(const String& var){
  if(var == "NOTIFICATION"){
    return String(notification);
  }
  return String();
}

boolean getNotification(int bat_pin, int alt_pin) {
  notification = "Hello User, enjoy wonderful pics";
  if (digitalRead(bat_pin) == HIGH) {
    notification = "Battery Error";
    return false;
  } else if (digitalRead(alt_pin) == HIGH) {
    notification = "Altitude Error";
    return false;
  }
  return true;
}

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Foldable Drone Information Panel</title>
</head>
<body>
    <div id="container">
        <h2>Photo Panel</h2>
        <p>Note: It might take more than 5 seconds to capture a photo</p>
        <p>
          <button onclick="rotatePhoto();">ROTATE</button>
          <button onclick="capturePhoto()">CAPTURE PHOTO</button>
          <button onclick="location.reload();">REFRESH PAGE</button>
        </p>
    </div>

    <div class = "taken-photo">
          <img src="saved-photo" id="photo" width="70%">
    </div>

    <div id="content">
        <div class = "card">
            <p>
                <span class = "messaging">
                    <span id = "noti"></span>
                </span>
            </p>
        </div>
    </div>
</body>
<script>
    var deg = 0;
    function capturePhoto() {
        var xhr = new XMLHttpRequest();
        xhr.open('GET', "/capture", true);
        xhr.send();
    }
    function rotatePhoto() {
        var img = document.getElementById("photo");
        deg += 90;
        img.style.transform = "rotate(" + deg + "deg)";
    }
    if (!!window.EventSource) {
        var source = new EventSource('/events');
        
        source.addEventListener('open', function(e) {
        console.log("Events Connected");
        }, false);
        source.addEventListener('error', function(e) {
        if (e.target.readyState != EventSource.OPEN) {
            console.log("Events Disconnected");
        }
        }, false);
        
        source.addEventListener('message', function(e) {
        console.log("message", e.data);
        }, false);

        source.addEventListener('notification', function(e) {
            console.log("notification", e.data);
            document.getElementById("noti").innerHTML = e.data;
        }, false);
    }
</script>
</html>)rawliteral";

void initCamera() {

  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    ESP.restart();
  }
  else {
    delay(500);
    Serial.println("SPIFFS mounted successfully");
  }
  
  // Turn-off the 'brownout detector'
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  digitalWrite(PWDN_GPIO_NUM, LOW);
  delay(10);
  digitalWrite(PWDN_GPIO_NUM, HIGH);
  delay(10);

  // OV2640 camera module
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    ESP.restart();
  }
  Serial.println("Camera init ok");
}

void setup() {
  Serial.begin(115200);
  initWiFi();
  initCamera();


  // Handle Web Server
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });

  server.on("/capture", HTTP_GET, [](AsyncWebServerRequest * request) {
    takeNewPhoto = true;
    request->send_P(200, "text/plain", "Taking Photo");
  });

  server.on("/saved-photo", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, FILE_PHOTO, "image/jpg", false);
  });

  // Handle Web Server Events
  events.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    // send event with message "hello!", id current millis
    // and set reconnect delay to 1 second
    client->send("hello!", NULL, millis(), 10000);
  });
  server.addHandler(&events);
  server.begin();
  pinMode(BATTERY_ERROR_PIN,INPUT);
  pinMode(ALTITUDE_ERROR_PIN,INPUT);
}

void loop() {
  if (takeNewPhoto) {
    capturePhotoSaveSpiffs();
    takeNewPhoto = false;
  }
  delay(1);
  if ((millis() - lastTime) > timerDelay) {
    Serial.println("Message sent");
    if (!getNotification(BATTERY_ERROR_PIN, ALTITUDE_ERROR_PIN)) {
      events.send("ping",NULL,millis());
      events.send(String(notification).c_str(),"notification",millis());
      delay(2000);
      exit(0);
    }

    // Send Events to the Web Server with the Sensor Readings
    events.send("ping",NULL,millis());
    events.send(String(notification).c_str(),"notification",millis());
    
    
    lastTime = millis();
  }
}

// Check if photo capture was successful
bool checkPhoto( fs::FS &fs ) {
  File f_pic = fs.open( FILE_PHOTO );
  unsigned int pic_sz = f_pic.size();
  return ( pic_sz > 100 );
}

// Capture Photo and Save it to SPIFFS
void capturePhotoSaveSpiffs( void ) {
  camera_fb_t * fb = NULL; // pointer
  bool ok = 0; // Boolean indicating if the picture has been taken correctly

  do {
    // Take a photo with the camera
    Serial.println("Taking a photo...");

    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      return;
    }

    // Photo file name
    Serial.printf("Picture file name: %s\n", FILE_PHOTO);
    File file = SPIFFS.open(FILE_PHOTO, FILE_WRITE);

    // Insert the data in the photo file
    if (!file) {
      Serial.println("Failed to open file in writing mode");
    }
    else {
      file.write(fb->buf, fb->len); // payload (image), payload length
      Serial.print("The picture has been saved in ");
      Serial.print(FILE_PHOTO);
      Serial.print(" - Size: ");
      Serial.print(file.size());
      Serial.println(" bytes");
    }
    // Close the file
    file.close();
    esp_camera_fb_return(fb);

    // check if file has been correctly saved in SPIFFS
    ok = checkPhoto(SPIFFS);
  } while ( !ok );
}
