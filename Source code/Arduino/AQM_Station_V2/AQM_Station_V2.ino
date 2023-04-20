//----------------------------------------------------------------------------- 
// Project          : AQM_Station_V1 
// VSCode Extension : PlatformIO IDE 1.10.0 
// Source           : https://github.com/meanakorn/AQM_Station_V1.git 
// Board            : Node32s (Gravitech Node32Lite LamLoei) 
// Additional URLs  : https://dl.espressif.com/dl/package_esp32_index.json 
// LED_BUILTIN      : Pin 2 

//                  : Adafruit GFX Library by Adafruit (1.11.3) 
//                  : Adafruit SSD1306 by Adafruit (2.5.7) 
//                  : Firebase ESP32 Client by Mobizt (4.3.0) 
//                  : https://github.com/mobizt/Firebase-ESP32 

//----------------------------------------------------------------------------- 
#include <Arduino.h> 



#include <Wire.h> 
#include <SPI.h> 
#include <Adafruit_GFX.h> 
#include <Adafruit_SSD1306.h> 

//----------------------------------------------------------------------------- 
#if defined(ESP32) || defined(ARDUINO_RASPBERRY_PI_PICO_W)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif

#include <Firebase_ESP_Client.h>

#include <addons/TokenHelper.h>

#define WIFI_SSID            " " 
#define WIFI_PASSWORD        " " 

#define API_KEY              " " 
#define FIREBASE_PROJECT_ID  " " 
#define USER_EMAIL           " " 
#define USER_PASSWORD        " " 

FirebaseData fbdo;

FirebaseAuth auth;
FirebaseConfig config;

bool taskcomplete = false;

#if defined(ARDUINO_RASPBERRY_PI_PICO_W)
WiFiMulti multi;
#endif

//----------------------------------------------------------------------------- 
String documentPath   = "station/0"; 
String updateTime_str = ""; 

#define PIN_RX_0            3 
#define PIN_TX_0            1 
#define PIN_RX_1            16 
#define PIN_TX_1            17 

char str_buff[200] = {0}; 
unsigned long t_old = 0; 
int tmr_cnt = 0; 

int status_process        = 0; 
int status_process_mon    = 0; 
int error = 0; 

unsigned int dix, dix_max = 0; 
unsigned int dlen, dtype  = 0; 
unsigned char din; 
unsigned char dbuff[120]  = {0}; 

unsigned int pm_1         = 0; 
unsigned int pm_25        = 0; 
unsigned int pm_10        = 0; 

//----------------------------------------------------------------------------- 
#define PIN_ID_A2           32 
#define PIN_ID_A1           33 
#define PIN_ID_A0           25 

unsigned int station_id   = 0; 

//----------------------------------------------------------------------------- 
#define SCREEN_WIDTH 128 
#define SCREEN_HEIGHT 64 

#define OLED_RESET     4 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET); 

#define NUMFLAKES     10 

#define LOGO_HEIGHT   16
#define LOGO_WIDTH    16

//----------------------------------------------------------------------------- 
#include <WiFiUdp.h> 

unsigned int udp_localPort = 1000; 

IPAddress timeServerIP;                           // time.nist.gov NTP server address 
const char* ntpServerName = "time.nist.gov"; 
const int NTP_PACKET_SIZE = 48;                   // NTP time stamp is in the first 48 bytes of the message 
byte packetBuffer[NTP_PACKET_SIZE];               // buffer to hold incoming and outgoing packets 

WiFiUDP udp; 
int ntp_Y, ntp_M, ntp_D, ntp_H, ntp_m, ntp_S, ntp_S_10; 

unsigned long sendNTPpacket(IPAddress& address); 
void NTP_Update(); 

//----------------------------------------------------------------------------- 
void Disp_Info(); 
void Update_Document(); 


//----------------------------------------------------------------------------- 
void setup() { 
  pinMode(PIN_ID_A2, INPUT); 
  pinMode(PIN_ID_A1, INPUT); 
  pinMode(PIN_ID_A0, INPUT); 

  Serial.begin (9600, SERIAL_8N1, PIN_RX_0, PIN_TX_0); 
  Serial1.begin(9600, SERIAL_8N1, PIN_RX_1, PIN_TX_1); 

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println(F("SSD1306 allocation failed")); 
    for(;;); // Don't proceed, loop forever
  } 

  Disp_Info(); delay(1000); 

//----------------------------------------------------------------------------- 
#if defined(ARDUINO_RASPBERRY_PI_PICO_W) 
  multi.addAP(WIFI_SSID, WIFI_PASSWORD); 
  multi.run(); 
#else 
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD); 
#endif 

  Serial.print("\nConnecting to Wi-Fi"); 
  while (WiFi.status() != WL_CONNECTED) { 
    Serial.print("."); 
    delay(200); 
  } 

  Serial.print("\nConnected with IP: "); 
  Serial.println(WiFi.localIP()); 

//----------------------------------------------------------------------------- 
  udp.begin(udp_localPort); 
  ntp_Y = 0; ntp_M = 0; ntp_D = 0; 
  ntp_H = 0; ntp_m = 0; ntp_S = 0; 
  ntp_S_10 = 0; 

//----------------------------------------------------------------------------- 
  station_id = (7 - ((digitalRead(PIN_ID_A2) * 4) + (digitalRead(PIN_ID_A1) * 2) + (digitalRead(PIN_ID_A0) * 1))); 

  switch (station_id) { 
    default: 
    case 0: documentPath = "station/0"; break; 
    case 1: documentPath = "station/1"; break; 
    case 2: documentPath = "station/2"; break; 
    case 3: documentPath = "station/3"; break; 
    case 4: documentPath = "station/4"; break; 
    case 5: documentPath = "station/5"; break; 
    case 6: documentPath = "station/6"; break; 
    case 7: documentPath = "station/7"; break; 
  } 

//----------------------------------------------------------------------------- 
  Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION); 

  config.api_key      = API_KEY; 
  auth.user.email     = USER_EMAIL; 
  auth.user.password  = USER_PASSWORD; 

#if defined(ARDUINO_RASPBERRY_PI_PICO_W) 
  config.wifi.clearAP(); 
  config.wifi.addAP(WIFI_SSID, WIFI_PASSWORD); 
#endif 

  config.token_status_callback = tokenStatusCallback; // see addons/TokenHelper.h

#if defined(ESP8266) 
  fbdo.setBSSLBufferSize(2048 /* Rx buffer size in bytes from 512 - 16384 */, 2048 /* Tx buffer size in bytes from 512 - 16384 */); 
#endif 

  fbdo.setResponseSize(2048);
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

//----------------------------------------------------------------------------- 
  pm_1  = 0; 
  pm_25 = 0; 
  pm_10 = 0; 

} 


void loop() { 

  while (Serial1.available() > 0) { 
    din = Serial1.read(); 

    if ((din == 0x42) || (din == 0x4D)) { 
      if (din == 0x42) { dix = 0; dix_max = 0; dlen = 0; } 
      if (din == 0x4D) { dix = 1; } 
    } else { 
      if (dix < 100) { dix++; } 
      if (dix_max < dix) { dix_max = dix; } 
    } 

    dbuff[dix] = din; 

    if (dix == 3) { 
      dlen  = ((0x10 * (unsigned int)dbuff[2]) + (unsigned int)dbuff[3]); 

      if ((dlen == 0x14) || (dlen == 0x1C)) { 
        if (dlen == 0x14) { dtype = 3003; } 
        if (dlen == 0x1C) { dtype = 7003; } 
        dlen  = dlen + 2 + 1; 
      } else { 
        dlen  = 0; 
        dtype = 0; 
        pm_1  = 0; 
        pm_25 = 0; 
        pm_10 = 0; 
      } 
    } 

    if ((dlen > 0) && (dix == dlen)) { 
      pm_1  = ((0x10 * (unsigned int)dbuff[4]) + (unsigned int)dbuff[5]); 
      pm_25 = ((0x10 * (unsigned int)dbuff[6]) + (unsigned int)dbuff[7]); 
      pm_10 = ((0x10 * (unsigned int)dbuff[8]) + (unsigned int)dbuff[9]); 
    } 
  } 

  if (tmr_cnt == 0) { 
    NTP_Update(); 

    display.clearDisplay(); 

    display.setTextSize(1.5); 
    display.setTextColor(WHITE); 

    display.setCursor(0, 10); 
    sprintf(str_buff, "Station_%d", station_id); 
    display.print(str_buff);

    display.setCursor(72, 10); 
    sprintf(str_buff, "%02d:%02d:%02d", ntp_H, ntp_m, ntp_S); 
    display.print(str_buff);

    display.setCursor(0, 25); 
    sprintf(str_buff, "PM  1.0 = %4d ug/m3 ", pm_1); 
    display.print(str_buff);
    
    display.setCursor(0, 40); 
    sprintf(str_buff, "PM  2.5 = %4d ug/m3 ", pm_25); 
    display.print(str_buff);
    
    display.setCursor(0, 55); 
    sprintf(str_buff, "PM 10.0 = %4d ug/m3 ", pm_10); 
    display.print(str_buff);

    display.display(); 

    sprintf(str_buff, "%04d-%02d-%02dT%02d:%02d:%02d.0+07:00", ntp_Y, ntp_M, ntp_D, ntp_H, ntp_m, ntp_S); 
    Serial.print(str_buff); 

    updateTime_str = str_buff;

    sprintf(str_buff, " | %d | %04d | %4d | %4d | %4d | ", station_id, dtype, pm_1, pm_25, pm_10); 
    Serial.print(str_buff); 

    if (ntp_S_10 != (ntp_S / 30)) { 
      ntp_S_10 = (ntp_S / 30); 
      Update_Document(); 
      Serial.print("Updated "); 
    } 

    Serial.println(); 
  } 

  while ((micros() - t_old) < 100000L); t_old = micros(); 
  tmr_cnt++; if (tmr_cnt >= 20) { tmr_cnt = 0; } 
} 


//----------------------------------------------------------------------------- 
void Disp_Info() { 
  display.clearDisplay();

  display.setTextSize(1); // Draw 2X-scale text
  display.setTextColor(WHITE); 
  
  display.setCursor(40, 5); 
  display.println(F("Faculty")); 

  display.setCursor(53, 20); 
  display.println(F("of")); 

  display.setCursor(30, 35); 
  display.println(F("Engineering")); 

  display.setCursor(45, 50); 
  display.println(F("RMUTR")); 

  display.display(); 
}

//----------------------------------------------------------------------------- 
unsigned long sendNTPpacket(IPAddress& address) {
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[ 0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[ 1] = 0;     // Stratum, or type of clock
  packetBuffer[ 2] = 6;     // Polling Interval
  packetBuffer[ 3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udp.beginPacket(address, 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket(); 

  return 0;
} 

void NTP_Update() { 
  int bbytes = udp.parsePacket(); 

  if (bbytes > 0) { 
    // Do nothing. 
  } else { 
    udp.read(packetBuffer, NTP_PACKET_SIZE); 

    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]); 
    unsigned long lowWord  = word(packetBuffer[42], packetBuffer[43]); 
    unsigned long secs_since_1900 = highWord << 16 | lowWord; 

    if (secs_since_1900 > 0) { 
      unsigned long epoch = secs_since_1900; 
      unsigned long date_offset = 45015UL; 
      unsigned long gmt_offset = +7UL; 

      epoch = (epoch + (gmt_offset * 3600UL)); 

      ntp_Y = (int)2023; 
      ntp_M = (int)4; 
      ntp_D = (int)((epoch / 86400UL) - date_offset + 1UL); 
      //if (ntp_D > 99) { ntp_D = 99; } 

      ntp_H = (int)((epoch % 86400UL) / 3600UL); 
      ntp_m = (int)((epoch % 3600UL) / 60UL); 
      ntp_S = (int) (epoch % 60UL); 
    } 
  } 

  WiFi.hostByName(ntpServerName, timeServerIP); 
  sendNTPpacket(timeServerIP); 
} 

//----------------------------------------------------------------------------- 
void Update_Document() { 
  FirebaseJson content; 

  if (taskcomplete == false) { 
    taskcomplete = true; 

    content.clear(); 
    content.set("fields/station_data/stringValue", String(0).c_str()); 
    //content.set("fields/station_update/timestampValue", "2023-01-01T00:00:00.0Z"); 
    //content.set("fields/station_update/timestampValue", "2023-01-01T00:00:00.0+07:00"); 
    //Serial.print("Create a document... "); 

    if (Firebase.Firestore.createDocument(&fbdo, FIREBASE_PROJECT_ID, "" /* databaseId can be (default) or empty */, documentPath.c_str(), content.raw())) { 
      //Serial.printf("ok\n%s\n\n", fbdo.payload().c_str()); 
    } else { 
      //Serial.println(fbdo.errorReason()); 
    } 
  } 

  content.clear(); 
  content.set("fields/station_data/stringValue", String(pm_25).c_str()); 
  content.set("fields/station_update/timestampValue", updateTime_str); 
  //Serial.print("Update a document... "); 

  if (Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "" /* databaseId can be (default) or empty */, documentPath.c_str(), content.raw(), "station_data,station_update" /* updateMask */)) { 
    //Serial.printf("ok\n%s\n\n", fbdo.payload().c_str()); 
  } else { 
    //Serial.println(fbdo.errorReason()); 
  } 

}
