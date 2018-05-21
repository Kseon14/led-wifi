#define FASTLED_ESP8266_RAW_PIN_ORDER
#define FASTLED_ESP8266_NODEMCU_PIN_ORDER
#define FASTLED_FORCE_SOFTWARE_SPI
#include <stdio.h>
#include <TimeLib.h>
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

U8G2_SSD1306_128X32_UNIVISION_F_SW_I2C u8g2(U8G2_R0, CLOCK_PIN, DATA_PIN, /* reset=*/ U8X8_PIN_NONE);

char* ssid = "KS6";  //  your network SSID (name)
char* pass = "Rom654321";       // your network password

int bluePin = 14;
int redPin = 12;
int greenPin = 15;

int alarms[4] = {14, 21, 22, 22}; //array for values of time

//if "white" (R=255, G=255, B=255) doesn't look white,
//reduce the red, green, or blue max value.
int red = 0;
int green = 0;
int blue = 0;

int maxValue = 170;
int deltaValue = 50;

int timeToSyncWithNet = 86400;
int sundays[] = {0, 0, 0, 0, 0};

int maxDay = 0;

unsigned int localPort = 2390;
IPAddress timeServerIP; // time.nist.gov NTP server ad dress
const char* ntpServerName = "time.nist.gov";
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udp;

unsigned long sendNTPpacket(IPAddress& address) {
  Serial.print("sending NTP packet to ");
  Serial.println(address);
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;    // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udp.beginPacket(address, 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}

int getTimeZone() {

  if (month() == 10) {
    if (maxDay != 0) {
      Serial.println("max day not 0");
      if (day() < maxDay) {
        return 3;
      } else {
        return 2;
      }
    }
    calculateSundays();
    Serial.println(maxDay);
    if (day() < maxDay) {
      return 3;
    } else {
      return 2;
    }
  }
  if (month() == 3) {
    if (maxDay != 0) {
      if (day() < maxDay) {
        return 2;
      } else {
        return 3;
      }
    }
    calculateSundays();
    if (day() < maxDay) {
      return 2;
    } else {
      return 3;
    }
  }

  if (month() < 3 || month() > 10 ) {
    return 2;
  } else {
    return 3;
  }
}

void calculateSundays() {
  maxDay = 0;
  long curTime = now();
  long daysToMonthEnd = 31 - day();
  Serial.print("Days till the month end ");
  Serial.println(daysToMonthEnd);
  int i = 0;
  //if current day is Sunday add to array
  if (weekday(curTime) == 1) {
    sundays[i] = day(curTime);
    Serial.print("Current day is Sunday");
    Serial.println(sundays[i]);
    i++;
  }
  long tmpTime = curTime;
  for (int y = 0; y <= daysToMonthEnd; y ++) {
    Serial.print("Iteration # ");
    Serial.println(y);
    if (weekday(tmpTime) == 1) {
      sundays[i] = day(tmpTime);
      Serial.println(sundays[i]);
      i++;
    }
    tmpTime = tmpTime + 86400;
  }
  for (int y = 0; y < 5; y++) {
    if (sundays[y] > maxDay) {
      maxDay = sundays[y];
    }
  }
  if (maxDay == 0) {
    Serial.println("Current day after last Sunday of month");
    maxDay = -1;
  }
  Serial.println("Max month Sunday date");
  Serial.println(maxDay);
  for (int i = 0; i < (sizeof(sundays) / sizeof(int)); i++) {
    Serial.print(sundays[i]);
    Serial.print(",");
  }
}

void setConnection() {
  Serial.begin(115200);
  Serial.println();
  Serial.println();

  // We start by connecting to a WiFi network
  Serial.print("Connecting to ");
  Serial.println(ssid);
  printText("Connecting to " + String(ssid));
  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");

  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.println("Starting UDP");
  udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(udp.localPort());
}

time_t getTime() {
  //get a random server from the pool
  WiFi.hostByName(ntpServerName, timeServerIP);

  sendNTPpacket(timeServerIP); // send an NTP packet to a time server
  // wait to see if a reply is available
  printText("sending NTP....");
  delay(9000);
  unsigned long epoch = 0;

  int cb = udp.parsePacket();
  if (!cb) {
    Serial.println("no packet yet");
    printText("no packet yet....");
  } else {
    // We've received a packet, read the data from it
    udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
    //the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, esxtract the two words:
    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    Serial.print("Unix time = ");
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;
    epoch = secsSince1900 - seventyYears;
    Serial.println(epoch);
    printText("Unix time=" + String(epoch));
    return epoch;
  }
  return 0;
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
  if (alarms[0] <= hour() && hour() < alarms[1]) {
    t1 = "setLedOn";
    setLedOn();
  }
  else if (alarms[1] <= hour() && hour() < alarms[2]) {
    t1 = "setSunset";
    setSunset();
  }
  else if (alarms[2] <= hour() && hour() < alarms[3]) {
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

void printTzAndTimeToSync() {
  String d2 = "tz:" + String(getTimeZone()) + " ts:" + String(timeStatus());
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

void adjustTimeZone() {
  if (timeStatus() == timeSet) {
    adjustTime(getTimeZone() * SECS_PER_HOUR);
    Serial.print("Time is adjusted to ");
    Serial.println(getTimeZone());
  }
}

void setup() {
  FastLED.addLeds<P9813, DATA_PIN_LED, CLOCK_PIN_LED, RGB>(leds, NUM_LEDS);
  u8g2.begin();
  u8g2.setFont( u8g2_font_6x10_tf);

  initLedOff();
  checkLedsChannels();
  initLedOff();
  setConnection();

  while (timeStatus() != timeSet) {
    setSyncProvider(getTime);
  }

  setSyncInterval(7200);
  delay(1000);
  adjustTimeZone();
  setCurrentLedLights();

  //Create an alarm that will call a function every day at a particular time.
  Alarm.alarmRepeat(alarms[0], 00, 0, setLedOn);
  Alarm.alarmRepeat(alarms[1], 00, 0, setSunset);
  Alarm.alarmRepeat(alarms[2], 00, 0, setTwilight);
  Alarm.alarmRepeat(alarms[3], 30, 0, setLedOff);
}

void loop() {
  setLedPinWithLevel();
  u8g2.firstPage();
  adjustTimeZone();
  Serial.println();
  do {
    digitalClockDisplay();
    printLed();
    printTzAndTimeToSync();
  } while ( u8g2.nextPage() );
  Alarm.delay(1000);
}


