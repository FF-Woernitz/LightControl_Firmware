/*
  To upload through terminal you can use: curl -u admin:1234 -F "image=@v2.ino.generic.bin" 10.0.110.115/update
*/
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <EEPROM.h>

#include <config.h>

/*  MODES:
0 = Normal On Off no blinking (Default)
1 = Normal On Off with blinking
2 = Impulse switch
*/

ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;

int state = LOW;
byte mode = 255;
unsigned long alarm = 0;

int blinkCount;
unsigned int blinkDelay;
unsigned int lastBlink;

bool needRestart = false;

void handleRoot();
void blinkHandler();
void alarmHandler();

void setup(void)
{

    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);

    Serial.begin(115200);
    Serial.println();
    Serial.println("Booting FFW Shelly 1...");

    const String chipid = String(ESP.getChipId(), HEX);
    Serial.print("Chip-ID: ");
    Serial.println(chipid);

    if (chipid == "c42cb0") mode = 2; //DEBUG
    if (chipid == "c492f6") mode = 2; //Fahrzeughalle
    if (chipid == "c419a0") mode = 0; //Flur
    if (chipid == "c4f1c0") mode = 1; // Außen Fahrzeughalle
    if (chipid == "c4994b") mode = 1; // Außen Gemeinde
    if (mode == 255)
    {
        Serial.println("No mode for this chip-ID set. Setting to 0");
        mode = 0;
    }

    Serial.print("Light mode: ");
    Serial.println(mode);

    WiFi.mode(WIFI_STA);
    WiFi.persistent(false);
    WiFi.setOutputPower(20.5);
    if (DEBUG)
    {
        WiFi.begin(ADMINSID, ADMINPSK);
    }
    else
    {
        WiFi.begin(FFWSID, FFWPSK);
    }
    while (WiFi.waitForConnectResult(15000) != WL_CONNECTED)
    {
        Serial.println("WiFi failed, retrying.");
        WiFi.begin(FFWSID, FFWPSK);
        if (WiFi.waitForConnectResult(15000) == WL_CONNECTED)
            break;
        Serial.println("WiFi failed, retrying.");
        WiFi.begin(FFWSID, FFWPSK);
        if (WiFi.waitForConnectResult(15000) == WL_CONNECTED)
            break;
        Serial.println("WiFi failed, retrying with admin WLAN.");
        WiFi.begin(ADMINSID, ADMINPSK);
    }
    Serial.println("WIFI connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());

    httpUpdater.setup(&server, UPDATE_PATH, ADMINUSER, ADMINPASS);
    server.begin();
    Serial.println("HTTPUpdateServer ready!");

    server.on("/", handleRoot);
    Serial.println("FFW Shelly 1 ready!");
}

void loop(void)
{
    server.handleClient();
    blinkHandler();
    alarmHandler();

    if (needRestart)
    {
        Serial.println("Restarting...");
        ESP.restart();
    }
}

bool checkInput()
{
    return !digitalRead(INPUT_PIN);
}

int getState()
{
    if (mode != 2)
    {
        return state;
    }
    else
    {
        return checkInput();
    }
}

void applyAction(int action)
{
    if (getState() != action)
    {
        if (mode != 2)
        {
            digitalWrite(RELAY_PIN, action);
            state = action;
        }
        else
        {
            digitalWrite(RELAY_PIN, HIGH);
            delay(500);
            digitalWrite(RELAY_PIN, LOW);
            state = LOW;
        }
        Serial.print("Switched ");
        Serial.println(action == HIGH ? "ON" : "OFF");
    }
}

void toggleAction()
{
    applyAction(getState() == HIGH ? LOW : HIGH);
}

void blinkHandler()
{
    if (blinkCount > 0)
    {
        if (millis() - lastBlink > blinkDelay)
        {
            blinkCount--;
            lastBlink = millis();
            toggleAction();
        }
    }
}

void blink(int bd, int bc)
{
    if (mode != 2)
    {
        toggleAction();
        lastBlink = millis();
        blinkCount = bc * 2 - 1;
        blinkDelay = bd;
    }
}

void stopBlink()
{
    if (blinkCount > 0)
    {
        lastBlink = millis();
        blinkCount = 0;
    }
}

void alarmHandler()
{
    if (alarm > 0 && alarm < millis())
    {
        if (mode == 0)
        {
            applyAction(LOW);
        }
        if (mode == 1)
        {
            applyAction(LOW);
            blink(BLINK_DELAY_DEFAULT, BLINK_COUNT_DEFAULT);
        }
        alarm = 0;
        Serial.println("Alarm off");
    }
}

/**
   Handle web requests to "/" path.
*/
void handleRoot()
{
    if (!server.authenticate(IOTUSER, IOTPASS))
    {
        server.requestAuthentication();
        return;
    }
    if (server.hasArg("action"))
    {
        String action = server.arg("action");
        if (action.equals("on"))
        {
            alarm = 0;
            stopBlink();
            applyAction(HIGH);
        }
        else if (action.equals("off"))
        {
            alarm = 0;
            stopBlink();
            applyAction(LOW);
        }
        else if (action.equals("toggle"))
        {
            alarm = 0;
            stopBlink();
            toggleAction();
        }
        else if (action.equals("blink"))
        {

            alarm = 0;
            stopBlink();
            int bd = BLINK_DELAY_DEFAULT;
            int bc = BLINK_COUNT_DEFAULT;
            if (server.hasArg("blinkDelay"))
            {
                bd = server.arg("blinkDelay").toInt();
            }
            if (server.hasArg("blinkCount"))
            {
                bc = server.arg("blinkCount").toInt();
            }
            blink(bd, bc);
        }
    }
    if (server.hasArg("alarm"))
    {
        unsigned int alarmTime = server.arg("alarm").toInt();
        if (alarmTime > 0)
        {
            Serial.println("Alarm on");
            alarm = millis() + (alarmTime * 1000);
            stopBlink();
            applyAction(HIGH);
        }
    }

    String s = "";
    if (!server.hasArg("json"))
    {

        s = F("<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>");
        s += "<title>FFW Shelly 1</title></head><body>";
        if (needRestart)
        {
            s += "<h1>Restarting...</h1>";
        }
        s += "FFW Shelly 1";
        s += "<div>Mode: ";
        s += mode;
        s += "<div>Relais: ";
        s += (state == HIGH ? "ON" : "OFF");
        s += "</div>";
        s += "<div>Alarm: ";
        s += (alarm > 0 ? "YES" : "NO");
        if(alarm > 0 ){
           s += "  Time left: " + String((alarm-millis())/1000); 
        }
        s += "</div>";
        if (mode != 2)
        {
            s += "<div>Blinking: ";
            s += (blinkCount > 0 ? "YES" : "NO");
            s += "</div>";
        }
        if (mode == 2)
        {
            s += "<div>Input: ";
            s += (checkInput() == HIGH ? "ON" : "OFF");
            s += "</div>";
        }
        s += "<div>";
        s += "<button type='button' onclick=\"location.href='?action=on';\" >Turn ON</button>";
        s += "<button type='button' onclick=\"location.href='?action=off';\" >Turn OFF</button>";
        s += "<button type='button' onclick=\"location.href='?action=blink';\" >Blink</button>";
        s += "<button type='button' onclick=\"location.href='?';\" >Refresh</button>";
        s += "</div>";
        s += "<div>Go to <a href='update'>update</a> for a firmware update.</div>";
        s += "</body></html>\n";
    }
    else
    {
        s = F("{\"relais\": ");
        s += (state == HIGH ? "true" : "false");
        s += ", \"input\": ";
        s += (checkInput() == HIGH ? "true" : "false");
        s += ", \"blinking\": ";
        s += (blinkCount > 0 ? "true" : "false");
        s += ", \"mode\": ";
        s += mode;
        s += "}";
    }

    server.send(200, "text/html", s);
}
