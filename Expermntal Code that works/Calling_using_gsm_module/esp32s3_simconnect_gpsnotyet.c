#include <Arduino.h>
#include <HardwareSerial.h>
#include <TinyGPSPlus.h>

// --- HARDWARE SERIAL PORTS ---
// Port 1 for GPS (UART1)
HardwareSerial gpsSerial(1); 
const int GPS_RX_PIN = 16;
const int GPS_TX_PIN = 17;

// Port 2 for SIM800L (UART2)
HardwareSerial sim800(2);
const int SIM_RX_PIN = 18;
const int SIM_TX_PIN = 19;

// --- CONFIGURATION ---
const char apn[] = "internet";        // Your APN 
const char server[] = "81.10.88.24";   // Your Traccar IP
const int port = 5055;                // Traccar OsMand Port
const char deviceID[] = "nano_tracker"; // Set to your preferred identifier

// --- OBJECTS & TIMERS ---
TinyGPSPlus gps;
unsigned long lastSend = 0;
const unsigned long sendInterval = 10000; // Send every 10 seconds

/**
 * @brief Sends an AT command and checks for a specific expected response string.
 * @return true if the expected response is received within the timeout.
 */
bool simAT(String cmd, String expectedResponse, unsigned long timeout) {
  sim800.println(cmd);
  Serial.print("TX: "); Serial.println(cmd);

  String response = "";
  unsigned long t = millis();
  
  while (millis() - t < timeout) {
    while (sim800.available()) {
      char c = sim800.read();
      response += c;
      Serial.write(c); // Echo response to USB Serial Monitor
    }
    // Check for success condition
    if (response.indexOf(expectedResponse) >= 0) {
      return true;
    }
    // Check for general failure early (ERROR, FAIL, CME ERROR)
    if (response.indexOf("ERROR") >= 0 || response.indexOf("FAIL") >= 0) {
      return false; 
    }
  }
  return false;
}

/**
 * @brief Checks if the SIM800L is registered to the network.
 * @return true if the modem is registered (CREG: 0,1 or 0,5).
 */
bool isSimRegistered() {
  Serial.println(F("\nChecking SIM Registration..."));
  // 0,1 = Registered (Home Network), 0,5 = Registered (Roaming)
  if (!simAT("AT+CREG?", "+CREG: 0,1", 2000) && !simAT("AT+CREG?", "+CREG: 0,5", 2000)) {
    Serial.println(F("SIM NOT REGISTERED (Check SIM/Signal)."));
    return false;
  }
  Serial.println(F("SIM Registered."));
  return true;
}

void sendDataToTraccar(double lat, double lon, double speed, double alt) {
  Serial.println(F("\n--- Starting GPRS/Traccar Transmission ---"));

  // 1. Network Setup Check
  if (!isSimRegistered()) return;
  
  // Set Bearer Profile and APN
  if (!simAT("AT+SAPBR=3,1,\"Contype\",\"GPRS\"", "OK", 1000)) return;
  if (!simAT("AT+SAPBR=3,1,\"APN\",\"" + String(apn) + "\"", "OK", 1000)) return;
  
  // Open Bearer
  if (!simAT("AT+SAPBR=1,1", "OK", 10000)) { // 10 second timeout for GPRS connection
    Serial.println(F("GPRS Bearer failed. APN or connectivity issue."));
    return;
  }

  // --- 2. HTTP ACTION SETUP (FIXED) ---
  // CRITICAL FIX: Terminate any previous session before starting a new one
  simAT("AT+HTTPTERM", "OK", 1000); 

  if (!simAT("AT+HTTPINIT", "OK", 1000)) {
      Serial.println(F("HTTP initialization failed, cannot proceed."));
      return;
  }
  
  if (!simAT("AT+HTTPPARA=\"CID\",1", "OK", 1000)) return;

  // 3. Construct Traccar URL (OsMand Protocol)
  String url = "http://" + String(server) + ":" + String(port) + "/";
  url += "?id=" + String(deviceID);
  url += "&lat=" + String(lat, 6);
  url += "&lon=" + String(lon, 6);
  url += "&speed=" + String(speed);
  url += "&alt=" + String(alt);

  Serial.println("URL: " + url);

  if (!simAT("AT+HTTPPARA=\"URL\",\"" + url + "\"", "OK", 1000)) return;
  
  // 4. Send Request (0 = GET) 
  Serial.println(F("Sending HTTP GET (AT+HTTPACTION=0)..."));
  
  // Expected success response contains "200"
  if (simAT("AT+HTTPACTION=0", "+HTTPACTION: 0,200,", 15000)) { 
      Serial.println(F("✅ Success! Data sent to Traccar."));
  } else {
      Serial.println(F("❌ HTTP Request failed or Traccar returned an error code."));
  }

  // 5. Clean Up
  simAT("AT+HTTPTERM", "OK", 1000);
  simAT("AT+SAPBR=0,1", "OK", 1000); // Close Bearer
}

void setup() {
  // 1. Setup USB Serial for debugging
  Serial.begin(115200); 
  Serial.println(F("System Initializing on ESP32-S3..."));

  // 2. Setup GPS on UART1
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

  // 3. Setup SIM800L on UART2
  sim800.begin(9600, SERIAL_8N1, SIM_RX_PIN, SIM_TX_PIN);

  // Initial power-on delay
  Serial.println(F("Waiting 15 seconds for modem to boot and find network..."));
  delay(15000); 

  // Initial modem checks
  simAT("AT", "OK", 1000); 
  simAT("AT+CSQ", "CSQ:", 1000); 
  
  Serial.println(F("Setup Complete. Starting loop..."));
}

void loop() {
  // 1. Process GPS Data (runs concurrently)
  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
  }
  
  // 2. Timer Check: Is it time to send?
  if (millis() - lastSend >= sendInterval) {
    lastSend = millis();
    
    // 3. Collect GPS or Placeholder Data
    double lat = 0.0;
    double lon = 0.0;
    double speed = 0.0;
    double alt = 0.0;
    
    if (gps.location.isValid()) {
      // ✅ Use real data
      lat = gps.location.lat();
      lon = gps.location.lng();
      speed = gps.speed.kmph();
      alt = gps.altitude.meters();
      Serial.print("✅ GPS FIX! Lat: "); Serial.print(lat, 6); Serial.print(", Lon: "); Serial.println(lon, 6);
      
    } else {
      // ⚠️ Use placeholder data (0, 0)
      Serial.print(F("🔴 NO GPS FIX. Sending placeholder data. Satellites: "));
      Serial.println(gps.satellites.value());
    }
    
    // 4. Execute the Traccar transmission sequence regardless of GPS fix status
    sendDataToTraccar(lat, lon, speed, alt);
  }
}
