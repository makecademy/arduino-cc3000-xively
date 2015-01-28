/*************************************************** 
  This is a sketch to use the CC3000 WiFi chip & Xively
  
  Written by Marco Schwartz for Open Home Automation
 ****************************************************/

// Libraries
#include <Adafruit_CC3000.h>
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
#define API_key  "yourAPIKey"
#define feedID  "yourFeedID"
int buffer_size = 20;

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
  
  // Connect to WiFi network
  Serial.print(F("Connecting to WiFi network ..."));
  cc3000.connectToAP(WLAN_SSID, WLAN_PASS, WLAN_SECURITY);
  Serial.println(F("done!"));
  
  // Wait for DHCP to complete
  Serial.println(F("Request DHCP"));
  while (!cc3000.checkDHCP())
  {
    delay(100);
  }
 
}

void loop(void)
{
  // Start watchdog 
  wdt_enable(WDTO_8S); 
  
  // Get IP
  uint32_t ip = 0;
  Serial.print(F("api.xively.com -> "));
  while  (ip  ==  0)  {
    if  (!  cc3000.getHostByName("api.xively.com", &ip))  {
      Serial.println(F("Couldn't resolve!"));
      while(1){}
    }
    delay(500);
  }  
  cc3000.printIPdotsRev(ip);
  Serial.println(F(""));
  
  // Get data & transform to integers
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  
  int temperature = (int) t;
  int humidity = (int) h;

  // Prepare JSON for Xively & get length
  int length = 0;

  // JSON data
  String data = "";
  data = data + "\n" + "{\"version\":\"1.0.0\",\"datastreams\" : [ "
  + "{\"id\" : \"Temperature\",\"current_value\" : \"" + String(temperature) + "\"},"
  + "{\"id\" : \"Humidity\",\"current_value\" : \"" + String(humidity) + "\"}]}";
  
  // Get length
  length = data.length();
 
  // Reset watchdog
  wdt_reset();
  
  // Check connection to WiFi
  Serial.print(F("Checking WiFi connection ..."));
  if(!cc3000.checkConnected()){while(1){}}
  Serial.println(F("done."));
  wdt_reset();
  
  // Ping Xively server
   Serial.print(F("Pinging Xively server ..."));
  if(!cc3000.ping(ip, 2)){while(1){}}
  Serial.println(F("done."));
  wdt_reset();
  
  // Send request
  Adafruit_CC3000_Client client = cc3000.connectTCP(ip, 80);
  if (client.connected()) {
    Serial.println(F("Connected to Xively server."));
    
    // Send headers
    Serial.print(F("Sending headers "));
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
    Serial.print(F("Sending data ..."));
    client.fastrprintln(F(""));    
    sendData(client,data,buffer_size);  
    client.fastrprintln(F(""));
    Serial.println(F("done."));
    
    // Reset watchdog
    wdt_reset();
    
  } else {
    Serial.println(F("Connection failed"));    
    return;
  }
  
  // Reset watchdog
  wdt_reset();
  
  Serial.println(F("Reading answer ..."));
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
  Serial.println(F("Closing connection"));
  
  // Reset watchdog & disable
  wdt_reset();
  wdt_disable();
  
  // Wait 10 seconds until next update
  wait(10000);
  
}

// Send data chunk by chunk
void sendData(Adafruit_CC3000_Client& client, String input, int chunkSize) {
  
  // Get String length
  int length = input.length();
  int max_iteration = (int)(length/chunkSize);
  
  if(max_iteration*chunkSize < length)
    max_iteration++;
    
  for (int i = 0; i < length; i++) {
    client.print(input.substring(i*chunkSize, (i+1)*chunkSize));
    wdt_reset();
  }  
}

// Wait for a given time using the watchdog
void wait(int total_delay) {
  
  int number_steps = (int)(total_delay/5000);
  wdt_enable(WDTO_8S);
  for (int i = 0; i < number_steps; i++){
    delay(5000);
    wdt_reset();
  }
  wdt_disable();
}
