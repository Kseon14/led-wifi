#define FASTLED_ESP8266_RAW_PIN_ORDER
#define FASTLED_ESP8266_NODEMCU_PIN_ORDER
#define FASTLED_FORCE_SOFTWARE_SPI
#include <stdio.h>
#include <TimeAlarms.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <FastLED.h>
#include <U8g2lib.h>
#include <SPI.h>
#include <Wire.h>

#define NUM_LEDS 30

#define CLOCK_PIN_LED 5
#define DATA_PIN_LED 4

// Data pin that led data will be written out over
#define DATA_PIN 12
// Clock pin only needed for SPI based chipsets when not using hardware SPI
#define CLOCK_PIN 14

CRGBArray<NUM_LEDS> leds;

//U8G2_SSD1306_128X32_UNIVISION_F_SW_I2C u8g2(U8G2_R0, CLOCK_PIN, DATA_PIN, /* reset=*/ U8X8_PIN_NONE);
//U8G2_SSD1306_128X32_UNIVISION_F_SW_I2C u8g2(U8G2_R0, 5, 4, /* reset=*/ 16);
U8G2_SSD1306_128X32_UNIVISION_F_SW_I2C u8g2(U8G2_R0, 5, 4, /* reset=*/ 16);

#ifndef STASSID
#define STASSID ""
#define STAPSK  ""
#endif
#include <TZ.h>
#define MYTZ TZ_Europe_Kiev

#include <coredecls.h>                  // settimeofday_cb()
#include <Schedule.h>
#include <PolledTimeout.h>
#include <time.h>  // time() ctime()
#include <sys/time.h>                   // struct timeval

#include <sntp.h>                       // sntp_servermode_dhcp()

static timeval tv;
static time_t sntp_now;

static esp8266::polledTimeout::periodicMs showTimeNow(86400000);

int bluePin = 14;
int redPin = 12;
int greenPin = 15;

int alarms[4][4] = {{14, 21, 22, 22}, {00, 00, 00, 30}}; //array for values of time

//if "white" (R=255, G=255, B=255) doesn't look white,
//reduce the red, green, or blue max value.
int red = 0;
int green = 0;
int blue = 0;

int maxValue = 120;
int deltaValue = 50;

String serverIp;

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udp;

void setConnection() {
  Serial.begin(115200);
  Serial.println();
  Serial.println();

  // We start by connecting to a WiFi network
  Serial.print("Connecting to ");
  Serial.println(STASSID);
  printText("Connecting to " + String(STASSID));
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(STASSID, STAPSK);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");

  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void setLedOn() {
  red = maxValue;
  blue = maxValue;
  green = maxValue;
  setLedPinWithLevel();
}

void printText(String text) {
  int str_len = text.length() + 1;
  char char_array[str_len];
  text.toCharArray(char_array, str_len);
  u8g2.clearBuffer();
  u8g2.drawStr(0, 12, char_array);
  u8g2.sendBuffer();
}

void initLedOff() {
  leds = CRGB::Black;
  FastLED.show();
}

void setSunset() {
  for (int i = maxValue; i >= (maxValue - deltaValue); i-- ) {
    red = i;
    setLedPinWithLevel();
    delay(100);
  }

  for (int i = maxValue; i >= 80; i-- ) {
    green = i;
    setLedPinWithLevel();
    delay(100);
  }

  for (int i = maxValue ; i >= 50; i-- ) {
    blue = i;
    setLedPinWithLevel();
    delay(100);
  }
}

void setTwilight() {
  for (int i = 50; i <= (maxValue - deltaValue); i++ ) {
    blue = i;
    setLedPinWithLevel();
    delay(100);
  }

  for (int i = 80; i >= 0; i-- ) {
    green = i;
    setLedPinWithLevel();
    delay(100);
  }

  for (int i = (maxValue - deltaValue); i >= 0; i-- ) {
    red = i;
    setLedPinWithLevel();
    delay(100);
  }

  red = 0;
  green = 0;
}

void setLedOff() {
  red = 0;
  green = 0;
  for (int i = (maxValue - deltaValue); i >= 0; i-- ) {
    blue = i;
    setLedPinWithLevel();
    delay(100);
  }
  blue = 0;
}

void setCurrentLedLights() {
  String t1;
  if (alarms[0][0] <= hour() && hour() < alarms[0][1]) {
    t1 = "setLedOn";
    setLedOn();
  }
  else if (alarms[0][1] <= hour() && hour() < alarms[0][2]) {
    t1 = "setSunset";
    setSunset();
  }
  else if ((alarms[0][2] <= hour() && hour() < alarms[0][3]) || 
              (alarms[0][3] == hour() && minute() < alarms[1][3])) {
    t1 = "setTwilight";
    setTwilight();
  }

  else {
    t1 = "setLedOff";
    setLedOff();
  }
  printText(t1);
  Serial.println(t1);
}

void digitalClockDisplay() {
  String d2 = printDigits(hour()) + ":" + printDigits(minute()) + ":" + printDigits(second()) + " " +
              printDigits(day()) + "." + printDigits(month()) + "." +  String(year()).substring(2);
  Serial.println(d2);
  printOledFirstRow(d2);
}

String printDigits(int digits) {
  if (digits < 10) {
    return String("0") + digits;
  }
  return String(digits);
}

void printLed() {
  String d2 = "b:" + String(blue) + " g:" + String(green) + " r:" + String(red);
  Serial.println(d2);
  printOledSecondRow(d2);
}

void prinTimeServerIp() {
  String d2 = "ts:" + serverIp;
  Serial.println(d2);
  printOledThirdRow(d2);
}

void checkLedsChannels() {
  leds = CRGB(0, 0, 255);
  FastLED.show();
  printText("Checking blue...");
  Serial.println("Checking blue...");
  delay(1000);
  leds = CRGB(255, 0, 0);
  FastLED.show();
  printText("Checking red...");
  Serial.println("Checking red...");
  delay(1000);
  leds = CRGB(0, 255, 0);
  FastLED.show();
  printText("Checking green...");
  Serial.println("Checking green...");
  delay(1000);
}

void printOledFirstRow(String text) {
  int str_len = text.length() + 1;
  char char_array[str_len];
  text.toCharArray(char_array, str_len);
  u8g2.drawStr(0, 10, char_array);
}

void printOledSecondRow(String text) {
  int str_len = text.length() + 1;
  char char_array[str_len];
  text.toCharArray(char_array, str_len);
  u8g2.drawStr(0, 20, char_array);
}

void printOledThirdRow(String text) {
  int str_len = text.length() + 1;
  char char_array[str_len];
  text.toCharArray(char_array, str_len);
  u8g2.drawStr(0, 30, char_array);
}

void setLedPinWithLevel() {
  leds = CRGB(red, green, blue);
  FastLED.show();
}

void showTime() {
  gettimeofday(&tv, nullptr);
  sntp_now = time(nullptr);

#if LWIP_VERSION_MAJOR > 1
  // LwIP v2 is able to list more details about the currently configured SNTP servers
  for (int i = 0; i < SNTP_MAX_SERVERS; i++) {
    IPAddress sntp = *sntp_getserver(i);
    const char* name = sntp_getservername(i);
    if (sntp.isSet()) {
      Serial.printf("sntp%d:     ", i);
      if (name) {
        Serial.printf("%s (%s) ", name, sntp.toString().c_str());
        serverIp = sntp.toString().c_str();
      } else {
        Serial.printf("%s ", sntp.toString().c_str());
      }
    }
  }
#endif

  Serial.println();
  const tm* tm = localtime(&sntp_now);
  Serial.println("wating for time...");
  printText("wating for time...");
  while(tm->tm_year == 70) {
    delay(500);
    Serial.println(".");
  }
  setTime(tm->tm_hour, tm->tm_min, tm->tm_sec, tm->tm_mday, tm->tm_mon+1, tm->tm_year+1900);
 
   // human readable
  Serial.print("ctime:     ");
  Serial.print(ctime(&sntp_now));
 
}

void timeInit(){
  settimeofday_cb(showTime);
  configTime(MYTZ, "pool.ntp.org");
  showTime();
}

void setup() {
  FastLED.addLeds<P9813, DATA_PIN_LED, CLOCK_PIN_LED, RGB>(leds, NUM_LEDS);
  u8g2.begin();
  u8g2.setFont( u8g2_font_6x10_tf);

  initLedOff();
  checkLedsChannels();
  initLedOff();
  setConnection();
  timeInit();
  setCurrentLedLights();

  //Create an alarm that will call a function every day at a particular time.
  Alarm.alarmRepeat(alarms[0][0], alarms[1][0], 0, setLedOn);
  Alarm.alarmRepeat(alarms[0][1], alarms[1][1], 0, setSunset);
  Alarm.alarmRepeat(alarms[0][2], alarms[1][2], 0, setTwilight);
  Alarm.alarmRepeat(alarms[0][3], alarms[1][3], 0, setLedOff);
}

void loop() {
  setLedPinWithLevel();
  u8g2.firstPage();
  if (showTimeNow) {
    showTime();
  }
  Serial.println();
  do {
    digitalClockDisplay();
    printLed();
    prinTimeServerIp();
  } while ( u8g2.nextPage() );
  Alarm.delay(1000);
}
