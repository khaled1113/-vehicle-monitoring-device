#include <SoftwareSerial.h> // Include SoftwareSerial library for Arduino Nano

// Define the SoftwareSerial pins for the SIM800L
// Nano Digital Pin 10 -> SIM800L TX (Data from SIM800L)
// Nano Digital Pin 11 -> SIM800L RX (Data to SIM800L)
SoftwareSerial SIM800(10, 11); 

// --- TRACCAR CONFIGURATION ---
const char* SERVER_IP = "81.10.88.24";
const char* SERVER_PORT = "5055"; // OsmAnd protocol default port
const char* DEVICE_ID = "sim800_nano_demo"; // IMPORTANT: Change this to a unique ID for your device in Traccar
const char* APN = "internet"; // IMPORTANT: Replace with your mobile operator's APN (e.g., "giffgaff.com", "tdata.eg")
const char* APN_USER = "";    // APN username (often left blank)
const char* APN_PASS = "";    // APN password (often left blank)

// --- DEMO LOCATION (31°01'21.4"N 31°18'28.7"E) ---
const float DEMO_LATITUDE = 31.022611; 
const float DEMO_LONGITUDE = 31.307972; 
const int REPORT_INTERVAL_MS = 60000; // Report location every 60 seconds

// Configuration
const long BAUD_RATE = 9600; // Standard baud rate for SIM800L modules (important for SoftwareSerial stability)

// State variables
unsigned long lastReportTime = 0;

// FUNCTION PROTOTYPES
bool sendATCommand(const char* command, const char* expected_response, const char* description, unsigned long timeout = 3000);
void sendLocation();

void setup() {
  // Initialize standard Serial for debugging and logging (via USB)
  Serial.begin(115200);
  while (!Serial);

  Serial.println("--- ARDUINO NANO SIM800L Traccar Tracker Setup ---");

  // Initialize the SIM800L serial port (SoftwareSerial)
  SIM800.begin(BAUD_RATE); // Pins are defined in the constructor
  
  // Wait a moment for the module to start up and stabilize
  delay(3000); 

  // --- Initialize SIM800L Module and GPRS ---
  
  // 1. Send AT command to check communication
  if (!sendATCommand("AT", "OK", "Module Ready")) return; 
  
  // 2. Set command echo off (optional)
  sendATCommand("ATE0", "OK", "Echo Off"); 

  // 3. Deactivate any existing GPRS context
  sendATCommand("AT+CIPSHUT", "SHUT OK", "Deactivate GPRS");

  // 4. Attach to GPRS Service
  Serial.println("\n--- Attaching to GPRS... (Takes time) ---");
  if (!sendATCommand("AT+CGATT=1", "OK", "Attach GPRS", 10000)) return; // 10s timeout

  // 5. Set APN (Access Point Name)
  String apnCmd = "AT+CSTT=\"" + String(APN) + "\",\"" + String(APN_USER) + "\",\"" + String(APN_PASS) + "\"";
  if (!sendATCommand(apnCmd.c_str(), "OK", "Set APN")) return;

  // 6. Bring up the wireless connection (Start the GPRS context)
  Serial.println("\n--- Connecting to Network... (Takes time) ---");
  if (!sendATCommand("AT+CIICR", "OK", "Bring Up Wireless", 15000)) return; // 15s timeout

  // 7. Get local IP address (Optional, but confirms connection)
  sendATCommand("AT+CIFSR", ".", "Get Local IP"); // Expects the IP address, we just check for any response

  Serial.println("\n--- GPRS Initialized. Starting Location Loop. ---");
  lastReportTime = millis() - REPORT_INTERVAL_MS; // Force first report immediately
}

void loop() {
  // Check if it's time to send a new report
  if (millis() - lastReportTime >= REPORT_INTERVAL_MS) {
    sendLocation();
    lastReportTime = millis();
  }

  // Continuously monitor the SIM800L serial for any unsolicited messages (e.g., network status)
  while (SIM800.available()) {
    Serial.write(SIM800.read());
  }

  delay(10); 
}

/**
 * Constructs the Traccar URL and sends the data via a TCP connection.
 */
void sendLocation() {
  Serial.println("\n************************************************");
  Serial.println("*** SENDING LOCATION REPORT ***");
  Serial.println("************************************************");

  // 1. Build the Traccar/OsmAnd URL (Example: /?id=sim800-nano&lat=31.022611&lon=31.307972&timestamp=1672531200000&speed=0&alt=10)
  String payload = "GET /?id=";
  payload += DEVICE_ID;
  payload += "&lat=";
  payload += String(DEMO_LATITUDE, 6); // 6 decimal places for precision
  payload += "&lon=";
  payload += String(DEMO_LONGITUDE, 6);
  payload += "&timestamp=";
  payload += String(millis()); // Using milliseconds since startup as a demo timestamp
  payload += "&speed=0&altitude=10"; // Dummy values for speed and altitude
  payload += " HTTP/1.1\r\n"; // End of GET line
  payload += "Host: " + String(SERVER_IP) + ":" + String(SERVER_PORT) + "\r\n";
  payload += "User-Agent: SIM800L-Tracker\r\n";
  payload += "Connection: close\r\n\r\n"; // Important: two CRLF at the end of headers

  // 2. Start TCP Connection
  String cipstartCmd = "AT+CIPSTART=\"TCP\",\"" + String(SERVER_IP) + "\",\"" + String(SERVER_PORT) + "\"";
  if (!sendATCommand(cipstartCmd.c_str(), "CONNECT OK", "Start TCP Connection", 10000)) {
    Serial.println("Failed to start TCP connection. Retrying later.");
    return;
  }

  // 3. Send Data
  String cipsendCmd = "AT+CIPSEND=" + String(payload.length());
  if (!sendATCommand(cipsendCmd.c_str(), ">", "Ready to Send Data", 5000)) { // Expect prompt '>'
    Serial.println("Failed to get CIPSEND prompt. Closing connection.");
    sendATCommand("AT+CIPCLOSE", "CLOSE OK", "Close TCP (Error)");
    return;
  }
  
  // Actually send the payload
  SIM800.print(payload);
  Serial.print(">> Sent Payload (Length: "); Serial.print(payload.length()); Serial.println(" bytes)");
  
  // Wait for the final "SEND OK" response from the SIM800L
  sendATCommand("", "SEND OK", "Data Sent Confirmation", 10000); 

  // 4. Close TCP Connection (Traccar server will close it anyway, but good practice)
  sendATCommand("AT+CIPCLOSE", "CLOSE OK", "Close TCP Connection");

  Serial.println("Report cycle complete.");
}


/**
 * Helper function to send an AT command and check for a specific response.
 * @return true if the expected response is found, false otherwise.
 */
bool sendATCommand(const char* command, const char* expected_response, const char* description, unsigned long timeout) {
  // Clear any existing data in the buffer first
  while(SIM800.available()) {
    SIM800.read();
  }
  
  SIM800.println(command);
  Serial.print(">> Sending: ");
  Serial.print(command);
  Serial.print(" (");
  Serial.print(description);
  Serial.println(")");
  
  // Set the specified timeout
  SIM800.setTimeout(timeout); 
  String response = SIM800.readString(); // Read all available data until timeout
  
  response.trim();
  
  // Attempt to strip the echo of the command (common SIM800L behavior)
  String commandStr = String(command);
  if (commandStr.length() > 0 && response.startsWith(commandStr)) {
      response.remove(0, commandStr.length());
      response.trim();
  }

  // Check the response
  if (response.length() > 0) {
    Serial.print("SIM800L Raw Response (cleaned): ");
    Serial.println(response);
    
    // Check if the expected response is contained in the full response string
    if (response.indexOf(expected_response) != -1) { 
      Serial.println("Status: SUCCESS.");
      return true;
    } else {
      Serial.print("Status: FAILED. Expected '");
      Serial.print(expected_response);
      Serial.println("' not found.");
      return false;
    }
  } else {
    Serial.println("Status: FAILED. No response received within timeout.");
    return false;
  }
}
