/**
   Antenna rotator (azimut und elevation) with ESP8266 (NodeMCU)

   The Antenna rotor can be controlled by Web interface (port 80) or hamlib rotor interface TCP (port 4533).
   Access the web interface with /rotator?AZ=180&EL=20
   You can use Gpredict software to control the rotator via hamlib TCP interface.
   Create file "wifi_credentials.h" to set ssid and password.
   If wifi connection cannot be established a hotspot will be created with static ip address 192.168.177.1

   Libraries:
   ESP8266 https://github.com/esp8266/Arduino
   AccelStepper https://www.airspayce.com/mikem/arduino/AccelStepper/
   ESPAsyncWebServer https://github.com/me-no-dev/ESPAsyncWebServer
   ESPAsyncTCP https://github.com/me-no-dev/ESPAsyncTCP

   Hardware:
   2 28BYJ-48 geared stepper motors
   2 stepper motor driver ULN2003 or A4988
   1 adjustable StepDown converter from 12V to 8V
   1 wooden case as enclosure for the rotator
   4 ball bearings MF-115-ZZ
   2 gear wheels for stepper 28BYJ-48 ->  motor_gear.stl
   1 gear wheel for azimuth           ->  azimut-gear.stl
   1 gear wheel for elevation         ->  elevation-gear.stl
   1 sleeve bearing                   ->  mast_mount.stl

   all gears DIN 780 m=1
*/

#include "ESPAsyncWebServer.h"
#include "wifi_credentials.h"
#include <AccelStepper.h>
//const char* ssid     = ""; //set by wifi_credentials.h
//const char* password = ""; //set by wifi_credentials.h
const char* hostname = "rotator";

const int STEPPER_MAX_SPEED = 250;
const int STEPPER_ACCELERATION = 20;

const float AZIMUTH_STEPS_PER_DEGREE = 2048.0 * 3.0 / 360.0; //28BYJ-48 has 2048 steps for one turn * 3 for an addtional gear / 360 degress
const int AZIMUTH_MAX = 360;
const int AZIMUTH_MIN = 0;
const int DIR_PIN = 12;
const int STEP_PIN = 14;
const int ENABLE_PIN = 13;
AccelStepper azimuthStepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN); // A4988 driver
int azimuth = 0;
int lastAzimuth = 0;

const float ELEVATION_STEPS_PER_DEGREE = 2048.0 * 7.8 / 360.0; //28BYJ-48 has 2048 steps for one turn * 5 for an addtional gear / 360 degress
const int ELEVATION_MAX = 70;
const int ELEVATION_MIN = 0;
const int PIN_IN1 = 16;
const int PIN_IN2 = 5;
const int PIN_IN3 = 4;
const int PIN_IN4 = 0;
AccelStepper elevationStepper = AccelStepper(AccelStepper::FULL4WIRE, PIN_IN1, PIN_IN3, PIN_IN2, PIN_IN4); // ULN2003 driver
int elevation = 0;
int lastElevation = 0;

const int TCP_PORT = 4533; // default hamlib rotator port

void setup() {
  Serial.begin(115200);

  // setup stepper motors
  azimuthStepper.setEnablePin(ENABLE_PIN);
  azimuthStepper.setPinsInverted(false, false, true); // enablePin inverted
  azimuthStepper.setMaxSpeed(STEPPER_MAX_SPEED);
  azimuthStepper.setAcceleration(STEPPER_ACCELERATION);

  elevationStepper.setPinsInverted(false, false, true);
  elevationStepper.setMaxSpeed(STEPPER_MAX_SPEED);
  elevationStepper.setAcceleration(STEPPER_ACCELERATION);

  // Setup wifi
  WiFi.setHostname(hostname);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED && millis() < 30000) {
    delay(1000);
    Serial.print(".");
  }
  Serial.print("IP address: "); Serial.println(WiFi.localIP());
  Serial.print("Netmask: "); Serial.println(WiFi.subnetMask());
  Serial.print("Gateway: "); Serial.println(WiFi.gatewayIP());
  if (WiFi.status() != WL_CONNECTED) {
    IPAddress ip(192, 168, 177, 1);
    IPAddress gateway(192, 168, 177, 1);
    IPAddress subnet(255, 255, 255, 0);
    Serial.print("Setting soft-AP configuration "); Serial.println(WiFi.softAPConfig(ip, gateway, subnet) ? "Ready" : "Failed!");
    Serial.print("Setting soft-AP "); Serial.println(WiFi.softAP(hostname) ? "Ready" : "Failed!");
    Serial.print("Soft-AP IP address = "); Serial.println(WiFi.softAPIP());
  }

  // setup webserver
  AsyncWebServer* webServer = new AsyncWebServer(80);
  webServer->on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    if (request->hasParam("AZ")) {
      String paramValue = request->getParam("AZ")->value();
      setAzimuth(paramValue.toInt());
    }
    if (request->hasParam("EL")) {
      String paramValue = request->getParam("EL")->value();
      setElevation(paramValue.toInt());
    }

    String html = "<html><script>let azimuth = ";
    html += azimuth;
    html += "; let elevation = ";
    html += elevation;
    html += "; </script>\
<head><title>Wifi rotator</title></head>\
<body>\
    <h2>Hello from Wifi rotator</h2>\
    <div id=\"compass\">\
        <div id=\"az\">\
            <svg height=\"100\" width=\"100\">\
                <circle cx=\"50\" cy=\"50\" r=\"49\" stroke=\"blue\" stroke-width=\"3\" fill=\"none\" />\
                <line x1=\"10\" y1=\"10\" x2=\"80\" y2=\"80\" style=\"stroke:rgb(255,0,0);stroke-width:2\" />\
                <polygon points=\"26,10 10,10 10,26\" style=\"fill:rgb(255, 225, 0);stroke:rgb(255, 0, 0);stroke-width:2\" />\
            </svg>\
        </div>\
        <svg height=\"14\" width=\"14\" style=\"transform:  translate(44px, -106px);\"><text x=\"0\" y=\"11\" fill=\"red\">N</text></svg>\
        <svg height=\"14\" width=\"14\" style=\"transform:  translate(74px, -56px);\"><text x=\"0\" y=\"11\" fill=\"red\">E</text></svg>\
        <svg height=\"14\" width=\"14\" style=\"transform:  translate(8px, -8px);\"><text x=\"0\" y=\"11\" fill=\"red\">S</text></svg>\
        <svg height=\"14\" width=\"14\" style=\"transform:  translate(-60px, -56px);\"><text x=\"0\" y=\"11\" fill=\"red\">W</text></svg>\
    </div>\
    <script>\
        var div = document.getElementById('az');\
        div.style.width = '100px'; div.style.height = '100px'; div.style.transform = 'rotate(' + (45 + azimuth) + 'deg)';\
        document.write(\"AZ=\" + azimuth + \" EL=\" + elevation);\
    </script>\
    <p>\
        <button onclick=\"window.location.href = 'http://rotator?AZ=' + (azimuth - 10) ;\">AZ-10</button>\
        <button onclick=\"window.location.href = 'http://rotator?AZ=' + (azimuth - 5) ;\">AZ-5</button>\
        <button onclick=\"window.location.href = 'http://rotator?AZ=' + (azimuth + 5) ;\">AZ+5</button>\
        <button onclick=\"window.location.href = 'http://rotator?AZ=' + (azimuth + 10) ;\">AZ+10</button>&nbsp;&nbsp;\
        <button onclick=\"window.location.href = 'http://rotator?EL=' + (elevation - 5) ;\">EL-5</button>\
        <button onclick=\"window.location.href = 'http://rotator?EL=' + (elevation + 5) ;\">EL+5</button>\
    </p>\
    <p>\
        <button onclick=\"window.location.href = 'http://rotator?AZ=0'   ;\">N</button>\
        <button onclick=\"window.location.href = 'http://rotator?AZ=45'  ;\">NE</button>\
        <button onclick=\"window.location.href = 'http://rotator?AZ=90'  ;\">E</button>\
        <button onclick=\"window.location.href = 'http://rotator?AZ=135' ;\">SE</button>\
        <button onclick=\"window.location.href = 'http://rotator?AZ=180' ;\">S</button>\
        <button onclick=\"window.location.href = 'http://rotator?AZ=225' ;\">SW</button>\
        <button onclick=\"window.location.href = 'http://rotator?AZ=270' ;\">W</button>\
        <button onclick=\"window.location.href = 'http://rotator?AZ=315' ;\">NW</button>\
        <button onclick=\"window.location.href = 'http://rotator?AZ=360' ;\">N</button>\
    </p>\
</body></html>";

    request->send(200, "text/html", html);
  });
  webServer->begin();

  //  setup tcp server
  AsyncServer* tcpServer = new AsyncServer(TCP_PORT); // start listening on tcp port 7050
  tcpServer->onClient(&handleNewTcpClient, tcpServer);
  tcpServer->begin();
}

static void handleNewTcpClient(void* arg, AsyncClient* client) {
  Serial.printf("\n new client has been connected to server, ip: %s", client->remoteIP().toString().c_str());
  client->onData(&handleTcpData, NULL);
  client->onError(&handleTcpError, NULL);
  client->onTimeout(&handleTcpTimeOut, NULL);
}
static void handleTcpError(void* arg, AsyncClient* client, int8_t error) {
  Serial.printf("\n connection error %s from client %s \n", client->errorToString(error), client->remoteIP().toString().c_str());
}
static void handleTcpData(void* arg, AsyncClient* client, void *data, size_t len) {
  if (client->space() > 32 && client->canSend()) {
    char reply[32];
    char str[32];
    sprintf(str, "%s", data);
    char firstChar = str[0];
    if ('p' == firstChar) { // request position
      sprintf(reply, "%s\n%s\nRPRT 0\n", readAzimuth(), readElevation());
    }
    if ('P' == firstChar) { // set new position
      String* dataStr = new String(str); // like "P 180,00 45,00" or "P 180.00 45.00"

      String positionStr = dataStr->substring(2);
      int idx = positionStr.indexOf(' ');

      String azimuthStr = positionStr.substring(0, idx);
      setAzimuth(azimuthStr.toInt());
      Serial.print("New azimuth "); Serial.print(azimuth);

      String elevationStr = positionStr.substring(idx + 1);
      setElevation(elevationStr.toInt());
      Serial.print(", new elevation "); Serial.println(elevation);
      sprintf(reply, "\nRPRT 0\n");
    }
    client->add(reply, strlen(reply));
    client->send();
  }
}
static void handleTcpTimeOut(void* arg, AsyncClient* client, uint32_t time) {
  Serial.printf("\n client ACK timeout ip: %s \n", client->remoteIP().toString().c_str());
}

void loop() {
  if (lastAzimuth != azimuth) {
    lastAzimuth = azimuth;
    moveAzimuthStepper();
  }
  if (lastElevation != elevation) {
    lastElevation = elevation;
    moveElevationStepper();
  }
  delay(100);
}

void setAzimuth(int az) {
  if (az < AZIMUTH_MIN) {
    az = AZIMUTH_MIN;
  }
  if (az > AZIMUTH_MAX) {
    az = AZIMUTH_MAX;
  }
  azimuth = az;
}
String readAzimuth() {
  return String((int)azimuthStepper.currentPosition() / AZIMUTH_STEPS_PER_DEGREE);
}
void moveAzimuthStepper() {
  azimuthStepper.enableOutputs();
  azimuthStepper.moveTo(AZIMUTH_STEPS_PER_DEGREE * azimuth);
  azimuthStepper.runToPosition();
  azimuthStepper.disableOutputs();
}

void setElevation(int el) {
  if (el < ELEVATION_MIN) {
    el = ELEVATION_MIN;
  }
  if (el > ELEVATION_MAX) {
    el = ELEVATION_MAX;
  }
  elevation = el;
}
String readElevation() {
  return String((int)elevationStepper.currentPosition() / ELEVATION_STEPS_PER_DEGREE);
}
void moveElevationStepper() {
  elevationStepper.enableOutputs();
  elevationStepper.moveTo(ELEVATION_STEPS_PER_DEGREE  * elevation);
  elevationStepper.runToPosition();
  elevationStepper.disableOutputs();
}
