//./build-local.sh tcp_photographer
//particle flash --usb target/1.4.2/boron/OVFork.bin
SYSTEM_MODE(AUTOMATIC);
SYSTEM_THREAD(ENABLED);

#include "ArduCAM.h"
#include "memorysaver.h"
#include "ParticleFTPClient.h"
#include "SD.h"
#include "RTClib.h"

int statusLed = D7; 
FuelGauge fuel;

ApplicationWatchdog wd(30000, System.reset);

RTC_PCF8523 rtc;
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
DateTime now;

String hostname = "142.213.166.184";
String username = "bp1";
String password = "BlyuHet7";
/*
String hostname = "173.181.243.41";
String username = "6419_FTPCEL";
String password = "6419_IMYIKL";
*/
using namespace particleftpclient;
int port = 21;
int timeout = 50;
bool offlinemode = false;
int photo_interval = 240;

#define VERSION_SLUG "0.111"

#define TX_BUFFER_MAX 512
uint8_t buffer[TX_BUFFER_MAX + 1];
int tx_buffer_index = 0;

Sd2Card card;
SdVolume volume;

const int SPI_CS = A5;
const int SD_CS = D5;

String stationName ="undef";
int NTPSyncInt = 24;
String PublicIP = "0.0.0.0";
char str[50];
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

int UpdateInterval(String Interval) {
  photo_interval = Interval.toInt();
  Particle.publish("status", "Interval updated to " + String(photo_interval));
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
    Serial.println("received " + String(topic) + ": " + String(data));
    if(String(topic) == "particle/device/name") {
      stationName = data;
      }
    if(String(topic) == "particle/device/ip") {
      PublicIP = data;
      }
}

void initSD() {
  Serial.print("\nInitializing SD card...");
  if (!SD.begin(SD_CS)) {
    Serial.println("initialization failed!! Resetting in 5 seconds");
    digitalWrite(statusLed, HIGH);
    delay(5000);
    System.reset();
  } else {
    Serial.println("SD Ok!");
  }
}

bool syncFTP(String filename, bool retry) {
  
  static const size_t bufferSize = 512;
  static uint8_t buffer[bufferSize] = {0xFF};
  // Connect to host and authenticate, or halt
  if (!ftp.open(hostname, port, timeout)) {
    Serial.println("Couldn't connect to ftp host");
    return 0;
  }
  if (!ftp.user(username)) {
    Serial.println("Bad username");
    return 0;
  }
  if (!ftp.pass(password)) {
    Serial.println("Bad password");
    return 0;
  }

  if (!ftp.cwd("/Nouveau_CIDT")){
    Serial.println("could not set directory");
    return 0;
  }

  char buffersprint1 [50];
  char buffersprint2 [50];
  String DestFilename = "";
  sprintf(buffersprint1,"%s_%d%02d%s",stationName.c_str(), now.year(),now.month(),filename.c_str());
  DestFilename = buffersprint1;

  sprintf(buffersprint2,"%s/%d/%02d/%s",stationName.c_str(),now.year(), now.month(),filename.c_str());
  filename = buffersprint2;
 
  if (!ftp.list(DestFilename)) {
    Serial.print("could not list : ");
    Serial.println(DestFilename);
  } else {
      while(ftp.data.available()) {
        Serial.write(ftp.data.read());
      }
      ftp.data.flush();
      ftp.finish();
  }

  delay(1000);
  if(retry == true) {
    if(!ftp.dele(DestFilename)){
      Serial.println("could not delete file");
    //return 0;
    }
    Serial.println("Aborted file deleted, attempting rewrite");
  }

  // Test writing a file
  Serial.println("TYPE I (image file)");
  
  Serial.print("Trying to open file on SD: ");
  Serial.println(filename);
  File SDfile = SD.open(filename);
  delay(100);
    if (SDfile) {
      if (!ftp.type("I")) {
        Serial.println("Couldn't set file type");
      } else if (!ftp.stor(DestFilename)) {
        Serial.print("Couldn't STOR file: ");
        Serial.println(DestFilename);
        return 0;
      }
        Serial.println("STOR Img");
      size_t len = SDfile.size();
      if (len >= 0x07ffff) {
          Serial.println("Over size.");
          return 0;
        } else if (len == 0 ){
          Serial.println("Size is 0.");
          return 0;
        } 
        Serial.print("length of file is: "); Serial.println(len);
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
            Serial.println("connection failed");
            return 0;
          }       
        }
      

        if (tx_buffer_index != 0) {
             ftp.data.write(buffer, tx_buffer_index);
             Serial.println(100);
        }        
        Particle.process();
        SDfile.close();
      } else {
        Serial.println("Could not open File on SD... Aborting");
        return 0;
       }
    ftp.data.flush();
    if (!ftp.finish()) {
      Timeout_fail:
      Serial.println("Couldn't stop file upload");
      return 0;
    }
    // Quit!
  Serial.println("QUIT");
  if (!ftp.quit()) {
    Serial.println("Couldn't quit FTP");
    return 0;
  } else {
    Serial.println("Goodbye!");
    digitalWrite(statusLed, LOW);
  }

}

void setup() {
  //Particle.keepAlive(120);
  Particle.process();
  Particle.function("Grab Pic", grabPic);
  Particle.function("Photo interval", UpdateInterval);
  Particle.variable("Station Name", stationName);
  Particle.variable("IPAdress", PublicIP);
  Particle.subscribe("particle/device/ip", handler, MY_DEVICES);
  Particle.subscribe("particle/device/name", handler);  
  Particle.publishVitals(900);
  Time.zone(-5);
  pinMode(statusLed, OUTPUT);

  Particle.publish("status", "Good morning, Version: " + String(VERSION_SLUG));
  delay(1000);

  uint8_t vid,pid;
  uint8_t temp;

  Wire.setSpeed(CLOCK_SPEED_100KHZ);
  Wire.begin();
  rtc.begin();

  if (! rtc.initialized()) {
    Serial.println("RTC is NOT running!");
    // following line sets the RTC to the date & time this sketch was compiled
    //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }
  Serial.begin(115200);
  Serial.println("ArduCAM Start!");
  Serial.println(VERSION_SLUG);
  
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
    Particle.publish("status", "checking for camera");
    Serial.println("Checking for camera...");

    //Check if the ArduCAM SPI bus is OK
    myCAM.write_reg(ARDUCHIP_TEST1, 0x55);
    temp = myCAM.read_reg(ARDUCHIP_TEST1);
    if(temp != 0x55)
    {
      Serial.println("SPI interface Error! resetting in 5 seconds");
      Serial.println("myCam.read_reg said " + String(temp));
      delay(5000);
      System.reset();
    }
    else {
      break;
    }
    Particle.process();
  }

    Particle.publish("status", "Camera found.");

  while(1){
    Particle.process();
    //Check if the camera module type is OV5642
    // myCAM.rdSensorReg8_8(OV2640_CHIPID_HIGH, &vid);
    // myCAM.rdSensorReg8_8(OV2640_CHIPID_LOW, &pid);

    myCAM.wrSensorReg8_8(0xff, 0x01);
    myCAM.rdSensorReg8_8(OV2640_CHIPID_HIGH, &vid);
    myCAM.rdSensorReg8_8(OV2640_CHIPID_LOW, &pid);

    if ((vid != 0x26 ) && (( pid != 0x41 ) || ( pid != 0x42 ))){
      Particle.publish("status", "Can't find OV2640 module!");
      Particle.publish("status", "Not found, camera says " + String::format("%d:%d", vid, pid));
      delay(5000);
      continue;
    }
    else {
      Serial.println(F("OV5642 detected."));
      Particle.publish("status", "OV2640 detected: " + String::format("%d:%d", vid, pid));
      break;
    }
  }


  Serial.println("Camera found, initializing...");
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
    //myCAM.OV5642_set_JPEG_size(OV5642_2592x1944); //works
     myCAM.OV2640_set_JPEG_size(OV2640_1280x1024);

     myCAM.clear_fifo_flag();

    // wait a sec`
    delay(1000);
    //Initialize SD Card
    initSD();
}

int grabPic(String Short_filename) {
  Particle.publish("status", "Taking a picture...");
  Serial.println("Taking a picture...");
  Serial.println(Short_filename);

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
          Particle.publish("status", "waiting for photo " + String(now-start_time));
          last_publish = now;
      }

      if ((now-start_time) > 30000) {
          Particle.publish("status", "bailing...");
          break;
      }
  }
  delay(100);

  int length = myCAM.read_fifo_length();
  Particle.publish("status", "Image size is " + String(length));
  Serial.println("Image size is " + String(length));

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
          stationName = file.name();
          } else {
              Serial.println("Could not retrieve Station Name. Using default");
        }
        file.close();
    }

    if(!SD.exists(dir)){
      Serial.println("Directory does not exist, trying to create it...");
      if(!SD.mkdir(dir)){
      Serial.println("mkdir failed");
      } 
    } 

    String namePlusDir = "";
    Serial.println(Short_filename);
    namePlusDir = dir + Short_filename;
    Serial.println(namePlusDir);

    file = SD.open(namePlusDir, O_WRITE | O_CREAT | O_TRUNC);
    //file = SD.open(str, FILE_WRITE);
    delay(100);
    if(!file){
      Serial.print("open file failed: ");
      Serial.println(namePlusDir);
      myCAM.flush_fifo();
      delay(100);
      myCAM.clear_fifo_flag();
      delay(5000);
      return false;
    }
    file.write(0xFF);
    myCAM.CS_LOW();
    delay(100);
    Serial.println(F("ACK CMD CAM Capture Done."));
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
    Particle.publish("status", "End of Photo");
        
    //Clear the capture done flag
    myCAM.CS_HIGH();
    myCAM.clear_fifo_flag();
    file.close();
    Serial.println(F("End of Photo"));
    return 1;
  }
}

bool cleanSD() {

}

void loop() {
  int secstoTimeout = timeout;
  String Nametocard;
  Particle.process();   
  
  Serial.print("waiting for particle connection"); 
  while(!Particle.connected()){
      Serial.print(".");
      delay(1000);
      Particle.process();
      secstoTimeout--;
      if (secstoTimeout <= 0) {
        offlinemode = true;
        break;
      }
    }

  Serial.println("");
  if(!offlinemode) {
    Particle.publish("particle/device/ip", PRIVATE);
    Particle.publish("particle/device/name");
    String SOC = String(fuel.getSoC()) + "%";
    CellularSignal sig = Cellular.RSSI();
    Particle.publish("Batt_SOC",SOC);
    Particle.publish("RSSI",String(sig.qual));
    if (now.hour() == 7) {
        Particle.syncTime();
        while(Particle.syncTimePending()) {
          delay(1);
        rtc.adjust(DateTime(Time.year(), Time.month(), Time.day(), Time.hour(), Time.minute(), Time.second()));
        Particle.publish("Status","Time Synced!");
      }
    }
    while(stationName == "undef"){
      Particle.process();
    }
  }

  now = rtc.now();
  //Nametocard = "01153155.jpg";
  char buffer[40];
  sprintf(buffer,"%02d%02d%02d%02d.jpg", now.day(),now.hour(), now.minute(), now.second());
  Nametocard = buffer;
  Serial.println(Nametocard);
  if(!grabPic(Nametocard)){
    Serial.println("GrabPic failed, restarting in 10 secs");
    delay(10000);
    System.reset();
  }   

  int attempts = 0;
  if(!syncFTP(Nametocard, false)){
      Serial.println("Syncftp failed");
      while(!syncFTP(Nametocard, true) && attempts < 3){
        Serial.println("Syncftp failed");
        Particle.process();
        attempts++;
        delay(5000);
      }
    } else {
      Serial.println("Syncftp success");
    }   


  Serial.println("sleeping 60 seconds");
  Particle.publish("status", "Sleeping 60 seconds");
  
  System.sleep(D1,RISING,3600*1000);
}
