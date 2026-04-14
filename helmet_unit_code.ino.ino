#include <WiFi.h>
#include <Wire.h>
#include <MPU6050.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>

#define IR_SENSOR 32
#define ALCOHOL_SENSOR 34
#define SHOCK_SENSOR 25
#define BUTTON_PIN 0

#define ALCOHOL_THRESHOLD 1900
#define TILT_THRESHOLD 45

MPU6050 mpu;

// GPS
HardwareSerial gpsSerial(1);
TinyGPSPlus gps;

// GSM
HardwareSerial gsmSerial(2);

// WiFi credentials
const char* ssid = "BikeESP";
const char* password = "12345678";

WiFiClient client;

bool accidentDetected = false;
bool alertSent = false;
unsigned long accidentTime = 0;
bool lastHelmetState = HIGH;

//////////////////////////////////////////////////////////////

void setup()
{
  Serial.begin(115200);

  pinMode(IR_SENSOR, INPUT);
  pinMode(SHOCK_SENSOR, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Wire.begin();
  mpu.initialize();

  gpsSerial.begin(9600, SERIAL_8N1, 16, 17);
  gsmSerial.begin(9600, SERIAL_8N1, 26, 27);

  WiFi.begin(ssid, password);

  Serial.print("Connecting to Bike WiFi");

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnected to Bike Unit");

  delay(2000);

  gsmSerial.println("AT");
  delay(1000);

  gsmSerial.println("AT+CMGF=1");
  delay(1000);

  Serial.println("Helmet Unit Ready");
}

//////////////////////////////////////////////////////////////

void loop()
{
  checkHelmetAndAlcohol();

  detectAccident();

  handleAccident();

  delay(300);
}

//////////////////////////////////////////////////////////////
// Helmet + Alcohol Check
//////////////////////////////////////////////////////////////

void checkHelmetAndAlcohol()
{
  int helmet = digitalRead(IR_SENSOR);
  int alcohol = analogRead(ALCOHOL_SENSOR);

  if(helmet != lastHelmetState)
  {
    Serial.print("Alcohol Value: ");
    Serial.println(alcohol);

    if(client.connect("192.168.4.1",80))
    {
      if(helmet == LOW && alcohol < ALCOHOL_THRESHOLD)
      {
        client.println("ALLOW");
        Serial.println("Bike Allowed");
      }
      else
      {
        client.println("DENY");
        Serial.println("Bike Blocked");
      }

      client.stop();
    }

    lastHelmetState = helmet;
  }
}

//////////////////////////////////////////////////////////////
// Accident Detection
//////////////////////////////////////////////////////////////

void detectAccident()
{
  if (accidentDetected) return;

  int16_t ax, ay, az;
  mpu.getAcceleration(&ax, &ay, &az);

  float angleX = atan2((float)ax, (float)az) * 180.0 / PI;
  float angleY = atan2((float)ay, (float)az) * 180.0 / PI;

  bool tilt = abs(angleX) > TILT_THRESHOLD ||
              abs(angleY) > TILT_THRESHOLD;

  int shock = digitalRead(SHOCK_SENSOR);

  Serial.print("Tilt: ");
  Serial.print(tilt);
  Serial.print(" Shock: ");
  Serial.println(shock);

  if (tilt && shock == HIGH)
  {
    accidentDetected = true;
    accidentTime = millis();
    alertSent = false;

    Serial.println("Accident Detected!");
  }
}

//////////////////////////////////////////////////////////////
// Accident Handling
//////////////////////////////////////////////////////////////

void handleAccident()
{
  if (!accidentDetected) return;

  if (millis() - accidentTime < 20000)
  {
    Serial.println("Press BOOT Button to Cancel Alert");

    if (digitalRead(BUTTON_PIN) == LOW)
    {
      accidentDetected = false;
      alertSent = false;

      Serial.println("Alert Cancelled by Button");
    }
  }
  else
  {
    if (!alertSent)
    {
      getGPSAndSendSMS();
      alertSent = true;
    }
  }
}

//////////////////////////////////////////////////////////////
// GPS + SMS Alert
//////////////////////////////////////////////////////////////

void getGPSAndSendSMS()
{
  Serial.println("Getting GPS Location...");

  float lat = 0;
  float lon = 0;
  bool gpsFound = false;

  unsigned long start = millis();

  while (millis() - start < 10000)
  {
    while (gpsSerial.available())
    {
      gps.encode(gpsSerial.read());

      if (gps.location.isUpdated())
      {
        lat = gps.location.lat();
        lon = gps.location.lng();
        gpsFound = true;
        break;
      }
    }

    if(gpsFound) break;
  }

  if(gpsFound)
  {
    Serial.println("GPS Found");
    sendSMS(lat, lon);
  }
  else
  {
    Serial.println("GPS Not Found");
    sendSMSNoLocation();
  }
}

//////////////////////////////////////////////////////////////
// Send SMS with Location
//////////////////////////////////////////////////////////////

void sendSMS(float lat, float lon)
{
  Serial.println("Sending SMS with Location...");

  gsmSerial.println("AT");
  delay(1000);

  gsmSerial.println("AT+CMGF=1");
  delay(1000);

  gsmSerial.print("AT+CMGS=\"+918975545624\"\r");
  delay(2000);

  gsmSerial.println("It's an Emergency ..!! USER has faced an ACCIDENT, to know his loaction click on below link");
  gsmSerial.print("Location: https://maps.google.com/?q=");
  gsmSerial.print(lat,6);
  gsmSerial.print(",");
  gsmSerial.println(lon,6);

  gsmSerial.write(26);

  delay(5000);

  Serial.println("SMS Sent");
}

//////////////////////////////////////////////////////////////
// Send SMS without Location
//////////////////////////////////////////////////////////////

void sendSMSNoLocation()
{
  Serial.println("Sending SMS without GPS...");

  gsmSerial.println("AT");
  delay(1000);

  gsmSerial.println("AT+CMGF=1");
  delay(1000);

  gsmSerial.print("AT+CMGS=\"+918975545624\"\r");
  delay(2000);

  gsmSerial.println("It's an Emergency ..!! USER has faced an ACCIDENT, to know his loaction click on below link");
  gsmSerial.println("GPS location not available.");
  gsmSerial.println("Please contact the rider immediately.");

  gsmSerial.write(26);

  delay(5000);

  Serial.println("SMS Sent without GPS");
}