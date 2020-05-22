#include "tuerklingel.h"
#include "DFRobotDFPlayerMini.h"
#include "MQTT.h"
#include "MQTT_credentials.h"
#include "particle-dst.h"
#include "WebServer.h"

#include <stdlib.h>

#define PIN_BTN D5

#define SETTINGS_MAGIC 43
#define SETTINGS_START 1

// For daylight saving time:
DST dst;

dst_limit_t beginning;
dst_limit_t end;

int button_pressed = 1;
bool debug = true;
int current_melody = 2;
int current_volume = 30;

String lastVisitDate = "";
String myID = System.deviceID();

#define ONE_DAY_MILLIS (24 * 60 * 60 * 1000)
unsigned long lastSync = millis();

DFRobotDFPlayerMini myDFPlayer;

void mqtt_callback(char *, byte *, unsigned int);
void saveSettings();
void loadSettings();
void printDetail(uint8_t type, int value);
void publishState();
void publishButtonPushed();

// Timer functions:
//
Timer PublisherTimer(5000, publishState);

MQTT client(MQTT_HOST, 1883, mqtt_callback);

ApplicationWatchdog wd(60000, System.reset);

// Webserver related:
//
#define VERSION_STRING "0.1"

// ROM-based messages used by the application
// These are needed to avoid having the strings use up our limited
//    amount of RAM.

P(Page_start) = "<!DOCTYPE html><html><head><meta http-equiv=\"content-type\" content=\"text/html; charset=UTF-8\"><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\"><title>Doorbell settings</title><style> div, fieldset, input, select { padding: 5px; font-size: 1em; } fieldset { background-color: #f2f2f2; } p { margin: 0.5em 0; } input { width: 100%; box-sizing: border-box; -webkit-box-sizing: border-box; -moz-box-sizing: border-box; } input[type=checkbox], input[type=radio] { width: 1em; margin-right: 6px; vertical-align: -1px; } select { width: 100%; } textarea { resize: none; width: 98%; height: 318px; padding: 5px; overflow: auto; } body { text-align: center; font-family: verdana; } td { padding: 0; } button { border: 0; border-radius: 0.3rem; background-color: #1fa3ec; color: #fff; line-height: 2.4rem; font-size: 1.2rem; width: 100%; -webkit-transition-duration: 0.4s; transition-duration: 0.4s; cursor: pointer; } button:hover { background-color: #0e70a4; } .bred { background-color: #d43535; } .bred:hover { background-color: #931f1f; } .bgrn { background-color: #47c266; } .bgrn:hover { background-color: #5aaf6f; } a { text-decoration: none; } .p { float: left; text-align: left; } .q { float: right; text-align: right; } input[type=range] { height: 34px; -webkit-appearance: none; margin: 10px 0; width: 100%; } input[type=range]:focus { outline: none; } input[type=range]::-webkit-slider-runnable-track { width: 100%; height: 12px; cursor: pointer; animate: 0.2s; box-shadow: 1px 1px 1px #002200; background: #205928; border-radius: 1px; border: 1px solid #18D501; } input[type=range]::-webkit-slider-thumb { box-shadow: 3px 3px 3px #00AA00; border: 2px solid #83E584; height: 23px; width: 23px; border-radius: 23px; background: #439643; cursor: pointer; -webkit-appearance: none; margin-top: -7px; } input[type=range]:focus::-webkit-slider-runnable-track { background: #205928; } input[type=range]::-moz-range-track { width: 100%; height: 12px; cursor: pointer; animate: 0.2s; box-shadow: 1px 1px 1px #002200; background: #205928; border-radius: 1px; border: 1px solid #18D501; } input[type=range]::-moz-range-thumb { box-shadow: 3px 3px 3px #00AA00; border: 2px solid #83E584; height: 23px; width: 23px; border-radius: 23px; background: #439643; cursor: pointer; } input[type=range]::-ms-track { width: 100%; height: 12px; cursor: pointer; animate: 0.2s; background: transparent; border-color: transparent; color: transparent; } input[type=range]::-ms-fill-lower { background: #205928; border: 1px solid #18D501; border-radius: 2px; box-shadow: 1px 1px 1px #002200; } input[type=range]::-ms-fill-upper { background: #205928; border: 1px solid #18D501; border-radius: 2px; box-shadow: 1px 1px 1px #002200; } input[type=range]::-ms-thumb { margin-top: 1px; box-shadow: 3px 3px 3px #00AA00; border: 2px solid #83E584; height: 23px; width: 23px; border-radius: 23px; background: #439643; cursor: pointer; } input[type=range]:focus::-ms-fill-lower { background: #205928; } input[type=range]:focus::-ms-fill-upper { background: #205928; } </style></head><body>";
P(Page_end) = "</body></html>";
P(form_start) = "<div style=\"text-align:left;display:inline-block;min-width:340px;\"><div style=\"text-align:center;\"><noscript>JavaScript aktivieren diese Seit benutzen zu können<br/></noscript><h2>Türklingel</h2></div><fieldset><legend><b>&nbsp;Einstellungen&nbsp;</b></legend><form action=\"index.html\" oninput=\"x.value=parseInt(volume.value)\" method=\"POST\">";
P(form_end) = "<input type=\"submit\" value=\"Speichern\" class=\"button bgrn\"></form></fieldset></div>";
P(form_field_melody_start) ="<label for=\"melody\">Melodie:</label><select id=\"melody\" name=\"melody_nr\">";
P(form_field_melody_end) ="</select><p/>";
P(form_field_volume_start) ="<label for=\"volume\">Lautstärke (<output name=\"x\" for=\"volume\">";
P(form_field_volume_end) ="</output>):</label><input id=\"volume\" name=\"volume\" type=\"range\" min=\"0\" max=\"30\" ";
P(form_field_silence_begin) ="<label for=\"silence_begin\">Ruhezeit Start:</label><input id=\"silence_begin\" type=\"time\" value=\"22:00\"/><p/>";
P(form_field_silence_end) ="<label for=\"silence_end\">Ruhezeit Ende:</label><input id=\"silence_end\" type=\"time\" value=\"06:00\"/><p/>";
P(form_field_debug_unchecked) ="<label for=\"debug\">Debug:</label><input id=\"debug\" type=\"checkbox\"/><p/>";
P(form_field_debug_checked) ="<label for=\"debug\">Debug:</label><input id=\"debug\" type=\"checkbox\" checked /><p/>";

// Aus dem Beispiel:
P(Get_head) = "<h1>GET from ";
P(Post_head) = "<h1>POST to ";
P(Unknown_head) = "<h1>UNKNOWN request for ";
P(Default_head) = "unidentified URL requested.</h1><br>\n";
P(Raw_head) = "raw.html requested.</h1><br>\n";
P(Parsed_head) = "parsed.html requested.</h1><br>\n";
P(Good_tail_begin) = "URL tail = '";
P(Bad_tail_begin) = "INCOMPLETE URL tail = '";
P(Tail_end) = "'<br>\n";
P(Parsed_tail_begin) = "URL parameters:<br>\n";
P(Parsed_item_separator) = " = '";
P(Params_end) = "End of parameters<br>\n";
P(Post_params_begin) = "Parameters sent by POST:<br>\n";
P(Line_break) = "<br>\n";


/* This creates an instance of the webserver.  By specifying a prefix
 * of "", all pages will be at the root of the server. */
#define PREFIX ""
WebServer webserver(PREFIX, 80);

/* commands are functions that get called by the webserver framework
 * they can read any posted data from client, and they output to the
 * server to send data back to the web browser. */
void helloCmd(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete)
{
  /* this line sends the standard "we're all OK" headers back to the
     browser */
  server.httpSuccess();

  /* if we're handling a GET or POST, we can output our data here.
     For a HEAD request, we just stop after outputting headers. */
  if (type == WebServer::HEAD)
    return;

  server.printP(Page_start);

  switch (type)
    {
    case WebServer::GET:
        break;
    case WebServer::POST:
        break;
    default:
        ;
    }

    server.printP(form_start);
    server.printP(form_field_melody_start);

    for(int i=1; i < myDFPlayer.readFileCounts(); i++) {
        if (i == current_melody) {
            server.printf("<option selected>%d</option>",i);
        } else {
            server.printf("<option>%d</option>",i);
        }
    }
    server.printP(form_field_melody_end);
    server.printP(form_field_volume_start);
    server.printf("%d",current_volume);
    server.printP(form_field_volume_end);
    server.printf("value=\"%d\"/>",current_volume);
    server.printP(form_field_silence_begin);
    server.printP(form_field_silence_end);
    if (debug) {
        server.printP(form_field_debug_checked);
    } else {
        server.printP(form_field_debug_checked);
    }
    server.printP(form_end);
    server.printP(Page_end);

}

void rawCmd(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete)
{
  /* this line sends the standard "we're all OK" headers back to the
     browser */
  server.httpSuccess();

  /* if we're handling a GET or POST, we can output our data here.
     For a HEAD request, we just stop after outputting headers. */
  if (type == WebServer::HEAD)
    return;

  server.printP(Page_start);
  switch (type)
    {
    case WebServer::GET:
        server.printP(Get_head);
        break;
    case WebServer::POST:
        server.printP(Post_head);
        break;
    default:
        server.printP(Unknown_head);
    }

    server.printP(Raw_head);
    server.printP(tail_complete ? Good_tail_begin : Bad_tail_begin);
    server.print(url_tail);
    server.printP(Tail_end);
    server.printP(Page_end);

}

#define NAMELEN 32
#define VALUELEN 32

void parsedCmd(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete)
{
  URLPARAM_RESULT rc;
  char name[NAMELEN];
  char value[VALUELEN];

  /* this line sends the standard "we're all OK" headers back to the
     browser */
  server.httpSuccess();

  /* if we're handling a GET or POST, we can output our data here.
     For a HEAD request, we just stop after outputting headers. */
  if (type == WebServer::HEAD)
    return;

  server.printP(Page_start);
  switch (type)
    {
    case WebServer::GET:
        server.printP(Get_head);
        break;
    case WebServer::POST:
        server.printP(Post_head);
        break;
    default:
        server.printP(Unknown_head);
    }

    server.printP(Parsed_head);
    server.printP(tail_complete ? Good_tail_begin : Bad_tail_begin);
    server.print(url_tail);
    server.printP(Tail_end);

  if (strlen(url_tail))
    {
    server.printP(Parsed_tail_begin);
    while (strlen(url_tail))
      {
      rc = server.nextURLparam(&url_tail, name, NAMELEN, value, VALUELEN);
      if (rc == URLPARAM_EOS)
        server.printP(Params_end);
       else
        {
        server.print(name);
        server.printP(Parsed_item_separator);
        server.print(value);
        server.printP(Tail_end);
        }
      }
    }
  if (type == WebServer::POST)
  {
    server.printP(Post_params_begin);
    while (server.readPOSTparam(name, NAMELEN, value, VALUELEN))
    {
      server.print(name);
      server.printP(Parsed_item_separator);
      server.print(value);
      server.printP(Tail_end);
    }
  }
  server.printP(Page_end);

}

void my_failCmd(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete)
{
  /* this line sends the "HTTP 400 - Bad Request" headers back to the
     browser */
  server.httpFail();

  /* if we're handling a GET or POST, we can output our data here.
     For a HEAD request, we just stop after outputting headers. */
  if (type == WebServer::HEAD)
    return;

  server.printP(Page_start);
  switch (type)
    {
    case WebServer::GET:
        server.printP(Get_head);
        break;
    case WebServer::POST:
        server.printP(Post_head);
        break;
    default:
        server.printP(Unknown_head);
    }

    server.printP(Default_head);
    server.printP(tail_complete ? Good_tail_begin : Bad_tail_begin);
    server.print(url_tail);
    server.printP(Tail_end);
    server.printP(Page_end);

}

// MQTT Callback function:
//
void mqtt_callback(char *topic, byte *payload, unsigned int length)
{
    String myTopic = String(topic);

    bool stateChanged = false;
    char *myPayload = NULL;
    int int_payload = 0;

    myPayload = (char *)malloc(length + 1);

    memcpy(myPayload, payload, length);
    myPayload[length] = 0;

    if (!client.isConnected())
    {
        client.connect(myID, MQTT_USER, MQTT_PASSWORD);
    }

    client.publish("/" + myID + "/state/LastPayload", String(myPayload));

    if (myTopic == "/"+myID+"/set/Volume") {
        int_payload = atoi(myPayload);
        if (int_payload > 0 && int_payload < 30) {
            current_volume = int_payload;
        }
        stateChanged = true;
    }

    if (myTopic == "/"+myID+"/set/melody") {
        int_payload = atoi(myPayload);
        if (int_payload > 0 && int_payload <= myDFPlayer.readFileCounts()) {
            current_melody = int_payload;
        }
        stateChanged = true;
    }

    if (stateChanged)
    {
        publishState();
    }

    free(myPayload);
    myPayload = NULL;
}

// This will publish the current state of the device to MQTT:
//
void publishState()
{

    if (!client.isConnected())
    {
        client.connect(myID, MQTT_USER, MQTT_PASSWORD);
    }

    if (client.isConnected())
    {
        client.publish("/" + myID + "/state/FirmwareVersion", System.version());
        client.publish("/" + myID + "/state/CurrentMelody", String(current_melody));
        client.publish("/" + myID + "/state/CurrentVolume", String(current_volume));
        client.publish("/" + myID + "/state/Debug", debug ? "True" : "False");
        client.publish("/" + myID + "/state/VisitorDate", lastVisitDate);
      }
}

// Publish current date+time when someone pushed the button:
//
void publishButtonPushed()
{

    lastVisitDate = Time.format(Time.now(), TIME_FORMAT_ISO8601_FULL);

    if (!client.isConnected())
    {
        client.connect(myID, MQTT_USER, MQTT_PASSWORD);
    }

    if (client.isConnected())
    {
        client.publish("/" + myID + "/state/Visitor", "True");
        client.publish("/" + myID + "/state/VisitorDate", lastVisitDate);
    }
}

void loadSettings()
{

    uint16_t maxSize = EEPROM.length();
    uint16_t address = 1;
    uint8_t version = 0;
    char loadArray[2048];

    EEPROM.get(address++, version);

    if (version == SETTINGS_MAGIC)
    { // Valid settings in EEPROM?
        EEPROM.get(address, debug);
        address = address + sizeof (debug);
        EEPROM.get(address, current_melody);
        address = address + sizeof (current_melody);
        EEPROM.get(address, current_volume);
        address = address + sizeof (current_volume);
        if (debug) { Particle.publish("DEBUG: Loaded settings.", PRIVATE); }
    } else {
        if (debug) { Particle.publish("DEBUG: Resetted to default settings.", PRIVATE); }
        debug = true;
        current_melody = 2;
        current_volume = 30;
        saveSettings();
    }
}

void saveSettings()
{

    uint16_t address = SETTINGS_START;
    uint16_t maxlength = EEPROM.length();
    char saveArray[2048];

    if (debug) { Particle.publish("DEBUG: Saving settings.", PRIVATE); }

    EEPROM.write(address++, SETTINGS_MAGIC);
    EEPROM.put(address, debug);
    address = address + sizeof(debug);
    EEPROM.put(address, current_melody);
    address = address + sizeof(current_melody);
    EEPROM.put(address, current_volume);
    address = address + sizeof(current_volume);
}

// Play the current configured melody:
//
void playMelody()
{
    if (myDFPlayer.readState() != 513)
    {
        myDFPlayer.play(current_melody);
        publishButtonPushed();
    }
}

void setup()
{
    // Switch off onboard RGB-LED:
    RGB.control(true);
    RGB.brightness(0);
    
    Serial1.begin(9600);
    delay(2000);

    loadSettings();

    if (debug) { Particle.publish("DEBUG: Initializing DFPlayer ... (May take 3~5 seconds)", PRIVATE); }

    if (!myDFPlayer.begin(Serial1)) { //Use softwareSerial to communicate with mp3.
        if (debug) { Particle.publish("DEBUG: Unable to begin:", PRIVATE); }
        if (debug) { Particle.publish("DEBUG: 1.Please recheck the connection!", PRIVATE); }
        if (debug) { Particle.publish("DEBUG: 2.Please insert the SD card!", PRIVATE); }
    } else {
        if (debug) { Particle.publish("DEBUG: DFPlayer Mini online.", PRIVATE); }
    }

    //----Set serial communictaion time out 500ms
    myDFPlayer.setTimeOut(500);

    //----Set volume----
    myDFPlayer.volume(current_volume); //Set volume value (0~30).

    //----Set different EQ----
    //myDFPlayer.EQ(DFPLAYER_EQ_NORMAL);
    myDFPlayer.EQ(DFPLAYER_EQ_POP);

    //----Set device we use SD as default----
    myDFPlayer.outputDevice(DFPLAYER_DEVICE_SD);

    //----Init Pushbutton pin----
    pinMode(PIN_BTN, INPUT_PULLUP);

    //----Time and date settings----
    Particle.syncTime();
    Time.zone(+1);
    beginning.hour = 2;
    beginning.day = DST::days::sun;
    beginning.month = DST::months::mar;
    beginning.occurrence = -1;

    end.hour = 3;
    end.day = DST::days::sun;
    end.month = DST::months::oct;
    end.occurrence = -1;

    dst.begin(beginning, end, 1);
    dst.check();
    dst.automatic(true);

    client.connect(System.deviceID(), MQTT_USER, MQTT_PASSWORD); // uid:pwd based authentication
    if (client.isConnected())
    {
        PublisherTimer.start();
        client.subscribe("/" + System.deviceID() + "/set/+");
    }

    // Attach interrupt to input pin of push-button:
    attachInterrupt(D5,playMelody,RISING);

    // Init webserver:
    //
    /* setup our default command that will be run when the user accesses
     * the root page on the server */
    webserver.setDefaultCommand(&helloCmd);

    /* setup our default command that will be run when the user accesses
     * a page NOT on the server */
    webserver.setFailureCommand(&my_failCmd);

    /* run the same command if you try to load /index.html, a common
     * default page name */
    webserver.addCommand("index.html", &helloCmd);

    /*This command  is called if you try to load /raw.html */
    webserver.addCommand("raw.html", &rawCmd);
    webserver.addCommand("parsed.html", &parsedCmd);

    /* start the webserver */
    webserver.begin();
}

void loop()
{
    char buff[64];
    int len = 64;

    button_pressed = digitalRead(PIN_BTN);
    if (button_pressed == LOW)
    {
        if (debug) { Particle.publish("DEBUG: Button pushed!", PRIVATE); }
        button_pressed = HIGH;
        playMelody();
    }
    delay(200);

  /* process incoming connections one at a time forever */
  webserver.processConnection(buff, &len);}
