#include <string.h>
#include <mpu6050_esp32.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <math.h>
#include <WiFi.h>

//TIMING VARIABLES
int seconds = 0;
int minutes = 0;
int hours = 0;
char hourStr[3];
char minuteStr[3];
char output[10];
const int32_t SEC_UPDATE = 1000;
const int32_t LOOP_DELAY = 1000;
uint32_t last_sec_update;
uint32_t last_request_time = 0; //used for timing

//Mode constants
int startup = 0;
const int PIN_A = 40;
const uint8_t BUTTON1 = 45;
const uint8_t BUTTON2 = 39;
int button1_state = 0;
int button2_state = 0;
const int ALWAYS_ON = 0;  //change if you'd like
const int IMU_MODE = 1; //change if you'd like
int display = ALWAYS_ON; //modes: 0 (ALWAYS ON) and 1 (IMU-triggered)
int mode = 0;
const int COLON_TIME = 1000;
int last_colon_time = 0;
int colon_state = 0;

//imu constants:
MPU6050 imu; //imu object called, appropriately, imu
float x, y, z; //variables for grabbing x,y,and z values
uint32_t last_motion = 0; //used for timing 
const int MOTION_TIMEOUT = 15000;
float old_acc_mag, older_acc_mag; //maybe use for remembering older values of acceleration magnitude
float acc_mag = 0;  //used for holding the magnitude of acceleration
float avg_acc_mag = 0; //used for holding the running average of acceleration magnitude
float threshold = 11.5;
const float ZOOM = 9.81;

//WiFi Variables
//wifi network credentials for 6.08 Lab (this is a decent 2.4 GHz router that we have set up...try to only use for our ESP32s)
// char network[] = "608_24G";
// char password[] = "608g2020";
char network[] = "MIT GUEST";
char password[] = "";
uint8_t scanning = 0;//set to 1 if you'd like to scan for wifi networks (see below):

//Buffer variables:
const int RESPONSE_TIMEOUT = 6000; //ms to wait for response from host
const int GETTING_PERIOD = 60000; //periodicity of getting a number fact.
const uint16_t IN_BUFFER_SIZE = 1000; //size of buffer to hold HTTP request
const uint16_t OUT_BUFFER_SIZE = 1000; //size of buffer to hold HTTP response
char request_buffer[IN_BUFFER_SIZE]; //char array buffer to hold HTTP request
char response_buffer[OUT_BUFFER_SIZE]; //char array buffer to hold HTTP response

TFT_eSPI tft = TFT_eSPI();  // Invoke library, pins defined in User_Setup.h

void setup() {
  tft.init();
  tft.setRotation(1);
  tft.setTextSize(3);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(0, 2, 1);
  Serial.begin(115200); //begin serial
  if (scanning){
    int n = WiFi.scanNetworks();
    Serial.println("scan done");
    if (n == 0) {
      Serial.println("no networks found");
    } else {
      Serial.print(n);
      Serial.println(" networks found");
      for (int i = 0; i < n; ++i) {
        Serial.printf("%d: %s, Ch:%d (%ddBm) %s ", i + 1, WiFi.SSID(i).c_str(), WiFi.channel(i), WiFi.RSSI(i), WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "open" : "");
        uint8_t* cc = WiFi.BSSID(i);
        for (int k = 0; k < 6; k++) {
          Serial.print(*cc, HEX);
          if (k != 5) Serial.print(":");
          cc++;
        }
        Serial.println("");
      }
    }
  }
  delay(100); //wait a bit (100 ms)

  //if using regular connection use line below:
  WiFi.begin(network, password);
  //if using channel/mac specification for crowded bands use the following:
  // WiFi.begin(network, password, channel, bssid);

  uint8_t count = 0; //count used for Wifi check times
  Serial.print("Attempting to connect to ");
  Serial.println(network);
  while (WiFi.status() != WL_CONNECTED && count < 6) { //can change this to more attempts
    delay(500);
    Serial.print(".");
    count++;
  }
  if (imu.setupIMU(1)) {
    Serial.println("IMU Connected!");
  } else {
    Serial.println("IMU Not Connected :/");
    Serial.println("Restarting");
    ESP.restart(); // restart the ESP (proper way)
  }
  delay(2000);  //acceptable since it is in the setup function.
  if (WiFi.isConnected()) { //if we connected then print our IP, Mac, and SSID we're on
    Serial.println("CONNECTED!");
    Serial.printf("%d:%d:%d:%d (%s) (%s)\n", WiFi.localIP()[3], WiFi.localIP()[2],
                  WiFi.localIP()[1], WiFi.localIP()[0],
                  WiFi.macAddress().c_str() , WiFi.SSID().c_str());
    delay(500);
  } else { //if we failed to connect just Try again.
    Serial.println("Failed to Connect :/  Going to restart");
    Serial.println(WiFi.status());
    ESP.restart(); // restart the ESP (proper way)
  }
  pinMode(BUTTON1, INPUT_PULLUP); //set input pin as an input!
  pinMode(BUTTON2, INPUT_PULLUP); //set input pin as an input!
  last_sec_update = millis();
  last_motion = millis();
}


void loop() {
  //get IMU information:
  imu.readAccelData(imu.accelCount);
  x = ZOOM * imu.accelCount[0] * imu.aRes;
  y = ZOOM * imu.accelCount[1] * imu.aRes;
  z = ZOOM * imu.accelCount[2] * imu.aRes;

  //Serial printing:
  char output[100];
  acc_mag = sqrt(pow(x, 2) + pow(y, 2) + pow(z, 2));
  avg_acc_mag = (older_acc_mag + old_acc_mag + acc_mag)/3;
  older_acc_mag = old_acc_mag;
  old_acc_mag = acc_mag;

  //get button readings:
  uint8_t button1 = digitalRead(BUTTON1);
  uint8_t button2 = digitalRead(BUTTON2);

  // toggle modes and get time
  toggle_time(button1);
  toggle_display(button2);

  if ((startup == 0) || ((millis() - last_request_time) >= GETTING_PERIOD)) { // GETTING_PERIOD since last lookup? Look up again
    //formulate GET request...first line:
    strcpy(request_buffer, "GET http://iesc-s3.mit.edu/esp32test/currenttime HTTP/1.1\r\n");
    strcat(request_buffer, "Host: iesc-s3.mit.edu\r\n"); //add more to the end
    strcat(request_buffer, "\r\n"); //add blank line!
    //submit to function that performs GET.  It will return output using response_buffer char array
    do_http_GET("iesc-s3.mit.edu", request_buffer, response_buffer, OUT_BUFFER_SIZE, RESPONSE_TIMEOUT, true);
    // Serial.println(response_buffer); //print to serial monitor
    memset(minuteStr, 0, sizeof(minuteStr));
    memset(hourStr, 0, sizeof(hourStr));
    strncpy(hourStr, response_buffer + 11, 2);
    hours = atoi(hourStr);
    if (hours > 12) {
      hours = hours - 12;
    }
    strncpy(minuteStr, response_buffer + 14, 2);
    ++startup;
    last_request_time = millis();//remember when this happened so we perform next lookup in GETTING_PERIOD milliseconds
  }

  set_display();

  if (millis() - last_sec_update >= SEC_UPDATE) {
    ++seconds;
    if (seconds == 60) {
      seconds = 0;
    }
    // memset(output, 0, sizeof(output));
    // if (mode == 0) {
    //   hour_min();
    // } else {
    //   hour_min_sec();
    // }
    last_sec_update = millis();
  }
}

void set_display () {
  if (display == IMU_MODE) {
    Serial.println("In IMU mode");
    if (avg_acc_mag >= threshold) {
      last_motion = millis();
      Serial.print("Accleration: ");
      Serial.println(avg_acc_mag);
    } else {
      if (millis() - last_motion < MOTION_TIMEOUT) {
        if (mode == 0) {
          hour_min();
        } else {
          hour_min_sec();
        }
      } else {
        tft.setCursor(0,2,1);
        tft.print("                     ");
        Serial.println("No motion!");
      }
    }
  } else {
      if (mode == 0) {
        hour_min();
      } else {
        hour_min_sec();
      }
  }
}

//2 Time Modes:
//1) Hour: Minute - colon flashes on-off at 0.5 Hz with 50% duty cycle (every 1000 ms)
void hour_min () {
  char withColon[100];
  char withoutColon[100];
  sprintf(withColon, "%d:%s              ", hours, minuteStr, seconds);
  sprintf(withoutColon, "%d %s              ", hours, minuteStr, seconds);

  if (millis() - last_colon_time >= 1000) {
    tft.setCursor(0,2,1);
    if (colon_state == 0) {
      tft.print(withColon);
      colon_state = 1;
    } else {
      tft.print(withoutColon);
      colon_state = 0;
    }
    last_colon_time = millis();
  }
}

//2) Hour: Minute: Second - colons don't flash, but seconds update once per second
void hour_min_sec() {
  char curr_time[100];
  sprintf(curr_time, "%d:%s:%02d              ", hours, minuteStr, seconds);
  tft.setCursor(0, 2, 1);
  tft.println(curr_time);
}

/*-----------------------------------
  Generate a request to the numbersapi server for a random number
  Display the response both on the TFT and in the Serial Monitor
*/

//Toggle Timing Mode- PIN 45
void toggle_time(uint8_t button1) {
  if(button1 == 0 && button1_state == 0) { //button1 just pressed, was previously unpressed
    button1_state = 1;
  } else if (button1 == 1 && button1_state == 1) { //button1 just released, was previously pressed
    if (mode == 1) {
      mode = 0;
    } else {
      mode = 1;
    }
    button1_state = 0;
  }
}

//Toggle Display - PIN 39
//Always On - Display always on and shows time
//IMU - monitored for overall motion - if none (by acc_mag) for 15s the screen should go black. 
//If motion, screen turns on. Then if motion stops, the screen should go dark after 15 seconds again (START TIMING AFTER MOTION ENDS)
void toggle_display(uint8_t button2) {
  if(button2 == 0 && button2_state == 0) { //button2 just pressed, was previously unpressed
    button2_state = 1;
  } else if (button2 == 1 && button2_state == 1) { //button2 just released, was previously pressed
    button2_state = 0;
    if (display == ALWAYS_ON) {
      display = IMU_MODE;
      last_motion = millis();
    } else {
      display = ALWAYS_ON;
    }
  }
}

/*----------------------------------
   char_append Function:
   Arguments:
      char* buff: pointer to character array which we will append a
      char c:
      uint16_t buff_size: size of buffer buff

   Return value:
      boolean: True if character appended, False if not appended (indicating buffer full)
*/
uint8_t char_append(char* buff, char c, uint16_t buff_size) {
  int len = strlen(buff);
  if (len > buff_size) return false;
  buff[len] = c;
  buff[len + 1] = '\0';
  return true;
}

/*----------------------------------
   do_http_GET Function:
   Arguments:
      char* host: null-terminated char-array containing host to connect to
      char* request: null-terminated char-arry containing properly formatted HTTP GET request
      char* response: char-array used as output for function to contain response
      uint16_t response_size: size of response buffer (in bytes)
      uint16_t response_timeout: duration we'll wait (in ms) for a response from server
      uint8_t serial: used for printing debug information to terminal (true prints, false doesn't)
   Return value:
      void (none)
*/
void do_http_GET(char* host, char* request, char* response, uint16_t response_size, uint16_t response_timeout, uint8_t serial) {
  WiFiClient client; //instantiate a client object
  if (client.connect(host, 80)) { //try to connect to host on port 80
    if (serial) Serial.print(request);//Can do one-line if statements in C without curly braces
    client.print(request);
    memset(response, 0, response_size); //Null out (0 is the value of the null terminator '\0') entire buffer
    uint32_t count = millis();
    while (client.connected()) { //while we remain connected read out data coming back
      client.readBytesUntil('\n', response, response_size);
      if (serial) Serial.println(response);
      if (strcmp(response, "\r") == 0) { //found a blank line! (end of response header)
        break;
      }
      memset(response, 0, response_size);
      if (millis() - count > response_timeout) break;
    }
    memset(response, 0, response_size);  //empty in prep to store body
    count = millis();
    while (client.available()) { //read out remaining text (body of response)
      char_append(response, client.read(), OUT_BUFFER_SIZE);
    }
    if (serial) Serial.println(response);
    client.stop();
    if (serial) Serial.println("-----------");
  } else {
    if (serial) Serial.println("connection failed :/");
    if (serial) Serial.println("wait 0.5 sec...");
    client.stop();
  }
}