/**************************************************************
 *
 * TinyGSM Getting Started guide:
 *   https://tiny.cc/tinygsm-readme
 *
 * NOTE:
 * Some of the functions may be unavailable for your modem.
 * Just comment them out.
 *
 **************************************************************/

// Select your modem:
//#define TINY_GSM_MODEM_SIM800
// #define TINY_GSM_MODEM_SIM808
// #define TINY_GSM_MODEM_SIM868
// #define TINY_GSM_MODEM_SIM900
// #define TINY_GSM_MODEM_SIM7000
// #define TINY_GSM_MODEM_SIM7000SSL
#define TINY_GSM_MODEM_SIM7020
// #define TINY_GSM_MODEM_SIM7080
// #define TINY_GSM_MODEM_SIM5360
// #define TINY_GSM_MODEM_SIM7600
// #define TINY_GSM_MODEM_UBLOX
// #define TINY_GSM_MODEM_SARAR4
// #define TINY_GSM_MODEM_M95
// #define TINY_GSM_MODEM_BG96
// #define TINY_GSM_MODEM_A6
// #define TINY_GSM_MODEM_A7
// #define TINY_GSM_MODEM_M590
// #define TINY_GSM_MODEM_MC60
// #define TINY_GSM_MODEM_MC60E
// #define TINY_GSM_MODEM_ESP8266
// #define TINY_GSM_MODEM_XBEE
// #define TINY_GSM_MODEM_SEQUANS_MONARCH

// Set serial for debug console (to the Serial Monitor, default speed 115200)
#define SerialMon Serial

// Set serial for AT commands (to the module)
// Use Hardware Serial on Mega, Leonardo, Micro
#ifndef __AVR_ATmega328P__
#define SerialAT Serial1

// or Software Serial on Uno, Nano
#else
#include <SoftwareSerial.h>
SoftwareSerial SerialAT(2, 3);  // RX, TX
#endif

// See all AT commands, if wanted
#define DUMP_AT_COMMANDS

// Define the serial console for debug prints, if needed
#define TINY_GSM_DEBUG SerialMon

// Range to attempt to autobaud
// NOTE:  DO NOT AUTOBAUD in production code.  Once you've established
// communication, set a fixed baud rate using modem.setBaud(#).
#define GSM_AUTOBAUD_MIN 9600
#define GSM_AUTOBAUD_MAX 57600

// For TINY_GSM_MODEM_SIM7020 on M5 ATOM
#ifdef TINY_GSM_MODEM_SIM7020

#define GSM_BAUDRATE 115200
#define GSM_TX_PIN 22
#define GSM_RX_PIN 19
#if !defined(TINY_GSM_RX_BUFFER)
#define TINY_GSM_RX_BUFFER 1460
#endif

#endif


// Add a reception delay, if needed.
// This may be needed for a fast processor at a slow baud rate.
// #define TINY_GSM_YIELD() { delay(2); }

/*
 * Tests enabled
 */
#define TINY_GSM_TEST_GPRS true
#define TINY_GSM_TEST_WIFI false
//#define TINY_GSM_TEST_TCP true
#define TINY_GSM_TEST_TCP false
//#define TINY_GSM_TEST_SSL true
#define TINY_GSM_TEST_SSL false
#define TINY_GSM_TEST_HTTP true
#define TINY_GSM_TEST_CALL false
#define TINY_GSM_TEST_SMS false
#define TINY_GSM_TEST_USSD false
//#define TINY_GSM_TEST_BATTERY true
#define TINY_GSM_TEST_BATTERY false
//#define TINY_GSM_TEST_TEMPERATURE true
#define TINY_GSM_TEST_TEMPERATURE false
#define TINY_GSM_TEST_GSM_LOCATION false
#define TINY_GSM_TEST_NTP false
#define TINY_GSM_TEST_TIME false
#define TINY_GSM_TEST_GPS false
// disconnect and power down modem after tests
#define TINY_GSM_POWERDOWN false

// set GSM PIN, if any
#define GSM_PIN ""

// Set phone numbers, if you want to test SMS and Calls
// #define SMS_TARGET  "+380xxxxxxxxx"
// #define CALL_TARGET "+380xxxxxxxxx"

// Your GPRS credentials, if any
const char apn[] = "telstra.iot";
// const char apn[] = "ibasis.iot";
const char gprsUser[] = "";
const char gprsPass[] = "";

// Your WiFi connection credentials, if applicable
const char wifiSSID[] = "YourSSID";
const char wifiPass[] = "YourWiFiPass";

// Server details to test TCP/SSL, checking IPv6 http://v4v6.ipv6-test.com/api/myip.php
//const char server[]   = "vsh.pp.ua";
//const char resource[] = "/TinyGSM/logo.txt";
const char server[] = "v4v6.ipv6-test.com";
const char server_ip[] = "51.75.78.103";
const char server_ipv6[] = "2001:41d0:701:1100::29c8";
const char resource[] = "/api/myip.php";

#include <M5Atom.h>
#include <TinyGsmClient.h>
#include <ArduinoHttpClient.h>
#include <Arduino.h>

#if TINY_GSM_TEST_GPRS && not defined TINY_GSM_MODEM_HAS_GPRS
#undef TINY_GSM_TEST_GPRS
#undef TINY_GSM_TEST_WIFI
#define TINY_GSM_TEST_GPRS false
#define TINY_GSM_TEST_WIFI true
#endif
#if TINY_GSM_TEST_WIFI && not defined TINY_GSM_MODEM_HAS_WIFI
#undef TINY_GSM_USE_GPRS
#undef TINY_GSM_USE_WIFI
#define TINY_GSM_USE_GPRS true
#define TINY_GSM_USE_WIFI false
#endif

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm        modem(debugger);
#else
TinyGsm        modem(SerialAT);
#endif

// Root certificate for http://v4v6.ipv6-test.com/api/myip.php
const String root_ca = \
  "-----BEGIN CERTIFICATE-----\n" \
  "MIIF3jCCA8agAwIBAgIQAf1tMPyjylGoG7xkDjUDLTANBgkqhkiG9w0BAQwFADCB\n" \
  "iDELMAkGA1UEBhMCVVMxEzARBgNVBAgTCk5ldyBKZXJzZXkxFDASBgNVBAcTC0pl\n" \
  "cnNleSBDaXR5MR4wHAYDVQQKExVUaGUgVVNFUlRSVVNUIE5ldHdvcmsxLjAsBgNV\n" \
  "BAMTJVVTRVJUcnVzdCBSU0EgQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkwHhcNMTAw\n" \
  "MjAxMDAwMDAwWhcNMzgwMTE4MjM1OTU5WjCBiDELMAkGA1UEBhMCVVMxEzARBgNV\n" \
  "BAgTCk5ldyBKZXJzZXkxFDASBgNVBAcTC0plcnNleSBDaXR5MR4wHAYDVQQKExVU\n" \
  "aGUgVVNFUlRSVVNUIE5ldHdvcmsxLjAsBgNVBAMTJVVTRVJUcnVzdCBSU0EgQ2Vy\n" \
  "dGlmaWNhdGlvbiBBdXRob3JpdHkwggIiMA0GCSqGSIb3DQEBAQUAA4ICDwAwggIK\n" \
  "AoICAQCAEmUXNg7D2wiz0KxXDXbtzSfTTK1Qg2HiqiBNCS1kCdzOiZ/MPans9s/B\n" \
  "3PHTsdZ7NygRK0faOca8Ohm0X6a9fZ2jY0K2dvKpOyuR+OJv0OwWIJAJPuLodMkY\n" \
  "tJHUYmTbf6MG8YgYapAiPLz+E/CHFHv25B+O1ORRxhFnRghRy4YUVD+8M/5+bJz/\n" \
  "Fp0YvVGONaanZshyZ9shZrHUm3gDwFA66Mzw3LyeTP6vBZY1H1dat//O+T23LLb2\n" \
  "VN3I5xI6Ta5MirdcmrS3ID3KfyI0rn47aGYBROcBTkZTmzNg95S+UzeQc0PzMsNT\n" \
  "79uq/nROacdrjGCT3sTHDN/hMq7MkztReJVni+49Vv4M0GkPGw/zJSZrM233bkf6\n" \
  "c0Plfg6lZrEpfDKEY1WJxA3Bk1QwGROs0303p+tdOmw1XNtB1xLaqUkL39iAigmT\n" \
  "Yo61Zs8liM2EuLE/pDkP2QKe6xJMlXzzawWpXhaDzLhn4ugTncxbgtNMs+1b/97l\n" \
  "c6wjOy0AvzVVdAlJ2ElYGn+SNuZRkg7zJn0cTRe8yexDJtC/QV9AqURE9JnnV4ee\n" \
  "UB9XVKg+/XRjL7FQZQnmWEIuQxpMtPAlR1n6BB6T1CZGSlCBst6+eLf8ZxXhyVeE\n" \
  "Hg9j1uliutZfVS7qXMYoCAQlObgOK6nyTJccBz8NUvXt7y+CDwIDAQABo0IwQDAd\n" \
  "BgNVHQ4EFgQUU3m/WqorSs9UgOHYm8Cd8rIDZsswDgYDVR0PAQH/BAQDAgEGMA8G\n" \
  "A1UdEwEB/wQFMAMBAf8wDQYJKoZIhvcNAQEMBQADggIBAFzUfA3P9wF9QZllDHPF\n" \
  "Up/L+M+ZBn8b2kMVn54CVVeWFPFSPCeHlCjtHzoBN6J2/FNQwISbxmtOuowhT6KO\n" \
  "VWKR82kV2LyI48SqC/3vqOlLVSoGIG1VeCkZ7l8wXEskEVX/JJpuXior7gtNn3/3\n" \
  "ATiUFJVDBwn7YKnuHKsSjKCaXqeYalltiz8I+8jRRa8YFWSQEg9zKC7F4iRO/Fjs\n" \
  "8PRF/iKz6y+O0tlFYQXBl2+odnKPi4w2r78NBc5xjeambx9spnFixdjQg3IM8WcR\n" \
  "iQycE0xyNN+81XHfqnHd4blsjDwSXWXavVcStkNr/+XeTWYRUc+ZruwXtuhxkYze\n" \
  "Sf7dNXGiFSeUHM9h4ya7b6NnJSFd5t0dCy5oGzuCr+yDZ4XUmFF0sbmZgIn/f3gZ\n" \
  "XHlKYC6SQK5MNyosycdiyA5d9zZbyuAlJQG03RoHnHcAP9Dc1ew91Pq7P8yF1m9/\n" \
  "qS3fuQL39ZeatTXaw2ewh0qpKJ4jjv9cJ2vhsE/zB+4ALtRZh8tSQZXq9EfX7mRB\n" \
  "VXyNWQKV3WKdwrnuWih0hKWbt5DHDAff9Yk2dDLWKMGwsAvgnEzDHNb842m1R0aB\n" \
  "L6KCq9NjRHDEjf8tM7qtj3u1cIiuPhnPQCjY/MiQu12ZIvVS5ljFH4gxQ+6IHdfG\n" \
  "jjxDah2nGN59PRbxYvnKkKj9\n" \
  "-----END CERTIFICATE-----";

void setup() {
  // Set console baud rate
  SerialMon.begin(115200);
  delay(10);

  // !!!!!!!!!!!
  // Set your reset, enable, power pins here
  // !!!!!!!!!!!

  DBG("Wait...");
  delay(6000);

  // Set GSM module baud rate
  #ifdef GSM_BAUDRATE
    // SerialAT.begin(9600);
    SerialAT.begin(GSM_BAUDRATE, SERIAL_8N1, GSM_RX_PIN, GSM_TX_PIN);
  #else
    TinyGsmAutoBaud(SerialAT, GSM_AUTOBAUD_MIN, GSM_AUTOBAUD_MAX);
  #endif
}

void loop() {
  // Restart takes quite some time
  // To skip it, call init() instead of restart()
  DBG("Initializing modem...");
  //if (!modem.restart()) {
  if (!modem.init()) {
    DBG("Failed to restart modem, delaying 10s and retrying");
    // restart autobaud in case GSM just rebooted
    // TinyGsmAutoBaud(SerialAT, GSM_AUTOBAUD_MIN, GSM_AUTOBAUD_MAX);
    return;
  }

  String name = modem.getModemName();
  DBG("Modem Name:", name);

  String modemInfo = modem.getModemInfo();
  DBG("Modem Info:", modemInfo);

#if TINY_GSM_TEST_GPRS
  // Unlock your SIM card with a PIN if needed
  if (GSM_PIN && modem.getSimStatus() != 3) { modem.simUnlock(GSM_PIN); }
#endif

#if TINY_GSM_TEST_WIFI
  DBG("Setting SSID/password...");
  if (!modem.networkConnect(wifiSSID, wifiPass)) {
    DBG(" fail");
    delay(10000);
    return;
  }
  SerialMon.println(" success");
#endif

#if TINY_GSM_TEST_GPRS && defined TINY_GSM_MODEM_XBEE
  // The XBee must run the gprsConnect function BEFORE waiting for network!
  modem.gprsConnect(apn, gprsUser, gprsPass);
#endif

  DBG("Waiting for network...");
  if (!modem.waitForNetwork(600000L, true)) {
    delay(10000);
    return;
  }

  if (modem.isNetworkConnected()) { DBG("Network connected"); }

#if TINY_GSM_TEST_GPRS
  DBG("Connecting to", apn);
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    delay(10000);
    return;
  }

  bool res = modem.isGprsConnected();
  DBG("GPRS status:", res ? "connected" : "not connected");

  String ccid = modem.getSimCCID();
  DBG("CCID:", ccid);

  String imei = modem.getIMEI();
  DBG("IMEI:", imei);

  String imsi = modem.getIMSI();
  DBG("IMSI:", imsi);

  String cop = modem.getOperator();
  DBG("Operator:", cop);

  IPAddress local = modem.localIP();
  DBG("Local IP:", local);

  int csq = modem.getSignalQuality();
  DBG("Signal quality:", csq);

  modem.sendAT("+CGCONTRDP");
  modem.waitResponse();  

  modem.sendAT("+IPCONFIG");
  modem.waitResponse();  
#endif

#if TINY_GSM_TEST_USSD && defined TINY_GSM_MODEM_HAS_SMS
  String ussd_balance = modem.sendUSSD("*111#");
  DBG("Balance (USSD):", ussd_balance);

  String ussd_phone_num = modem.sendUSSD("*161#");
  DBG("Phone number (USSD):", ussd_phone_num);
#endif

#if TINY_GSM_TEST_TCP && defined TINY_GSM_MODEM_HAS_TCP
  TinyGsmClient client(modem, 0);
  const int     port = 80;
  DBG("Connecting to", server, server_ip);
  if (!client.connect(server_ip, port)) {
    DBG("... failed");
  } else {
    // Make a HTTP GET request:
    client.print(String("GET ") + resource + " HTTP/1.0\r\n");
    client.print(String("Host: ") + server + "\r\n");
    client.print("Connection: close\r\n\r\n");

    // Wait for data to arrive
    uint32_t start = millis();
    while (client.connected() && !client.available() &&
           millis() - start < 30000L) {
      delay(100);
    };

    // Read data
    start          = millis();
    char logo[1460] = {
        '\0',
    };
    int read_chars = 0;
    while (client.connected() && millis() - start < 10000L) {
      while (client.available()) {
        logo[read_chars]     = client.read();
        logo[read_chars + 1] = '\0';
        read_chars++;
        start = millis();
      }
    }
    SerialMon.println("##### OUTPUT:");
    SerialMon.println(logo);
    DBG("#####  RECEIVED:", strlen(logo), "CHARACTERS");
    client.stop();
  }
  delay(2000);
#endif

#if TINY_GSM_TEST_SSL && defined TINY_GSM_MODEM_HAS_SSL
  TinyGsmClientSecure secureClient(modem, 1);
  const int           securePort = 443;

#ifdef TINY_GSM_MODEM_SIM7020
  DBG("Setting root CA");
  modem.setRootCA(root_ca);
  //DBG("Setting server certificate");
  //secureClient.setServerCertificate(root_ca);
#endif

  DBG("Connecting securely to", server);
  if (!secureClient.connect(server_ip, securePort)) {
    DBG("... failed");
  } else {
    // Make a HTTP GET request:
    secureClient.print(String("GET ") + resource + " HTTP/1.0\r\n");
    secureClient.print(String("Host: ") + server + "\r\n");
    secureClient.print("Connection: close\r\n\r\n");

    // Wait for data to arrive
    uint32_t startS = millis();
    while (secureClient.connected() && !secureClient.available() &&
           millis() - startS < 30000L) {
      delay(100);
    };

    // Read data
    startS          = millis();
    char logoS[640] = {
        '\0',
    };
    int read_charsS = 0;
    while (secureClient.connected() && millis() - startS < 10000L) {
      while (secureClient.available()) {
        logoS[read_charsS]     = secureClient.read();
        logoS[read_charsS + 1] = '\0';
        read_charsS++;
        startS = millis();
      }
    }
    SerialMon.println(logoS);
    DBG("#####  RECEIVED:", strlen(logoS), "CHARACTERS");
    secureClient.stop();
  }
#endif

#if TINY_GSM_TEST_HTTP
  SerialMon.println("##### HTTP TEST");
  #ifndef TINY_GSM_MODEM_HAS_HTTP
  typedef HttpClient   TinyGsmHttpClient;
  #endif
  TinyGsmClient client2(modem, 2);
  TinyGsmHttpClient http(client2, server, 80);

  int rc = http.get("/api/myip.php");
  if (rc != 0) {
    DBG("HTTP GET error:", rc);
  } else {
    int httpCode = http.responseStatusCode();
    if (httpCode != 200 && httpCode != 301) {
      DBG("HTTP response code error: ", httpCode);
    } else {
      DBG("HTTP response code: ", httpCode);
      String payload = http.responseBody();
      SerialMon.println("##### PAYLOAD:");
      SerialMon.println(payload);
    }
  }

  http.stop();

  delay(2000);
#endif

#if TINY_GSM_TEST_CALL && defined TINY_GSM_MODEM_HAS_CALLING && \
    defined                       CALL_TARGET
  DBG("Calling:", CALL_TARGET);

  // This is NOT supported on M590
  res = modem.callNumber(CALL_TARGET);
  DBG("Call:", res ? "OK" : "fail");

  if (res) {
    delay(1000L);

    // Play DTMF A, duration 1000ms
    modem.dtmfSend('A', 1000);

    // Play DTMF 0..4, default duration (100ms)
    for (char tone = '0'; tone <= '4'; tone++) { modem.dtmfSend(tone); }

    delay(5000);

    res = modem.callHangup();
    DBG("Hang up:", res ? "OK" : "fail");
  }
#endif

#if TINY_GSM_TEST_SMS && defined TINY_GSM_MODEM_HAS_SMS && defined SMS_TARGET
  res = modem.sendSMS(SMS_TARGET, String("Hello from ") + imei);
  DBG("SMS:", res ? "OK" : "fail");

  // This is only supported on SIMxxx series
  res = modem.sendSMS_UTF8_begin(SMS_TARGET);
  if (res) {
    auto stream = modem.sendSMS_UTF8_stream();
    stream.print(F("Привіііт! Print number: "));
    stream.print(595);
    res = modem.sendSMS_UTF8_end();
  }
  DBG("UTF8 SMS:", res ? "OK" : "fail");

#endif

#if TINY_GSM_TEST_GSM_LOCATION && defined TINY_GSM_MODEM_HAS_GSM_LOCATION
  float lat      = 0;
  float lon      = 0;
  float accuracy = 0;
  int   year     = 0;
  int   month    = 0;
  int   day      = 0;
  int   hour     = 0;
  int   min      = 0;
  int   sec      = 0;
  for (int8_t i = 15; i; i--) {
    DBG("Requesting current GSM location");
    if (modem.getGsmLocation(&lat, &lon, &accuracy, &year, &month, &day, &hour,
                             &min, &sec)) {
      DBG("Latitude:", String(lat, 8), "\tLongitude:", String(lon, 8));
      DBG("Accuracy:", accuracy);
      DBG("Year:", year, "\tMonth:", month, "\tDay:", day);
      DBG("Hour:", hour, "\tMinute:", min, "\tSecond:", sec);
      break;
    } else {
      DBG("Couldn't get GSM location, retrying in 15s.");
      delay(15000L);
    }
  }
  DBG("Retrieving GSM location again as a string");
  String location = modem.getGsmLocation();
  DBG("GSM Based Location String:", location);
#endif

#if TINY_GSM_TEST_GPS && defined TINY_GSM_MODEM_HAS_GPS
  DBG("Enabling GPS/GNSS/GLONASS and waiting 15s for warm-up");
  modem.enableGPS();
  delay(15000L);
  float lat2      = 0;
  float lon2      = 0;
  float speed2    = 0;
  float alt2      = 0;
  int   vsat2     = 0;
  int   usat2     = 0;
  float accuracy2 = 0;
  int   year2     = 0;
  int   month2    = 0;
  int   day2      = 0;
  int   hour2     = 0;
  int   min2      = 0;
  int   sec2      = 0;
  for (int8_t i = 15; i; i--) {
    DBG("Requesting current GPS/GNSS/GLONASS location");
    if (modem.getGPS(&lat2, &lon2, &speed2, &alt2, &vsat2, &usat2, &accuracy2,
                     &year2, &month2, &day2, &hour2, &min2, &sec2)) {
      DBG("Latitude:", String(lat2, 8), "\tLongitude:", String(lon2, 8));
      DBG("Speed:", speed2, "\tAltitude:", alt2);
      DBG("Visible Satellites:", vsat2, "\tUsed Satellites:", usat2);
      DBG("Accuracy:", accuracy2);
      DBG("Year:", year2, "\tMonth:", month2, "\tDay:", day2);
      DBG("Hour:", hour2, "\tMinute:", min2, "\tSecond:", sec2);
      break;
    } else {
      DBG("Couldn't get GPS/GNSS/GLONASS location, retrying in 15s.");
      delay(15000L);
    }
  }
  DBG("Retrieving GPS/GNSS/GLONASS location again as a string");
  String gps_raw = modem.getGPSraw();
  DBG("GPS/GNSS Based Location String:", gps_raw);
  DBG("Disabling GPS");
  modem.disableGPS();
#endif

#if TINY_GSM_TEST_NTP && defined TINY_GSM_MODEM_HAS_NTP
  DBG("Asking modem to sync with NTP");
  modem.NTPServerSync("132.163.96.5", 20);
#endif

#if TINY_GSM_TEST_TIME && defined TINY_GSM_MODEM_HAS_TIME
  int   year3    = 0;
  int   month3   = 0;
  int   day3     = 0;
  int   hour3    = 0;
  int   min3     = 0;
  int   sec3     = 0;
  float timezone = 0;
  for (int8_t i = 5; i; i--) {
    DBG("Requesting current network time");
    if (modem.getNetworkTime(&year3, &month3, &day3, &hour3, &min3, &sec3,
                             &timezone)) {
      DBG("Year:", year3, "\tMonth:", month3, "\tDay:", day3);
      DBG("Hour:", hour3, "\tMinute:", min3, "\tSecond:", sec3);
      DBG("Timezone:", timezone);
      break;
    } else {
      DBG("Couldn't get network time, retrying in 15s.");
      delay(15000L);
    }
  }
  DBG("Retrieving time again as a string");
  String time = modem.getGSMDateTime(DATE_FULL);
  DBG("Current Network Time:", time);
#endif

#if TINY_GSM_TEST_BATTERY && defined TINY_GSM_MODEM_HAS_BATTERY
  uint8_t  chargeState = -99;
  int8_t   percent     = -99;
  uint16_t milliVolts  = -9999;
  modem.getBattStats(chargeState, percent, milliVolts);
  DBG("Battery charge state:", chargeState);
  DBG("Battery charge 'percent':", percent);
  DBG("Battery voltage:", milliVolts / 1000.0F);
#endif

#if TINY_GSM_TEST_TEMPERATURE && defined TINY_GSM_MODEM_HAS_TEMPERATURE
  float temp = modem.getTemperature();
  DBG("Chip temperature:", temp);
#endif

#if TINY_GSM_POWERDOWN

#if TINY_GSM_TEST_GPRS
  modem.gprsDisconnect();
  delay(5000L);
  if (!modem.isGprsConnected()) {
    DBG("GPRS disconnected");
  } else {
    DBG("GPRS disconnect: Failed.");
  }
#endif

#if TINY_GSM_TEST_WIFI
  modem.networkDisconnect();
  DBG("WiFi disconnected");
#endif

  // Try to power-off (modem may decide to restart automatically)
  // To turn off modem completely, please use Reset/Enable pins
  modem.poweroff();
  DBG("Poweroff.");
#endif

  DBG("End of tests.");

  // Do nothing forevermore
  while (true) { modem.maintain(); }
}
