#include <ESP8266WiFi.h>

#define RELAY_PIN D2   // Relay control pin

const char* ssid = "BikeESP";
const char* password = "12345678";

WiFiServer server(80);

void setup()
{
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  WiFi.softAP(ssid, password);   // create WiFi access point

  Serial.println("Bike WiFi Access Point Started");
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());

  server.begin();
}

////////////////////////////////////////////////////////////

void loop()
{
  WiFiClient client = server.available();

  if (client)
  {
    Serial.println("Helmet Connected");

    String command = client.readStringUntil('\n');
    command.trim();

    Serial.print("Received Command: ");
    Serial.println(command);

    if (command == "ALLOW")
    {
      digitalWrite(RELAY_PIN, HIGH);
      Serial.println("Ignition ON");
    }

    if (command == "DENY")
    {
      digitalWrite(RELAY_PIN, LOW);
      Serial.println("Ignition OFF");
    }

    client.stop();
  }
}