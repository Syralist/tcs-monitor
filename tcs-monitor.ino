#include <FS.h>                   //this needs to be first, or it all crashes and burns...

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager

#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson

#include <PubSubClient.h>

// #define HASBUTTON
#ifdef HASBUTTON
#include <PMButton.h>
PMButton button(D3);
#endif

WiFiClient net;
PubSubClient client(net);

//define your default values here, if there are different values in config.json, they are overwritten.
char mqtt_server[40];
char mqtt_port[6] = "8080";
char mqtt_topic[34] = "home/flur/klingel";
#ifdef HASBUTTON
char mqtt_button[50] = "home/flur/klingel/button";
#endif
char mqtt_client[16] = "klingel";
char mqtt_user[16] = "user";
char mqtt_pass[16] = "pass";

//flag for saving data
bool shouldSaveConfig = false;


void connectToWifi() {
  Serial.print("checking wifi...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }

//   connectToMqtt();

  Serial.println("\nconnected!");
}

void connectToMqtt() {
  Serial.print("\nconnecting to MQTT...");
  while (!client.connect(mqtt_client, mqtt_user, mqtt_pass)) {
    Serial.print(".");
    delay(5000);
  }
}

void sendMessage() {
    connectToMqtt();
    client.publish(mqtt_topic, "triggered");
    delay(100);
    client.disconnect();
}

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

const int analogIn = A0;

int sensorValue = 0;
float voltageDivider = 0;
float actualVoltage = 0;

bool zustand = false;

unsigned long microsFlanke = 0;

const float threshold = 20.0;

const int sequenzLaenge = 43;
const unsigned long sequenz[sequenzLaenge] = {
        6000,
        4000,
        2000,
        2000,
        2000,
        2000,
        4000,
        4000,
        4000,
        2000,
        2000,
        2000,
        4000,
        2000,
        2000,
        2000,
        4000,
        2000,
        4000,
        4000,
        4000,
        2000,
        2000,
        2000,
        4000,
        2000,
        4000,
        2000,
        2000,
        2000,
        2000,
        2000,
        2000,
        2000,
        4000,
        5000,
        6000,
        2000,
        2000,
        2000,
        2000,
        4000,
        2000
    };
const unsigned long jitter = 400;
int sequenzZaehler = 0;
bool sequenzLaeuft = false;

//-
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on the LED if an 1 was received as first character
//   if ((char)payload[0] == '1') {
//     digitalWrite(BUILTIN_LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
    // but actually the LED is on; this is because
    // it is active low on the ESP-01)
//   } else {
//     digitalWrite(BUILTIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
//   }

}

void setup() {
    // initialize serial communication at 115200
    Serial.begin(115200);
    //clean FS, for testing
    // SPIFFS.format();

    //read configuration from FS json
    Serial.println("mounting FS...");

    if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
        //file exists, reading and loading
        Serial.println("reading config file");
        File configFile = SPIFFS.open("/config.json", "r");
        if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonDocument json(1024);
        DeserializationError error = deserializeJson(json, buf.get());
        serializeJson(json, Serial);
        if (!error) {
            Serial.println("\nparsed json");

            strcpy(mqtt_server, json["mqtt_server"]);
            strcpy(mqtt_port, json["mqtt_port"]);
            strcpy(mqtt_topic, json["mqtt_topic"]);
    #ifdef HASBUTTON
            strcpy(mqtt_button, json["mqtt_topic"]);
            sprintf(mqtt_button, "%s%s", mqtt_topic, "/button");
    #endif
            strcpy(mqtt_client, json["mqtt_client"]);
            strcpy(mqtt_user, json["mqtt_user"]);
            strcpy(mqtt_pass, json["mqtt_pass"]);

        } else {
            Serial.println("failed to load json config");
        }
        }
    }
    } else {
    Serial.println("failed to mount FS");
    }
    //end read
    
    // The extra parameters to be configured (can be either global or just in the setup)
    // After connecting, parameter.getValue() will get you the configured value
    // id/name placeholder/prompt default length
    WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
    WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 5);
    WiFiManagerParameter custom_mqtt_topic("topic", "mqtt topic", mqtt_topic, 32);
    WiFiManagerParameter custom_mqtt_client("client", "mqtt client", mqtt_client, 32);
    WiFiManagerParameter custom_mqtt_user("user", "mqtt user", mqtt_user, 32);
    WiFiManagerParameter custom_mqtt_pass("pass", "mqtt pass", mqtt_pass, 32);

    //WiFiManager
    //Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wifiManager;

    //set config save notify callback
    wifiManager.setSaveConfigCallback(saveConfigCallback);

    //set static ip
    // wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));

    //add all your parameters here
    wifiManager.addParameter(&custom_mqtt_server);
    wifiManager.addParameter(&custom_mqtt_port);
    wifiManager.addParameter(&custom_mqtt_topic);
    wifiManager.addParameter(&custom_mqtt_client);
    wifiManager.addParameter(&custom_mqtt_user);
    wifiManager.addParameter(&custom_mqtt_pass);

    //reset settings - for testing
    //wifiManager.resetSettings();

    //set minimu quality of signal so it ignores AP's under that quality
    //defaults to 8%
    //wifiManager.setMinimumSignalQuality();

    //sets timeout until configuration portal gets turned off
    //useful to make it all retry or go to sleep
    //in seconds
    //wifiManager.setTimeout(120);

    //fetches ssid and pass and tries to connect
    //if it does not connect it starts an access point with the specified name
    //here  "AutoConnectAP"
    //and goes into a blocking loop awaiting configuration
    if (!wifiManager.autoConnect("KlingelAP", "password")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
    }

    //if you get here you have connected to the WiFi
    Serial.println("connected...yeey :)");

    //read updated parameters
    strcpy(mqtt_server, custom_mqtt_server.getValue());
    strcpy(mqtt_port, custom_mqtt_port.getValue());
    strcpy(mqtt_topic, custom_mqtt_topic.getValue());
    #ifdef HASBUTTON
    strcpy(mqtt_button, custom_mqtt_topic.getValue());
    sprintf(mqtt_button, "%s%s", mqtt_topic, "/button");
    #endif
    strcpy(mqtt_client, custom_mqtt_client.getValue());
    strcpy(mqtt_user, custom_mqtt_user.getValue());
    strcpy(mqtt_pass, custom_mqtt_pass.getValue());

    //save the custom parameters to FS
    if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonDocument json(1024);
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;
    json["mqtt_topic"] = mqtt_topic;
    json["mqtt_client"] = mqtt_client;
    json["mqtt_user"] = mqtt_user;
    json["mqtt_pass"] = mqtt_pass;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
        Serial.println("failed to open config file for writing");
    }

    serializeJson(json, Serial);
    serializeJson(json, configFile);
    configFile.close();
    //end save
    }

    Serial.println("local ip");
    Serial.println(WiFi.localIP());

    Serial.print("MQTT Server ");
    Serial.println(mqtt_server);
    Serial.print("MQTT port ");
    Serial.println(atoi(mqtt_port));

    client.setServer(mqtt_server, atoi(mqtt_port));
    client.setCallback(callback);

    connectToWifi();

    #ifdef HASBUTTON
    button.begin();
    button.holdTime(5000);
    button.longHoldTime(8000);
    #endif

}

void loop() {

    // if (!client.connected()) {
    //     connectToWifi();
    // }

    // client.loop();
    // delay(10);  // <- fixes some issues with WiFi stability

    #ifdef HASBUTTON
        button.checkSwitch();
        if(button.clicked()) {
            client.publish(mqtt_button, "clicked");
        }
        if(button.held()) {
            client.publish(mqtt_button, "held");
        }
        if(button.heldLong()) {
            client.publish(mqtt_button, "heldLong");
        }
    #endif
    sensorValue = analogRead(analogIn);
    voltageDivider = map(sensorValue, 0, 1023, 0, 320) / 100.0;
    actualVoltage = voltageDivider * 24.0 / 2.2;
    // Serial.print("Digits: ");
    // Serial.print(sensorValue);
    // Serial.print(", Divider: ");
    // Serial.print(voltageDivider);
    // Serial.print(", Actual: ");
    // Serial.println(actualVoltage);
    // delay(500);
    unsigned long microsJetzt = micros();
    unsigned long microsSeitLetzterFlanke = microsJetzt - microsFlanke;
    if(actualVoltage < threshold && zustand == false)
    {
        //Spannung sinkt -> positive Flanke
        if(!sequenzLaeuft) {
            // Sequenz startet bei positiver Flanke
            // erste Zeit ist die darauf folgende negative Flanke
            sequenzLaeuft = true;
        }
        else {
            if( abs(microsSeitLetzterFlanke - sequenz[sequenzZaehler]) < jitter ) {
                sequenzZaehler++;
            }
            else {
                sequenzLaeuft = false;
                sequenzZaehler = 0;
            }
        }
        Serial.print("positive Flanke nach ");
        Serial.print(microsSeitLetzterFlanke);
        Serial.println("us");
        zustand = true;
        microsFlanke = microsJetzt;
    }
    if(actualVoltage >= threshold && zustand == true)
    {
        //Spannung steigt -> negative Flanke
        if(sequenzLaeuft) {
            if( abs(microsSeitLetzterFlanke - sequenz[sequenzZaehler]) < jitter ) {
                sequenzZaehler++;
            }
            else {
                sequenzLaeuft = false;
                sequenzZaehler = 0;
            }
        }
        Serial.print("negative Flanke nach ");
        Serial.print(microsSeitLetzterFlanke);
        Serial.println("us");
        zustand = false;
        microsFlanke = microsJetzt;
    }
    if(sequenzLaeuft && sequenzZaehler >= sequenzLaenge) {
        Serial.println("Sequenz vollständig!");
        sendMessage();
        // client.publish(mqtt_topic, "triggered");
        sequenzLaeuft = false;
        sequenzZaehler = 0;
    }
}