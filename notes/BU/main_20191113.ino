//./build-local.sh tcp_photographer
//particle flash --usb target/1.4.2/boron/OVFork.bin
SYSTEM_MODE(AUTOMATIC);
SYSTEM_THREAD(ENABLED);

#include "ArduCAM.h"
#include "memorysaver.h"
#include "ParticleFTPClient.h"
#include "SD.h"
#include <PCF8523.h>
#include <ArduinoJson.h>

const int statusLed = D7; 
const int SPI_CS = A5;
const int SD_CS = D5;
const int Set_rly = A0;
const int Reset_rly = D6;
FuelGauge fuel;

ApplicationWatchdog wd(90000, System.reset);

PCF8523 rtc;
char monthOfTheYear[12][5] = {"jan", "fev", "mar", "avr", "mai", "juin", "juil", "aou", "sep", "oct", "nov", "dec"};
DateTime now;

String hostname = "142.213.166.184";
String username = "bp1";
String password = "BlyuHet7";
String ftp_dir = "/AutoCamDEH";
/*
String hostname = "173.181.243.41";
String username = "6419_FTPCEL";
String password = "6419_IMYIKL";
*/
using namespace particleftpclient;
int port = 21;
int timeout = 50;
bool offlinemode = false;
int Batt_low_SP = 20;//%

#define VERSION_SLUG "V0.25"

#define TX_BUFFER_MAX 512
uint8_t buffer[TX_BUFFER_MAX + 1];
int tx_buffer_index = 0;

Sd2Card card;
SdVolume volume;
String configFileName = "Config.jsn";
char logbuffer[1000];

String stationName = "undef";
String BDH = "00000";
String PublicIP = "0.0.0.0";
int captureMode = 0;

// allow us to use itoa() in this scope
//extern char* itoa(int a, char* buffer, unsigned char radix);
//#define BMPIMAGEOFFSET 66
//const char bmp_header[BMPIMAGEOFFSET] PROGMEM =
//{
//      0x42, 0x4D, 0x36, 0x58, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x42, 0x00, 0x00, 0x00, 0x28, 0x00,
//      0x00, 0x00, 0x40, 0x01, 0x00, 0x00, 0xF0, 0x00, 0x00, 0x00, 0x01, 0x00, 0x10, 0x00, 0x03, 0x00,
//      0x00, 0x00, 0x00, 0x58, 0x02, 0x00, 0xC4, 0x0E, 0x00, 0x00, 0xC4, 0x0E, 0x00, 0x00, 0x00, 0x00,
//      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF8, 0x00, 0x00, 0xE0, 0x07, 0x00, 0x00, 0x1F, 0x00,
//      0x00, 0x00
//};

ArduCAM myCAM(OV2640, SPI_CS);

ParticleFtpClient ftp = ParticleFtpClient();

struct Config {
  char BDH[8] = "00000";
  char stationName[20] =  "undef";
  char PublicIP[20] = "0.0.0.0";
  int captureMode = 0;  //0:3 img/jour, 1:12 img/jour (7-18), 2:24 img/jour
  int Batt_low_SP = 20;
  char ftp_hostname[20] = "142.213.166.184";
  char ftp_username[20] = "bp1";
  char ftp_password[20] = "BlyuHet7";
  char ftp_dir[20] = "/AutoCamDEH";
};

Config config;

void loadConfiguration(const char *filename, Config &config) {
  // Open file for reading
  File file = SD.open(filename);

  // Allocate a temporary JsonDocument
  // Don't forget to change the capacity to match your requirements.
  // Use arduinojson.org/v6/assistant to compute the capacity.
  StaticJsonDocument<512> doc;

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, file);
  if (error)
  log("Failed to read file, using default configuration", 3);
  // Copy values from the JsonDocument to the Config
  strlcpy(config.BDH,                  // <- destination
    doc["BDH"] | BDH,  // <- source
    sizeof(config.BDH));         // <- destination's capacity

  strlcpy(config.stationName,
    doc["stationName"] | stationName,
    sizeof(config.stationName));
    
  strlcpy(config.PublicIP,
    doc["PublicIP"] | PublicIP,
    sizeof(config.PublicIP));

  config.captureMode = doc["Capture_Mode"] | captureMode;
  config.Batt_low_SP = doc["Batt_low_SP"] | Batt_low_SP;

  strlcpy(config.ftp_hostname,
    doc["ftp_hostname"] | hostname,
    sizeof(config.ftp_hostname));  

  strlcpy(config.ftp_username,
    doc["ftp_username"] | username,
    sizeof(config.ftp_username)); 

  strlcpy(config.ftp_password,
    doc["ftp_password"] | password,
    sizeof(config.ftp_password)); 

  strlcpy(config.ftp_dir,
    doc["ftp_dir"] | ftp_dir,
    sizeof(config.ftp_dir));   

  BDH = config.BDH;
  stationName = config.stationName;
  PublicIP = config.PublicIP;
  captureMode = config.captureMode;
  Batt_low_SP = config.Batt_low_SP;
  hostname = config.ftp_hostname;
  username = config.ftp_username;
  password = config.ftp_password;
  ftp_dir = config.ftp_dir;
  // Close the file (Curiously, File's destructor doesn't close the file)
  file.close();
}

void saveConfiguration(const char *filename, const Config &config) {
  // Delete existing file, otherwise the configuration is appended to the file
  SD.remove(filename);

  // Open file for writing
  File file = SD.open(filename, FILE_WRITE);
  if (!file) {
    log("Failed to create config file", 2);
    return;
  }

  // Allocate a temporary JsonDocument
  // Don't forget to change the capacity to match your requirements.
  // Use arduinojson.org/assistant to compute the capacity.
  StaticJsonDocument<512> doc;

  doc["BDH"] = BDH;
  doc["stationName"] = stationName;
  doc["PublicIP"] = PublicIP;
  doc["Capture_Mode"] = captureMode;
  doc["Batt_low_SP"] = Batt_low_SP;
  doc["ftp_hostname"] = hostname;
  doc["ftp_username"] = username;
  doc["ftp_password"] = password;
  doc["ftp_dir"] = ftp_dir;

  // Serialize JSON to file
  if (serializeJson(doc, file) == 0) {
    log("Failed to write to file", 2);
  }

  // Close the file
  file.close();
}

void printFile(const char *filename) {
  // Open file for reading
  File file = SD.open(filename);
  if (!file) {
    Serial.println(F("Failed to read file"));
    return;
  }

  // Extract each characters by one by one
  while (file.available()) {
    Serial.print((char)file.read());
  }
  Serial.println();

  // Close the file
  file.close();
}

void readPasvStream() {
  int count = 0;
  while (ftp.data.connected()) {
    while (ftp.data.available()) {
      char c = ftp.data.read();
      count++;
      Serial.print(c);
    }
  }
}

void handler(const char *topic, const char *data) {
    log("received from Particle cloud " + String(topic) + ": " + String(data), 4);
    if(String(topic) == "particle/device/name") {
      BDH = data;
      }
    if(String(topic) == "particle/device/ip") {
      PublicIP = data;
      }
}

void initSD() {
  if (!SD.begin(SD_CS)) {
    Serial.println("SD Card initialization failed!! Resetting in 5 seconds");
    digitalWrite(statusLed, HIGH);
    delay(5000);
    System.reset();
  }
}

bool syncFTP(String SDfilename, bool retry, String dir, bool islog) {
  
  static const size_t bufferSize = 512;
  static uint8_t buffer[bufferSize] = {0xFF};
  String DestFilename = "";
  char buffersprint1 [50];
  char buffersprint2 [50];

  // Connect to host and authenticate, or halt
  if (!ftp.open(hostname, port, timeout)) {
    log("Couldn't connect to ftp host", 2);
    return 0;
  }
  if (!ftp.user(username)) {
    log("Bad ftp username", 2);
    return 0;
  }
  if (!ftp.pass(password)) {
    log("Bad ftp password", 2);
    return 0;
  }

  if (!ftp.cwd(dir)){
    log("could not set directory", 3);
    if(!ftp.mkd(dir)){
      log("ftp mkdir failed: " + dir, 2);
      return 0;
    }
    log("ftp mkdir success: " + dir, 3);
  }

  if(!islog) {
    sprintf(buffersprint1,"%s_%d%02d%s",stationName.c_str(), now.year(),now.month(),SDfilename.c_str());
    DestFilename = buffersprint1;

    sprintf(buffersprint2,"%s/%d/%02d/%s",stationName.c_str(),now.year(), now.month(),SDfilename.c_str());
    SDfilename = buffersprint2;
  } else  {
      sprintf(buffersprint2,"/%s/%s", "LOGS",SDfilename.c_str());
      DestFilename = SDfilename;
      SDfilename = buffersprint2;
    }
 
  if (!ftp.list(DestFilename)) {
    log("could not find : " + DestFilename, 4);
  } else {/*
      while(ftp.data.available()) {
        log("Found: " + String(ftp.data.read()), 4);
      }*/
      ftp.data.flush();
      ftp.finish();
  }

  delay(1000);
  if(retry == true) {
    if(!ftp.dele(DestFilename)){
      log("could not delete file", 2);
    //return 0;
    }
    log("partial file deleted, attempting rewrite", 4);
  }

  log("Transferring file: " + SDfilename + " to " + DestFilename , 4);


  File SDfile = SD.open(SDfilename);
  delay(100);
  if (SDfile) {
    if(!islog) {
      if (!ftp.type("I")) {
        //log("Couldn't set file type", 2);
      } else if (!ftp.stor(DestFilename)) {
        //log("Couldn't STOR file: " + DestFilename, 2);
        return 0;
      }
    } else if (islog) {
        if (!ftp.type("A")) {
        //log("Couldn't set file type", 2);
      } else if (!ftp.stor(DestFilename)) {
        //log("Couldn't STOR file: " + DestFilename, 2);
        return 0;
      }  
  }

  size_t len = SDfile.size();
  if (len >= 0x07ffff) {
      //log("Over size file.", 2);
      return 0;
    } else if (len == 0 ){
      //log("File size is 0.", 2);
      return 0;
    } 

    //log("length of file is: " + len, 4);
    uint8_t temp_last;
    uint8_t temp;
    int bytesRead = 0;
    unsigned long previousMillis = 0; 
    const long interval = 1000;
    tx_buffer_index = 0;

    while(SDfile.available()) {
      Particle.process();
      temp_last = temp;
      temp = SDfile.read();
      bytesRead++;
      buffer[tx_buffer_index++] = temp;

      if (tx_buffer_index >= TX_BUFFER_MAX) {
          digitalWrite(statusLed, LOW);
          ftp.data.write(buffer, tx_buffer_index);
          tx_buffer_index = 0;
          Particle.process();
          digitalWrite(statusLed, HIGH);
      }
      delayMicroseconds(2000);
      if (bytesRead > 500000) {
          // failsafe
          return 0;
      }
      if (!ftp.connected()) {
        //log("connection failed", 2);
        return 0;
      }       
    }

    if (tx_buffer_index != 0) {
          ftp.data.write(buffer, tx_buffer_index);
    }        
    Particle.process();
    SDfile.close();
  } else {
    log("Could not open File on SD... Aborting", 2);
    return 0;
    }
    ftp.data.flush();
  if (!ftp.finish()) {
    Timeout_fail:
    log("Couldn't stop file upload", 2);
    return 0;
  }
  if (!ftp.quit()) {
    log("Couldn't quit FTP", 2);
    return 0;
  } else {
    log("Transfer finished!", 4);
    digitalWrite(statusLed, LOW);
  }

}

bool getConfig(String dir) {
    // Connect to host and authenticate, or halt
    log("updating config file", 4);
    
    if (!ftp.open(hostname, port, timeout)) {
      Serial.println("Couldn't connect to ftp host");
      while (1);
    }
    if (!ftp.user(username)) {
      Serial.println("Bad username");
      while (1);
    }
    if (!ftp.pass(password)) {
      Serial.println("Bad password");
      while (1);
    }  

    if (!ftp.cwd(dir)){
      log("could not set directory", 3);
    if(!ftp.mkd(dir)){
      log("ftp mkdir failed: " + dir, 2);
      return 0;
    }
    log("ftp mkdir success: " + dir, 3);
  }
    // Test retrieving the file
    Serial.println("RETR");
    if (!ftp.retr(ftp_dir + "/config/" + BDH + "/CONFIG.JSN")) {
      Serial.println("Couldn't retrieve CONFIG.JSN");
      return 0;
    } else {
      if(SD.exists("CONFIG.JSN")) {
        SD.remove("CONFIG.JSN");
      }
      File file = SD.open("CONFIG.JSN",  FILE_WRITE);

      while (ftp.data.connected()) {
        while (ftp.data.available()) {
        file.write(ftp.data.read());
        }
      }
      file.close();
      ftp.finish();
      ftp.quit();

      log("Configuration updated", 4);
      loadConfiguration("CONFIG.JSN", config);
      
    }
}

void setup() {
  //Particle.keepAlive(120);
  Particle.process();
  Particle.function("Grab Pic", grabPic);
  Particle.variable("Station Name", stationName);
  Particle.variable("IPAdress", PublicIP);
  Particle.subscribe("particle/device/ip", handler, MY_DEVICES);
  Particle.subscribe("particle/device/name", handler);  
  Particle.publishVitals();
  Particle.publishVitals(300);
  Time.zone(-5);
  pinMode(Set_rly, OUTPUT);
  pinMode(Reset_rly, OUTPUT);
  pinMode(statusLed, OUTPUT);
  pinMode(D8, INPUT_PULLUP);

  Particle.publish("status", "Good morning, Version: " + String(VERSION_SLUG));
  delay(1000);

  uint8_t vid,pid;
  uint8_t temp;

  Wire.setSpeed(CLOCK_SPEED_100KHZ);
  Wire.begin();
  
  rtc.begin();
  Serial.begin(115200);
  rtc.clear_rtc_interrupt_flags();
  //Initialize SD Card
  initSD();
  log("Initializing " + String(VERSION_SLUG), 4);

  if (!rtc.isrunning()) {
    log("RTC is NOT running!", 2);
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
    delay(5000);
    System.reset();
  } else {
      rtc.config();
      rtc.clear_rtc_interrupt_flags();
    }
  
  // set the SPI_CS as an output:
  pinMode(D7, OUTPUT);
  pinMode(SPI_CS, OUTPUT);
  digitalWrite(SPI_CS, HIGH);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);

  // initialize SPI:
  SPI1.begin(SPI_CS);
  //SPI.begin(SPI_MODE_MASTER);
  //SPI.begin(SPI_MODE_SLAVE, SPI_CS);
  
  while(1) {
    digitalWrite(Set_rly, HIGH); //Powering Camera
    delay(200);
    digitalWrite(Set_rly, LOW);
    delay(500);  
    //Particle.publish("status", "checking for camera");
    //Serial.println("Checking for camera...");

    //Check if the ArduCAM SPI bus is OK
    myCAM.write_reg(ARDUCHIP_TEST1, 0x55);
    temp = myCAM.read_reg(ARDUCHIP_TEST1);
    if(temp != 0x55)
    {
      log("SPI interface Error! resetting in 5 seconds, myCam.read_reg said " + String(temp), 1);
      delay(5000);
      System.reset();
    }
    else {
      break;
    }
    Particle.process();
  }

  while(1){
    Particle.process();
    //Check if the camera module type is OV5642
    // myCAM.rdSensorReg8_8(OV2640_CHIPID_HIGH, &vid);
    // myCAM.rdSensorReg8_8(OV2640_CHIPID_LOW, &pid);

    myCAM.wrSensorReg8_8(0xff, 0x01);
    myCAM.rdSensorReg8_8(OV2640_CHIPID_HIGH, &vid);
    myCAM.rdSensorReg8_8(OV2640_CHIPID_LOW, &pid);

    if ((vid != 0x26 ) && (( pid != 0x41 ) || ( pid != 0x42 ))){
      log("Can't find OV2640 module! camera says " + String::format("%d:%d", vid, pid), 1);
      delay(5000);
      continue;
    }
    else { 
      break;
    }
  }

  //myCAM.write_reg(ARDUCHIP_MODE, 0x01);		 	//Switch to CAM

  //Change MCU mode
  myCAM.set_format(JPEG);
  delay(100);

  myCAM.InitCAM();
  delay(100);

  //    myCAM.set_format(JPEG);
  //    delay(100);

  //myCAM.set_bit(ARDUCHIP_TIM, VSYNC_LEVEL_MASK);
  //myCAM.write_reg(ARDUCHIP_TIM, VSYNC_LEVEL_MASK);
  //delay(100);

  //myCAM.write_reg(ARDUCHIP_FRAMES,0x00);
  //delay(100);

  //myCAM.OV5642_set_JPEG_size(OV5642_320x240);   //works
  //myCAM.OV5642_set_JPEG_size(OV5642_640x480);   // works
  //myCAM.OV5642_set_JPEG_size(OV5642_1600x1200); // works
  //myCAM.OV5642_set_JPEG_size(OV5642_1280x960);  // works
  myCAM.OV2640_set_JPEG_size(OV2640_1600x1200); //works
  //myCAM.OV2640_set_JPEG_size(OV2640_1280x1024);

  myCAM.clear_fifo_flag();

  //Load config file from SD
  loadConfiguration(configFileName,config);
}

int grabPic(String Short_filename) {
  
  digitalWrite(Set_rly, HIGH); //Powering Camera
  delay(200);
  digitalWrite(Set_rly, LOW);
  delay(500);

  myCAM.flush_fifo();
  delay(100);
  myCAM.clear_fifo_flag();
  delay(100);
  myCAM.start_capture();
  delay(100);

  unsigned long start_time = millis(),
  last_publish = millis();
  //
  //  wait for the photo to be done
  //
  while(!myCAM.get_bit(ARDUCHIP_TRIG , CAP_DONE_MASK)) {
      Particle.process();
      delay(10);

      unsigned long now = millis();
      if ((now - last_publish) > 1000) {
          last_publish = now;
      }

      if ((now-start_time) > 30000) {
          log("Photo timeout", 1);
          break;
      }
  }
  delay(100);

  int length = myCAM.read_fifo_length();
  log("Image size is " + String(length), 4);

  uint8_t temp = 0xff, temp_last = 0;
  int bytesRead = 0;
  String Pic = "";

  if(myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK))
  {
    byte buf[256];
    static int k = 0;
    uint8_t temp, temp_last;
    File file;
    String dir;
    myCAM.CS_HIGH();
    delay(100);
    //Open the new file
    
    if(!offlinemode) {
    dir = stationName + "/";
    dir += String(now.year()) + "/";
    dir += String(now.month()) + "/";
    } else {
        SD.open("/");
        file.openNextFile();
        if(file.isDirectory()) {
          BDH = file.name();
          } else {
              //log("Could not retrieve Station Name. Using default", 3);
        }
        file.close();
    }

    if(!SD.exists(dir)){
      //log("Directory does not exist, trying to create it...", 3);
      if(!SD.mkdir(dir)){
      //log("SD mkdir failed", 2);
      } 
    } 

    String namePlusDir = "";
    namePlusDir = dir + Short_filename;
    file = SD.open(namePlusDir, O_WRITE | O_CREAT | O_TRUNC);
    //file = SD.open(str, FILE_WRITE);
    delay(100);
    if(!file){
      //log("open file failed: " + namePlusDir, 1);
      myCAM.flush_fifo();
      delay(100);
      myCAM.clear_fifo_flag();
      delay(5000);
      return false;
    }
    file.write(0xFF);
    myCAM.CS_LOW();
    delay(100);
    //log("Capture Done.", 4);
    Particle.publish("status", "Capture done");
    myCAM.set_fifo_burst();

    tx_buffer_index = 0;
    temp = 0;
    while( (temp != 0xD9) | (temp_last != 0xFF) )
    {
      temp_last = temp;
      temp = myCAM.read_fifo();
      bytesRead++;
      buffer[tx_buffer_index++] = temp;
      if (tx_buffer_index >= TX_BUFFER_MAX) {
          file.write(buffer, tx_buffer_index);
          tx_buffer_index = 0;
          Particle.process();
      }
      if (bytesRead > 150000) {
          // failsafe
          break;
      }
    }
    if (tx_buffer_index != 0) {
        file.write(buffer, tx_buffer_index);
    }          
        
    //Clear the capture done flag
    myCAM.CS_HIGH();
    myCAM.clear_fifo_flag();
    file.close();
    digitalWrite(Reset_rly, HIGH);
    delay(200);
    digitalWrite(Reset_rly, LOW);
    return 1;
  }
}

bool cleanSD() {

}

void loop() {
  int secstoTimeout = timeout;
  String Nametocard;
  uint8_t buf[4];
  Particle.process(); 
   
  SleepResult result = System.sleepResult();

  if(result.wokenUpByRtc() || result.wokenUpByPin() ){
    log("woken up", 4);
    System.reset();
  }   
  
  log("waiting for cell connection", 4); 
  unsigned long previousMillis = millis();
  unsigned long thatTook = 0;
  while(!Cellular.ready()){
      delay(1000);
      Particle.process();
      secstoTimeout--;
      if (secstoTimeout <= 0) {
        offlinemode = true;
        log("Timeout, Setting mode to offline", 3);
        break;
      }
    }
  thatTook =  (millis() - previousMillis)/1000;
  log("That took " + String(thatTook) + "Secs", 4);

  getConfig(ftp_dir + "/config/" + BDH + "/");

  if(!offlinemode) {
    Particle.publish("particle/device/ip", PRIVATE);
    Particle.publish("particle/device/name");
    String SOC = String(fuel.getSoC()) + "%";
    CellularSignal sig = Cellular.RSSI();
    Particle.publish("Batt_SOC",SOC);
    log("SOC = " + String(SOC), 4);
    log("RSSI = " + String(sig), 4);
    Particle.publish("RSSI",String(sig.rssi));
    Particle.syncTime();
    while(!Particle.syncTimeDone() && timeout > 0) {
      delay(500);
      timeout --;
    }
    if(timeout > 0) {
      rtc.adjust(DateTime(Time.year(), Time.month(), Time.day(), Time.hour(), Time.minute(), Time.second()));
      Particle.syncTimeDone();
      Particle.publish("Status","Time Synced!");
      log("Time Synced!", 4);
     } else {
       log("Time Sync failed!", 2);
     }
    
    while(BDH == "undef"){
      Particle.process();
    }
    saveConfiguration(configFileName,config);
  }

  now = rtc.now();

    Serial.print(now.year(), DEC);
    Serial.print('/');
    Serial.print(now.month(), DEC);
    Serial.print('/');
    Serial.print(now.day(), DEC);
    Serial.print(' ');
    Serial.print(now.hour(), DEC);
    Serial.print(':');
    Serial.print(now.minute(), DEC);
    Serial.print(':');
    Serial.print(now.second(), DEC);
    Serial.println();
  //Nametocard = "01153155.jpg";
  char buffer[40];
  sprintf(buffer,"%02d%02d%02d%02d.jpg", now.day(),now.hour(), now.minute(), now.second());
  Nametocard = buffer;
  if(!grabPic(Nametocard)){
    log("GrabPic failed, restarting... name of file: " + Nametocard, 1);
    delay(5000);
    System.reset();
  }   

  int attempts = 0;
  if(!syncFTP(Nametocard, false,ftp_dir, 0)){
      log("Img sync failed", 2);
      while(!syncFTP(Nametocard, true,ftp_dir, 0) && attempts < 3){
        log("Img sync failed again", 2);
        Particle.process();
        attempts++;
        delay(5000);
      }
    } else {
      log("Img sync success", 4);
    }   

  attempts = 0;

  if(!syncFTP(String(monthOfTheYear[now.month()-1]) + String(now.year()) + ".txt", true,ftp_dir + "/config/" + BDH, 1)){
      log("Log sync failed", 2);
      while(!syncFTP(String(monthOfTheYear[now.month()-1]) + String(now.year()) + "txt", true,ftp_dir + "/config/" + BDH, 1) && attempts < 3){
        log("Log sync failed again", 2);
        Particle.process();
        attempts++;
        delay(5000);
      }
    } else {
      log("Log sync success", 4);
      attempts = 0;
    }    

  Particle.publish("status", "Sleeping");
  
  now = rtc.now();
  rtc.clear_rtc_interrupt_flags();
  int secsToWakeup = 0;

  switch (captureMode)
  {
  case 0: //3 img/jour
    if(7 <= now.hour() && now.hour() < 11) {
        DateTime wakeupTime(DateTime(now.year(), now.month(), now.day(), 11, 0, 0));
        secsToWakeup = wakeupTime.secondstime() - now.secondstime();
      }
    else if(11 <= now.hour() && now.hour() < 15) {
        DateTime wakeupTime(DateTime(now.year(), now.month(), now.day(), 15, 0, 0));
        secsToWakeup = wakeupTime.secondstime() - now.secondstime();
      }
    else if(15 <= now.hour()) {
        DateTime wakeupTime(DateTime(now.year(), now.month(), now.day()+1, 7, 0, 0));
        secsToWakeup = wakeupTime.secondstime() - now.secondstime();
      }
    else if(now.hour() < 7) {
        DateTime wakeupTime(DateTime(now.year(), now.month(), now.day(), 7, 0, 0));
        secsToWakeup = wakeupTime.secondstime() - now.secondstime();
      }      
    break;

  case 2: //12 img/jour
    if(7 <= now.hour() && now.hour() < 18) {
        DateTime wakeupTime(DateTime(now.year(), now.month(), now.day(), now.hour()+1, 0, 0));
        secsToWakeup = wakeupTime.secondstime() - now.secondstime();
      } 
    else if (18 >= now.hour()) {
        DateTime wakeupTime(DateTime(now.year(), now.month(), now.day()+1, 7, 0, 0));
        secsToWakeup = wakeupTime.secondstime() - now.secondstime();
      } 
    else if(now.hour() < 7) {
        DateTime wakeupTime(DateTime(now.year(), now.month(), now.day(), 7, 0, 0));
        secsToWakeup = wakeupTime.secondstime() - now.secondstime();
      }
 
    break; 
  case 3: //24 img/jour
        DateTime wakeupTime(DateTime(now.year(),now.month(),now.day(),now.hour(),0,0) + TimeSpan(0,1,0,0));
        secsToWakeup = wakeupTime.secondstime() - now.secondstime();    
    break;   

  }
  /*
  rtc.set_alarm(wakeupTime.day(),wakeupTime.hour(),wakeupTime.minute());
  delay(100);
  rtc.get_alarm(buf);
  Serial.print("Alarm set at: ");
  Serial.print(buf[0]);
  Serial.print(" minute, ");
  Serial.print(buf[1]);
  Serial.print(" hour, ");
  Serial.print(buf[2]);      
  Serial.println(" day, ");  
  */     

  log("Sleeping for " + String(secsToWakeup) + " Secs" , 4);
  Cellular.off();
  System.sleep(D8,FALLING,secsToWakeup);
}

void log(String msg, int Loglevel) {
  Serial.println(msg);
  String dir ="logs/" ;
  DateTime logtime = rtc.now();
  String logFileName = dir + String(monthOfTheYear[logtime.month()-1]) + String(logtime.year()) + ".txt";
  File file;

  if(!SD.exists(dir)){
    //log("Directory does not exist, trying to create it...", 3);
    if(!SD.mkdir(dir)){
    //log("SD mkdir failed", 2);
    } 
  } 

  file = SD.open(logFileName,  FILE_WRITE);
  if(!file) {
    Serial.println("could not open log file");
    Serial.println(logFileName);
    return;
  } else {

    /*
    * Log levels:
    * 1: Critical error
    * 2: error
    * 3: warning
    * 4: info
    */

    switch (Loglevel)
    {
    case 1:
      file.print("CRITICAL ERROR    :");
      break;

    case 2:
      file.print("ERROR    :");
      break;

    case 3:
      file.print("WARNING    :");
      break;

    case 4:
      file.print("INFO    :");
      break;

    default:
      break;
    }
    
    file.print(String(logtime.year()) + String(logtime.month()) + String(logtime.day()) + String(logtime.hour()) + String(logtime.minute()) + String(logtime.second()) +": ");
    file.println(msg);
    
    file.close();
  }
}