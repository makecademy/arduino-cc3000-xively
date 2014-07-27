/*************************************************** 
  This is a sketch to use the CC3000 WiFi chip & Xively
  
  Written by Marco Schwartz for Open Home Automation
 ****************************************************/

// Libraries
#include <Adafruit_CC3000.h>
#include <ccspi.h>
#include <SPI.h>
#include "DHT.h"
#include <avr/wdt.h>

// Define CC3000 chip pins
#define ADAFRUIT_CC3000_IRQ   3
#define ADAFRUIT_CC3000_VBAT  5
#define ADAFRUIT_CC3000_CS    10

// DHT sensor
#define DHTPIN 7
#define DHTTYPE DHT11

// Create CC3000 instances
Adafruit_CC3000 cc3000 = Adafruit_CC3000(ADAFRUIT_CC3000_CS, ADAFRUIT_CC3000_IRQ, ADAFRUIT_CC3000_VBAT,
                                         SPI_CLOCK_DIV2); // you can change this clock speed
                                         
// DHT instance
DHT dht(DHTPIN, DHTTYPE);

// WLAN parameters
#define WLAN_SSID       "yourWiFiSSID"
#define WLAN_PASS       "yourWiFiPassword"
#define WLAN_SECURITY   WLAN_SEC_WPA2

// Xively parameters
#define WEBSITE  "api.xively.com"
#define API_key  "yourAPIKey"
#define feedID  "yourFeedID"

uint32_t ip;

void setup(void)
{
  // Initialize
  Serial.begin(115200);
  
  Serial.println(F("Initializing WiFi chip..."));
  if (!cc3000.begin())
  {
    Serial.println(F("Couldn't begin()! Check your wiring?"));
    while(1);
  }
 
}

void loop(void)
{
  // Connect to WiFi network
  cc3000.connectToAP(WLAN_SSID, WLAN_PASS, WLAN_SECURITY);
  Serial.println(F("Connected to WiFi network!"));
  
  /* Wait for DHCP to complete */
  Serial.println(F("Request DHCP"));
  while (!cc3000.checkDHCP())
  {
    delay(100);
  }
  
  // Start watchdog 
  wdt_enable(WDTO_8S);  

  // Set the website IP
  uint32_t ip = cc3000.IP2U32(216,52,233,120);
  
  // Get data & transform to integers
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  
  int temperature = (int) t;
  int humidity = (int) h;
  
  // Prepare JSON for Xively & get length
  int length = 0;

  // JSON beginning
  String data_start = "";
  data_start = data_start + "\n" + "{\"version\":\"1.0.0\",\"datastreams\" : [ ";
  
  // JSON for temperature & humidity
  String data_temperature = "{\"id\" : \"Temperature\",\"current_value\" : \"" + String(temperature) + "\"},";
  String data_humidity = "{\"id\" : \"Humidity\",\"current_value\" : \"" + String(humidity) + "\"}]}";
  
  // Get length
  length = data_start.length() + data_temperature.length() + data_humidity.length();
 
  // Reset watchdog
  wdt_reset();
 
  // Send request
  Adafruit_CC3000_Client client = cc3000.connectTCP(ip, 80);
  if (client.connected()) {
    Serial.println(F("Connected to Xively server."));
    
    // Send headers
    Serial.print(F("Sending headers"));
    client.fastrprint(F("PUT /v2/feeds/"));
    client.fastrprint(feedID);
    client.fastrprintln(F(".json HTTP/1.0"));
    Serial.print(F("."));
    client.fastrprintln(F("Host: api.xively.com"));
    Serial.print(F("."));
    client.fastrprint(F("X-ApiKey: "));
    client.fastrprintln(API_key);
    Serial.print(F("."));
    client.fastrprint(F("Content-Length: "));
    client.println(length);
    Serial.print(F("."));
    client.fastrprint(F("Connection: close"));
    Serial.println(F(" done."));
    
    // Reset watchdog
    wdt_reset();
    
    // Send data
    Serial.print(F("Sending data"));
    client.fastrprintln(F(""));    
    client.print(data_start);
    Serial.print(F("."));
    wdt_reset();
    client.print(data_temperature);
    Serial.print(F("."));
    wdt_reset();
    client.print(data_humidity);
    Serial.print(F("."));  
    client.fastrprintln(F(""));
    Serial.println(F(" done."));
    
    // Reset watchdog
    wdt_reset();
    
  } else {
    Serial.println(F("Connection failed"));    
    return;
  }
  
  // Reset watchdog
  wdt_reset();
  
  Serial.println(F("Reading answer..."));
  while (client.connected()) {
    while (client.available()) {
      char c = client.read();
      Serial.print(c);
    }
  }
  
  // Reset watchdog
  wdt_reset();
   
  // Close connection and disconnect
  client.close();
  Serial.println(F("Disconnecting"));
  Serial.println(F(""));
  cc3000.disconnect();
  
  // Reset watchdog & disable
  wdt_reset();
  wdt_disable();
  
  // Wait 10 seconds until next update
  delay(10000);
  
}
