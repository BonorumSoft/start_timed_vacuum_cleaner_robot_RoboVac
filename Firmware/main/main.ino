/*
    Author  : Jan Buhlrich
              Tjorben Eberle @TJ_ger
    Date    : Mai, 2018
    Project : Start timed Vacuum cleaner robot
    Desc    :
    Version : 1.1

    Company : Eufy
    Modell  : RoboVac
    Version : 11

    Hardware list:  - Wemos D1 mini
                    - IR LED (we used 940µm)
                    - Current limiting resistor (220 Ohm)

    circuit diagram:
    https://github.com/3DMech/start_timed_vacuum_cleaner_robot_RoboVac/tree/master/Media/Pictures

    Further project information:
    https://github.com/3DMech/start_timed_vacuum_cleaner_robot_RoboVac

    Common mistakes & tips:

*/

#ifndef UNIT_TEST
#include <Arduino.h>
#endif
#include <IRremoteESP8266.h>
#include <irsend.h>

#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
//MQTT
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
//Upload over the Air
#include <ArduinoOTA.h>

/*
  Local Headerfiles
*/
#include "pins.h"
#include "configuration.h"
#include "Eufy_RoboVac_11.h"

/*Initialized variables

*/
IPAddress timeServerIP; // time.nist.gov NTP server address

byte packetBuffer[ NTP_PACKET_SIZE];

WiFiUDP udp;

//MQTT
void callback(char* topic, byte* payload, unsigned int length);
WiFiClient wifiClient;
PubSubClient client(MQTT_SERVER, 1883, callback, wifiClient);


long currentmillis, previousmillis;
int16_t adc0, adc1;
long currenttime = 0, oldtime = 0;
int hour = 0, second = 0, number = 0;
boolean stoptime = true;
bool web_left = 0, web_right = 0, web_straight = 0, web_back = 0, web_auto = 0;
uint8_t MAC_array[6];
char MAC_char[18];
String msg, tmp;


/*
  Setup
*/

void setup() {
  irsend.begin();
  Serial.begin(115200, SERIAL_8N1, SERIAL_TX_ONLY);

  //Print mac adress first
  Serial.println("start_timed_vacuum_cleaner_robot_RoboVac");
  Serial.println("Mac adress:");
  WiFi.macAddress(MAC_array);
  for (int i = 0; i < sizeof(MAC_array); ++i) {
    sprintf(MAC_char, "%s%02x:", MAC_char, MAC_array[i]);
  }
  Serial.println(MAC_char); //- See more at: http://www.esp8266.com/viewtopic.php?f=29&t=3587#sthash.hV7FUT1J.dpuf

  WiFi.begin(ssid, password);

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());


  server.on("/", handleRoot);
  server.on("/ir", handleIr);
  server.on("/inline", []() {
    server.send(200, "text/plain", "this works without need of authentification");
  });


  server.onNotFound(handleNotFound);
  const char * headerkeys[] = {"User-Agent", "Cookie"} ;
  size_t headerkeyssize = sizeof(headerkeys) / sizeof(char*);
  server.collectHeaders(headerkeys, headerkeyssize );
  server.begin();
  Serial.println("HTTP server started");

  Serial.println("Starting UDP");
  udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(udp.localPort());
}

void loop() {
  client.loop(); //maintain MQTT connection
  server.handleClient();
  ArduinoOTA.handle();
  currentmillis = millis();
  currenttime = millis();
  if ((currenttime - oldtime) > 30000) {
    ask_for_Time();
    oldtime = currenttime;
    if (hour == 1 && stoptime) {
      number++;//count the cleaned days

      sende_auto();
      if (number > intensive_cleaning_frequency) {
        intensivprogramm();
        number = 0;
      }
      stoptime == false;
    }

    if (hour == 3) {
      stoptime == true;
    }
  }

  if (currentmillis - previousmillis > 100) {
    previousmillis = millis();
  }
}

void left() {
  if (adc1 > 20000 || web_left == 1 ) {
    irsend.sendRaw(left_raw, 167, 38);  // Send a raw data capture at 38kHz.
    Serial.println("left");
    web_left = 0;
  }
}
void right() {
  if (adc1 < 6000 || web_right == 1) {
    irsend.sendRaw(right_raw, 167, 38);  // Send a raw data capture at 38kHz.
    Serial.println("right");
    web_right = 0;
  }
}

void forth() {
  if (adc0 < 6000 || web_straight == 1) {
    irsend.sendRaw(straight_raw, 167, 38);  // Send a raw data capture at 38kHz.
    Serial.println("straight");
    web_straight = 0;
  }
}

void corner() {
  irsend.sendRaw(corner_raw, 167, 38);  // Send a raw data capture at 38kHz.
}


void back() {
  if (adc0 > 20000 || web_back == 1) {
    irsend.sendRaw(back_raw, 167, 38);  // Send a raw data capture at 38kHz.
    Serial.println("zurück");
    web_back = 0;
  }
}

void auto_clean() {
  //To ensure that the robot starts, the command is executed four times behind each other.
  for (int i = 0; i <= 3; i++) {
    irsend.sendRaw(automatic_raw, 419, 38);
    delay(1000);
  }
  Serial.println("auto");
  web_auto = 0;
}



void end_cleaning() {
  irsend.sendRaw(home_schleife_raw, 419, 38);
  Serial.println("auto");
  web_auto = 0;
}
//root page can be accessed only if authentification is ok
void handleRoot() {
  Serial.println("Enter handleRoot");
  String header;

  String content = "<html><H2>start timed vacuum cleaner robot</H2><br>";
  content += "<head><title>RoboVac</title></head>";
  content += "<body bgcolor=\"#d0d0F0\">";
  content += "<style type= \"text/css\">";
  content += ".tg  {border-collapse:collapse;border-spacing:0;}";
  content += ".tg td{font-family:Arial, sans-serif;font-size:14px;padding:10px 5px;border-style:solid;border-width:1px;overflow:hidden;word-break:normal;border-color:black;}";
  content += ".tg th{font-family:Arial, sans-serif;font-size:14px;font-weight:normal;padding:10px 5px;border-style:solid;border-width:1px;overflow:hidden;word-break:normal;border-color:black;}";
  content += ".tg .tg-cpu2{border-color:#000000;vertical-align:top}";
  content += "</style>";
  content += "<table class=\"tg\">";
  content += "  <tr>";
  content += "    <th class=\"tg-cpu2\"></th>";
  content += "    <th class=\"tg-cpu2\"><p><a href=\"ir?code=1\">straight</a></p></th>";
  content += "    <th class=\"tg-cpu2\"><p><a href=\"ir?code=5\">auto</a></p></th>";
  content += "  </tr>";
  content += "  <tr>";
  content += "    <td class=\"tg-cpu2\"><p><a href=\"ir?code=3\">left</a></p></td>";
  content += "    <td class=\"tg-cpu2\"><p><a href=\"ir?code=2\">back</a></p></td>";
  content += "    <td class=\"tg-cpu2\"><p><a href=\"ir?code=4\">right</a></p></td>";
  content += "  </tr>";
  content += "</table>";
  /* safe for later use
    content += "<p><a href=\"ir?code=1\">straight</a></p>";
    content += "<p><a href=\"ir?code=2\">back</a></p>";
    content += "<p><a href=\"ir?code=3\">left</a></p>";
    content += "<p><a href=\"ir?code=4\">right</a></p>";
    content += "<p><a href=\"ir?code=5\">auto</a></p>";
  */
  content += "</body></html>";
  server.send(200, "text/html", content);
}

void handleIr() {
  for (uint8_t i = 0; i < server.args(); i++) {
    if (server.argName(i) == "code") {
      uint32_t code = strtoul(server.arg(i).c_str(), NULL, 10);
      switch (code) {
        case 0:
          break;
        case 1:
          Serial.println("case 1 - straight");
          web_straight = 1;
          break;
        case 2:
          Serial.println("case 2 - back");
          web_back = 1;
          break;
        case 3:
          Serial.println("case 3- left");
          web_left = 1;
          break;
        case 4:
          Serial.println("case 4 - right");
          web_right = 1;
          break;
        case 5:
          Serial.println("case 5 - auto");
          web_auto = 1;
          sende_auto();
          break;
      }
    }
  }
  handleRoot();
}

//no need authentification
void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void sende_auto() {
  //Serial.println("auto");
  auto_clean();
}

void sende_MAX() {
  Serial.println("MAX");
  irsend.sendRaw(maximal, 419, 38);
}



void intensivprogramm(){
  while (hour != 3) {
    ask_for_Time();
  }
  for (int i = 0; i <= 3; i++)
  {
    sende_auto();
    delay(500);
    sende_MAX();
    delay(5000);
  }

  while (hour != 6) {
    ask_for_Time();
  }
  for (int i = 0; i <= 3; i++)
  {
    sende_auto();
    delay(500);
    sende_MAX();
    delay(5000);
  }
}

/*Communication with the NTP server and
   calculation of the time
*/
void ask_for_Time() {
  WiFi.hostByName(ntpServerName, timeServerIP);
  sendNTPpacket(timeServerIP);
  delay(1000);

  int content = udp.parsePacket();
  if (!content) {
    Serial.println("no packet yet");
  }
  else {
    Serial.println(content);
    udp.read(packetBuffer, NTP_PACKET_SIZE);
    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);

    unsigned long epoch = highWord << 16 | lowWord - 2208988800UL;

    hour = ((epoch  % 86400L) / 3600);
    Serial.print((epoch  % 86400L) / 3600); // print the hour (86400 equals secs per day)
    Serial.print(':');
    if ( ((epoch % 3600) / 60) < 10 ) {
      // In the first 10 minutes of each hour, we'll want a leading '0'
      Serial.print('0');
    }
    Serial.print((epoch  % 3600) / 60); // print the minute (3600 equals secs per minute)
    Serial.print(':');
    if ( (epoch % 60) < 10 ) {
      Serial.print('0');
    }
    second = epoch % 60;
    Serial.println(epoch % 60); // print the second
  }
  delay(10000);
}



unsigned long sendNTPpacket(IPAddress& address)
{
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  packetBuffer[0] = 0b11100011;
  packetBuffer[1] = 0;
  packetBuffer[2] = 6;
  packetBuffer[3] = 0xEC;
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  udp.beginPacket(address, 123);
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}

void callback(char* topic, byte* payload, unsigned int length) {

  //convert topic to string to make it easier to work with
  String topicStr = topic;
  //EJ: Note:  the "topic" value gets overwritten everytime it receives confirmation (callback) message from MQTT

  //Print out some debugging info
  Serial.println("Callback update.");
  Serial.print("Topic: ");
  Serial.println(topicStr);

  if (topicStr == actionTopic11)
  {

    //turn the switch on if the payload is '1' and publish to the MQTT server a confirmation message
    if (payload[0] == '1') {
      sende_auto();
      client.publish(confirmTopic11, "1");
    }

    //turn the switch off if the payload is '0' and publish to the MQTT server a confirmation message
    else if (payload[0] == '0') {
      end_cleaning();
      client.publish(confirmTopic11, "0");
    }
  }

  // EJ: copy and paste this whole else-if block, should you need to control more switches
  else if (topicStr == actionTopic12)
  {
    //turn the switch on if the payload is '1' and publish to the MQTT server a confirmation message
    if (payload[0] == '1') {
      corner();
      client.publish(confirmTopic12, "1");
    }

    //turn the switch off if the payload is '0' and publish to the MQTT server a confirmation message
    else if (payload[0] == '0') {
      end_cleaning();
      client.publish(confirmTopic12, "0");
    }
  }
  else if (topicStr == actionTopic13)
  {
    //turn the switch on if the payload is '1' and publish to the MQTT server a confirmation message
    if (payload[0] == '1') {
      sende_MAX();
      client.publish(confirmTopic13, "1");
    }

    //turn the switch off if the payload is '0' and publish to the MQTT server a confirmation message
    else if (payload[0] == '0') {
      end_cleaning();
      client.publish(confirmTopic13, "0");
    }
  }
  else if (topicStr == actionTopic21)
  {
    //turn the switch on if the payload is '1' and publish to the MQTT server a confirmation message
    if (payload[0] == '1') {
      end_cleaning();
      client.publish(confirmTopic21, "1");
    }

    //turn the switch off if the payload is '0' and publish to the MQTT server a confirmation message
    else if (payload[0] == '0') {
      end_cleaning();
      client.publish(confirmTopic21, "0");
    }
  }

  //turn the switch off if the payload is '0' and publish to the MQTT server a confirmation message
  else if (payload[0] == '0') {
    end_cleaning();
    client.publish("/house/switchConfirm4/", "0");
  }
}

