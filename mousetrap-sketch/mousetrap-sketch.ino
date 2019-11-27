#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>

// this block needs to move into EEPROM to be configurable
const char *ssid     = "*****";
const char *password = "*****";
const char *host = "maker.ifttt.com";
const char *myTrapName = "under stove";
// this block needs to move into EEPROM to be configurable

const int led = 2;
const int trapPin = D6;
const byte validHeader[] = {0xd, 0xe, 0xa, 0xd, 0xb, 0xe, 0xe, 0xf};
const int hasFiredOffset = sizeof(validHeader);

bool stayAwake = false;
long count = 0;

// enable a2d converter for cheap battery strength measurement
ADC_MODE(ADC_VCC);

void maybeInitializeEEPROM()
{
  bool notConsistent = false;
  EEPROM.begin(512);
  
  for (int i=0; i < 8; i++)
  {
    if (validHeader[i] != EEPROM.read(i))
    {
      Serial.print("byte didn't match valid bytes = assuming not initalized: ");
      Serial.print(validHeader[i]);
      Serial.print(" - ");
      Serial.println(EEPROM.read(i));
      EEPROM.write(i, validHeader[i]);
      notConsistent = true;
    }
  }
  if (notConsistent)
  {
    // default to true so if switch is open during initial configuration no notification is set
    EEPROM.write(hasFiredOffset, true);
    EEPROM.commit();
    Serial.print("eeprom was initialized from ground zero");
  }
}

void blinkMe(const int count, const int pause = 1000) 
{
    for (int i = 0; i < count ; i++)
    {
      pinMode(led, OUTPUT);
      digitalWrite(led, LOW); 
      delay(pause);
      digitalWrite(led, HIGH);
      // only delay again if not the last one
      if (i + 1 < count)
      {
        delay(pause);
      }
    }
}

void sendNotification(const char *trapName, const int batteryLevel)
{
  HTTPClient http;
  http.begin("http://maker.ifttt.com/trigger/mousetrap/with/key/bS8xx9j60e0wIHkgrPE-IE");
  http.addHeader("Content-type", "application/json");
  char postString[64];

  Serial.print("sending magic URL to ");
  Serial.println(host);
  sprintf(postString, "{ \"value1\" : \"%s\", \"value2\" : \"%d\"}", trapName, batteryLevel);
  int httpCode = http.POST(postString);
  String payload = http.getString();

  Serial.println(httpCode);
  Serial.println(payload);

  http.end();

  Serial.println();
  Serial.println("closing connection");

  // success
  if (httpCode == 200)
  {
    blinkMe(8, 300);
  }
  else
  {
    blinkMe(24, 150);
    // should keep track of failure here and retry sometime later?
  }
}

void enableOTA() 
{
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  // ArduinoOTA.setHostname("myesp8266");

  // No authentication by default
  // ArduinoOTA.setPassword((const char *)"123");

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
    pinMode(led, OUTPUT);
    blinkMe(5, 200);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
    blinkMe(5, 200);
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    if ((progress / total) % 20 == 0)
    {
      if (digitalRead(led) == HIGH)
      {
        digitalWrite(led, LOW);
      }
      else
      {
        digitalWrite(led, HIGH);
      }
    }
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  stayAwake = true;
}

void connectWifi()
{
  //Serial.println("turning on modem");
  //WiFi.forceSleepWake();
  //WiFi.mode(WIFI_STA);
  //delay(1000);
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");  
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void measureAndReport() 
{
  int batteryLevel = ESP.getVcc();
  bool hasFired;
  
  pinMode(trapPin, INPUT_PULLUP);
  int pinState = digitalRead(trapPin);
  hasFired = EEPROM.read(hasFiredOffset);

  Serial.println("fired/pin");
  Serial.print(hasFired);
  Serial.print(" - ");
  Serial.print(pinState);
  Serial.print(" : level is ");
  Serial.println(batteryLevel);

  // if batteryLevel gets below 2870 ? send help signal
  
  if (hasFired)
  {
    // already fired but is armed again
    if (pinState == LOW)
    {
      Serial.println("rearming");
      // set back to armed
      hasFired = false;
      blinkMe(3, 300); 
    }
  }
  else
  {
    // has trap fired now?
    if (pinState == HIGH)
    {
      connectWifi();
      sendNotification(myTrapName, batteryLevel);
      WiFi.disconnect();
      hasFired = true;
    }
  }
  if (EEPROM.read(hasFiredOffset) != hasFired)
  {
    EEPROM.write(hasFiredOffset, hasFired);
    EEPROM.commit();
  }

  // turn off pullup, no need to waste current
  pinMode(trapPin, INPUT);
}

void setup() 
{
  Serial.begin(74880);
  Serial.println("starting up");
  pinMode(D0, WAKEUP_PULLUP);
  
  pinMode(0, INPUT_PULLUP);
  delay(1000);
  
  // check if button pressed for > 1s; if so, turn on wifi and OTA for 120s
  if (digitalRead(0) == LOW)
  {
    Serial.println("button pressed at startup - going into OTA mode");
    blinkMe(2, 500);
    connectWifi();
    enableOTA();
    blinkMe(5, 150);
  }
  // if button pressed longer than 5s, go into configure mode and advertise self - not done yet
}

void loop()
{
  maybeInitializeEEPROM();
  measureAndReport();

  if (!stayAwake)
  {
    // do i need to disable ADC?  or the modem?
    // WiFi.mode(WIFI_OFF);
    // WiFi.forceSleepBegin();
    // https://github.com/esp8266/Arduino/issues/644
    //pinMode(16, WAKEUP_PULLUP);
    ESP.deepSleep(10e6);
  }
  else
  {
    // should set a timer and turn off this mode after 5 minutes
    ArduinoOTA.handle();

    // remind user we are not deep sleeping
    if ((count++ % 10) == 0)
    {
      blinkMe(1,5);
    }
    delay(1000);
  }
}
