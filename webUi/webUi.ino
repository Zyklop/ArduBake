#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <FS.h>

#define READ_SIZE 50
#define ROLLING_SIZE 1800
#define SEND_SIZE 500

ESP8266WebServer server(80);
float tcTopTemps[ROLLING_SIZE];
float ptTopTemps[ROLLING_SIZE];
float tcBottomTemps[ROLLING_SIZE];
float ptBottomTemps[ROLLING_SIZE];
int rollingCleaned = 0;
int secOffset = 0;
String phase;
String sendBuffer;

void readFromSerial()
{
    //sample: time:1;tT:1.0;pT:2.0;tB:3.0;pB:4.0;phase:idle

    char input[READ_SIZE + 1];
    byte size = Serial.readBytesUntil('\n', input, READ_SIZE);
    input[size] = 0;
    int time;

    char *type = strtok(input, ";");
    while (type != 0)
    {
        // Split the command in two values
        char *separator = strchr(type, ':');
        if (separator != 0)
        {
            // Actually split the string in 2: replace ':' with 0
            *separator = 0;
            ++separator;
            if (strstr(type, "time"))
            {
                time = atoi(separator);
                if (time > ROLLING_SIZE)
                {
                    time = time % ROLLING_SIZE;
                    while (rollingCleaned != time)
                    {
                        tcTopTemps[rollingCleaned] = 0;
                        ptTopTemps[rollingCleaned] = 0;
                        tcBottomTemps[rollingCleaned] = 0;
                        ptBottomTemps[rollingCleaned] = 0;
                        rollingCleaned = (rollingCleaned + 1) % ROLLING_SIZE;
                    }
                }
            }
            else if (strstr(type, "tT"))
            {
                tcTopTemps[time] = atof(separator);
            }
            else if (strstr(type, "pT"))
            {
                ptTopTemps[time] = atof(separator);
            }
            else if (strstr(type, "tB"))
            {
                tcBottomTemps[time] = atof(separator);
            }
            else if (strstr(type, "pB"))
            {
                ptBottomTemps[time] = atof(separator);
            }
            else if (strstr(type, "phase"))
            {
                phase = String(separator);
            }
        }
        // Find the next command in input string
        type = strtok(0, ";");
    }
}

void checkAndSend(bool force = 0)
{
    if (sendBuffer.length() >= SEND_SIZE || force)
    {
        server.sendContent(sendBuffer);
        sendBuffer = "";
    }
}

void sendStartSignal()
{
    Serial.println("Start");
    server.send(204, "", "");
}

void sendStopSignal()
{
    Serial.println("Stop");
    server.send(204, "", "");
}

void handleSettings()
{
    String message = "";
    for (int i = 0; i < server.args(); i++)
    {
        message += server.argName(i) + ":";      //Get the name of the parameter
        message += server.arg(i) + ";";          //Get the value of the parameter
    }
    Serial.println(message);
    server.send(200, "text/plain", "Message \"" + message + "\" sent");
}

void timeArrayToSendBuffer()
{
    sendBuffer += "[";
    bool started = 0;
    for (int i = 0; i < ROLLING_SIZE; i++)
    {
        if (tcTopTemps[i] != 0)
        {
            if (started)
            {
                sendBuffer += ",";
            }
            started = 1;
            sendBuffer += String(i + secOffset);
            checkAndSend();
        }
    }
    sendBuffer += "]";
}

void valueArrayToSendBuffer(float values[])
{
    sendBuffer += "[";
    bool started = 0;
    for (int i = 0; i < ROLLING_SIZE; i++)
    {
        if (tcTopTemps[i] != 0)
        {
            if (started)
            {
                sendBuffer += ",";
            }
            started = 1;
            sendBuffer += String(values[i], 2);
            checkAndSend();
        }
    }
    sendBuffer += "]";
}

void returnData()
{
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "-1");
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "application/json", "{\"time\":");
    timeArrayToSendBuffer();
    sendBuffer += ",\"tcTop\":";
    valueArrayToSendBuffer(tcTopTemps);
    sendBuffer += ",\"ptTop\":";
    valueArrayToSendBuffer(ptTopTemps);
    sendBuffer += ",\"tcBottom\":";
    valueArrayToSendBuffer(tcBottomTemps);
    sendBuffer += ",\"ptBottom\":";
    valueArrayToSendBuffer(ptBottomTemps);
    sendBuffer += ",\"phase\":\"" + String(phase) + "\"}";
    checkAndSend(1);
    server.sendContent(F(""));
    server.client().stop();
}

String getContentType(String filename)
{ // convert the file extension to the MIME type
    if (filename.endsWith(".htm"))
        return "text/html";
    else if (filename.endsWith(".html"))
        return "text/html";
    else if (filename.endsWith(".css"))
        return "text/css";
    else if (filename.endsWith(".js"))
        return "application/javascript";
    else if (filename.endsWith(".png"))
        return "image/png";
    else if (filename.endsWith(".gif"))
        return "image/gif";
    else if (filename.endsWith(".jpg"))
        return "image/jpeg";
    else if (filename.endsWith(".ico"))
        return "image/x-icon";
    else if (filename.endsWith(".xml"))
        return "text/xml";
    else if (filename.endsWith(".pdf"))
        return "application/x-pdf";
    else if (filename.endsWith(".zip"))
        return "application/x-zip";
    else if (filename.endsWith(".gz"))
        return "application/x-gzip";
    return "text/plain";
}

bool handleFileRead(String path)
{ // send the right file to the client (if it exists)
    Serial.println("handleFileRead: " + path);
    if (path.endsWith("/"))
        path += "index.html";                  // If a folder is requested, send the index file
    String contentType = getContentType(path); // Get the MIME type
    String pathWithGz = path + ".gz";
    if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path))
    {                                                       // If the file exists, either as a compressed archive, or normal
        if (SPIFFS.exists(pathWithGz))                      // If there's a compressed version available
            path += ".gz";                                  // Use the compressed version
        File file = SPIFFS.open(path, "r");                 // Open the file
        size_t sent = server.streamFile(file, contentType); // Send it to the client
        file.close();                                       // Close the file again
        Serial.println(String("\tSent file: ") + path);
        return true;
    }
    Serial.println(String("\tFile Not Found: ") + path);
    return false; // If the file doesn't exist, return false
}

void setup()
{
    Serial.begin(115200);

    WiFi.softAP("WirelessOven");

    IPAddress myIP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(myIP);
    if (MDNS.begin("oven"))
    { // Start the mDNS responder for esp8266.local
        Serial.println("mDNS responder started");
    }
    else
    {
        Serial.println("Error setting up MDNS responder!");
    }

    SPIFFS.begin();

    server.on("/data", returnData);
    server.on("/start", sendStartSignal);
    server.on("/stop", sendStopSignal);
    server.on("/settings", handleSettings);
    server.onNotFound([]() {                               // If the client requests any URI
        if (!handleFileRead(server.uri()))                 // send it if it exists
            server.send(404, "text/plain", "nothingHere"); // otherwise, respond with a 404 (Not Found) error
    });
    server.begin();
    Serial.println("HTTP server started");
}

void loop()
{
    server.handleClient();
    readFromSerial();
}
