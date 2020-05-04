#include "tuerklingel.h"
#include "DFRobotDFPlayerMini.h"
#include "MQTT.h"
#include "MQTT_credentials.h"
#include "particle-dst.h"

#include <stdlib.h>

#define PIN_BTN D0

#define SETTINGS_MAGIC 42
#define SETTINGS_START 1

// For daylight saving time:
DST dst;

dst_limit_t beginning;
dst_limit_t end;

#define ONE_DAY_MILLIS (24 * 60 * 60 * 1000)
unsigned long lastSync = millis();

DFRobotDFPlayerMini myDFPlayer;

void mqtt_callback(char *, byte *, unsigned int);
void saveSettings();
void loadSettings();
void printDetail(uint8_t type, int value);
void publishState();

// Timer functions:
//
Timer PublisherTimer(5000, publishState);

MQTT client(MQTT_HOST, 1883, mqtt_callback);

ApplicationWatchdog wd(60000, System.reset);

void mqtt_callback(char *topic, byte *payload, unsigned int length) {
    String myTopic = String(topic);

    bool stateChanged = false;
    char *myPayload = NULL;

    myPayload = (char *) malloc(length + 1);

    memcpy(myPayload, payload, length);
    myPayload[length] = 0;

    String myID = System.deviceID();

    if (!client.isConnected()) {
        client.connect(myID, MQTT_USER, MQTT_PASSWORD);
    }

    client.publish("/" + myID + "/state/LastPayload", String(myPayload));

    if (stateChanged) {
        publishState();
    }

    free(myPayload);
    myPayload = NULL;
}

void publishState() {

    String myID = System.deviceID();
    
    if (!client.isConnected()) {
        client.connect(myID, MQTT_USER, MQTT_PASSWORD);
    }

    if (client.isConnected()) {
        client.publish("/" + myID + "/state/FirmwareVersion", System.version());
    }
}

void loadSettings() {

    uint16_t maxSize = EEPROM.length();
    uint16_t address = 1;
    uint8_t version = 0;
    char loadArray[2048];

    EEPROM.get(address++, version);

    if (version == SETTINGS_MAGIC) { // Valid settings in EEPROM?
// FIXME       EEPROM.get(address, dispMode);
    } else {
        saveSettings();
    }
}

void saveSettings() {

    uint16_t address = SETTINGS_START;
    uint16_t maxlength = EEPROM.length();
    char saveArray[2048];

    EEPROM.write(address++, SETTINGS_MAGIC);
}

void playMelody() {
  if (myDFPlayer.readState() != 513) {
    myDFPlayer.play(1);
  }
}

void setup() {
  Serial.begin(9600);
  Serial1.begin(9600);
  delay(2000);
  Serial.println();
  Serial.println(F("Initializing DFPlayer ... (May take 3~5 seconds)"));

  if (!myDFPlayer.begin(Serial1)) {  //Use softwareSerial to communicate with mp3.
    Serial.println(F("Unable to begin:"));
    Serial.println(F("1.Please recheck the connection!"));
    Serial.println(F("2.Please insert the SD card!"));
    while (true);
  }
  Serial.println(F("DFPlayer Mini online."));

  //----Set serial communictaion time out 500ms
  myDFPlayer.setTimeOut(500);

  //----Set volume----
  myDFPlayer.volume(20);  //Set volume value (0~30).

  //----Set different EQ----
  //myDFPlayer.EQ(DFPLAYER_EQ_NORMAL);
  myDFPlayer.EQ(DFPLAYER_EQ_POP);

  //----Set device we use SD as default----
  myDFPlayer.outputDevice(DFPLAYER_DEVICE_SD);

  //----Init Pushbutton pin----
  pinMode(PIN_BTN, INPUT_PULLUP);
  
}

void loop() {
    if (digitalRead(PIN_BTN)) {
        playMelody();
    }
  delay(2000);
  if (myDFPlayer.available()) {
    printDetail(myDFPlayer.readType(), myDFPlayer.read()); //Print the detail message from DFPlayer to handle different errors and states.
  }
}

void printDetail(uint8_t type, int value) {
  switch (type) {
    case TimeOut:
      Serial.println(F("Time Out!"));
      break;
    case WrongStack:
      Serial.println(F("Stack Wrong!"));
      break;
    case DFPlayerCardInserted:
      Serial.println(F("Card Inserted!"));
      break;
    case DFPlayerCardRemoved:
      Serial.println(F("Card Removed!"));
      break;
    case DFPlayerCardOnline:
      Serial.println(F("Card Online!"));
      break;
    case DFPlayerUSBInserted:
      Serial.println("USB Inserted!");
      break;
    case DFPlayerUSBRemoved:
      Serial.println("USB Removed!");
      break;
    case DFPlayerPlayFinished:
      Serial.print(F("Number:"));
      Serial.print(value);
      Serial.println(F(" Play Finished!"));
      break;
    case DFPlayerError:
      Serial.print(F("DFPlayerError:"));
      switch (value) {
        case Busy:
          Serial.println(F("Card not found"));
          break;
        case Sleeping:
          Serial.println(F("Sleeping"));
          break;
        case SerialWrongStack:
          Serial.println(F("Get Wrong Stack"));
          break;
        case CheckSumNotMatch:
          Serial.println(F("Check Sum Not Match"));
          break;
        case FileIndexOut:
          Serial.println(F("File Index Out of Bound"));
          break;
        case FileMismatch:
          Serial.println(F("Cannot Find File"));
          break;
        case Advertise:
          Serial.println(F("In Advertise"));
          break;
        default:
          break;
      }
      break;
    default:
      break;
  }
}
