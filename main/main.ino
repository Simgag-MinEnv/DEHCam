//./build-local.sh main
//particle flash --usb target/1.5.2/boron/DEHCam.bin
//particle flash --usb target/2.0.0/boron/DEHCam.bin
//particle flash 44515 target/2.0.0/boron/DEHCam.bin
SYSTEM_MODE(AUTOMATIC);
SYSTEM_THREAD(ENABLED);

#include "ArduCAM.h"
#include "memorysaver.h"
#include "ParticleFTPClient.h"
#include "SD.h"
#include "PCF8523.h"
#include "ArduinoJson.h"
//#include "ov2640_regs.h"

#define ArduCAM_H

const int statusLed = D7; 
const int SPI_CS = D5;
const int SD_CS = A5;
const int Cam_on = A0;
const int SS_Button = D6;
FuelGauge fuel;

ApplicationWatchdog wd(90000, WDevent);

PCF8523 rtc;
char monthOfTheYear[12][5] = {"JAN", "FEV", "MAR", "AVR", "MAI", "JUN", "JUL", "AOU", "SEP", "OCT", "NOV", "DEC"};
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
int timeout = 200;
//ool offlineMode = false;
bool offlineMode = false;
bool cloudOutage = false;
int Batt_low_SP = 330;

#define VERSION_SLUG "V1.433" //2020/12/07

#define TX_BUFFER_MAX 256
uint8_t buffer[TX_BUFFER_MAX + 1];
int tx_buffer_index = 0;

String configFileName = "CONFIG.JSN";

const int LogEntries = 50;
int logBufIndex = 0;
String logbuffer[LogEntries];

String stationName = "UNDEF_";
String BDH = "00000";
String PublicIP = "no_IP";
int captureMode = 0;



////////////////////////Template EXIF/////////////////////////////////////////


  const uint8_t exif1[86] PROGMEM = { 0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46, 0x49, 0x46, 0x00, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xE1, 0x10, 0xF6, 0x45, 0x78, 0x69, 0x66, 0x00, 0x00, 0x4D, 0x4D, 0x00, 0x2A, 0x00, 0x00, 0x00, 0x08, 0x00, 0x03, 0x87, 0x69, 0x00, 0x04, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x08, 0x3E, 0x9C, 0x9E, 0x00, 0x01, 0x00, 0x00, 0x00, 0x90, 0x00, 0x00, 0x10, 0x5E, 0xEA, 0x1C, 0x00, 0x07, 0x00, 0x00, 0x08, 0x0C, 0x00, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 0x00, 0x1C, 0xEA, 0x00, 0x00, 0x00, 0x08};
  // puis une série de 2055 "00"
  // puis une courte string
  const uint8_t exif2[23] PROGMEM = { 0x01, 0xEA, 0x1C, 0x00, 0x07, 0x00, 0x00, 0x08, 0x0C, 0x00, 0x00, 0x08, 0x50, 0x00, 0x00, 0x00, 0x00, 0x1C, 0xEA, 0x00, 0x00, 0x00, 0x08};
  // puis une série de 2056 "00" 
  // puis le header de tags, Vbat à l'index 10 (#...#.#.), SOC: 28 (#.#...#.), date: 50 (A.A.A.A.M.M.D.D.), heure: 80 (H.H.M.M.S.S.), BDH: 102, Station: 130
  uint8_t exifInfo3[140] = {0x56, 0x00, 0x62, 0x00, 0x61, 0x00, 0x74, 0x00, 0x3B, 0x00, 0x31, 0x00, 0x33, 0x00, 0x2E, 0x00, 0x37, 0x00, 0x3B, 0x00, 0x52, 0x00, 0x53, 0x00, 0x53, 0x00, 0x49, 0x00, 0x3B, 0x00, 0x37, 0x00, 0x38, 0x00, 0x2E, 0x00, 0x3B, 0x00, 0x44, 0x00, 0x61, 0x00, 0x74, 0x00, 0x65, 0x00, 0x3B, 0x00, 0x32, 0x00, 0x30, 0x00, 0x31, 0x00, 0x39, 0x00, 0x31, 0x00, 0x32, 0x00, 0x31, 0x00, 0x38, 0x00, 0x3B, 0x00, 0x68, 0x00, 0x65, 0x00, 0x75, 0x00, 0x72, 0x00, 0x65, 0x00, 0x3B, 0x00, 0x31, 0x00, 0x33, 0x00, 0x30, 0x00, 0x31, 0x00, 0x30, 0x00, 0x31, 0x00, 0x3B, 0x00, 0x42, 0x00, 0x44, 0x00, 0x48, 0x00, 0x3B, 0x00, 0x34, 0x00, 0x34, 0x00, 0x35, 0x00, 0x31, 0x00, 0x35, 0x00, 0x3B, 0x00, 0x53, 0x00, 0x54, 0x00, 0x41, 0x00, 0x54, 0x00, 0x49, 0x00, 0x4F, 0x00, 0x4E, 0x00, 0x3B, 0x00, 0x30, 0x00, 0x31, 0x00, 0x32, 0x00, 0x32, 0x00, 0x33, 0x00, 0x33, 0x00 }; 
  // puis divers infos EXIF
  const uint8_t exif4[637] PROGMEM = {0x0, 0x0, 0xFF, 0xE1, 0x0D, 0xFC, 0x68, 0x74, 0x74, 0x70, 0x3A, 0x2F, 0x2F, 0x6E, 0x73, 0x2E, 0x61, 0x64, 0x6F, 0x62, 0x65, 0x2E, 0x63, 0x6F, 0x6D, 0x2F, 0x78, 0x61, 0x70, 0x2F, 0x31, 0x2E, 0x30, 0x2F, 0x0, 0x3C, 0x3F, 0x78, 0x70, 0x61, 0x63, 0x6B, 0x65, 0x74, 0x20, 0x62, 0x65, 0x67, 0x69, 0x6E, 0x3D, 0x27, 0xEF, 0xBB, 0xBF, 0x27, 0x20, 0x69, 0x64, 0x3D, 0x27, 0x57, 0x35, 0x4D, 0x30, 0x4D, 0x70, 0x43, 0x65, 0x68, 0x69, 0x48, 0x7A, 0x72, 0x65, 0x53, 0x7A, 0x4E, 0x54, 0x63, 0x7A, 0x6B, 0x63, 0x39, 0x64, 0x27, 0x3F, 0x3E, 0x0D, 0x0A, 0x3C, 0x78, 0x3A, 0x78, 0x6D, 0x70, 0x6D, 0x65, 0x74, 0x61, 0x20, 0x78, 0x6D, 0x6C, 0x6E, 0x73, 0x3A, 0x78, 0x3D, 0x22, 0x61, 0x64, 0x6F, 0x62, 0x65, 0x3A, 0x6E, 0x73, 0x3A, 0x6D, 0x65, 0x74, 0x61, 0x2F, 0x22, 0x3E, 0x3C, 0x72, 0x64, 0x66, 0x3A, 0x52, 0x44, 0x46, 0x20, 0x78, 0x6D, 0x6C, 0x6E, 0x73, 0x3A, 0x72, 0x64, 0x66, 0x3D, 0x22, 0x68, 0x74, 0x74, 0x70, 0x3A, 0x2F, 0x2F, 0x77, 0x77, 0x77, 0x2E, 0x77, 0x33, 0x2E, 0x6F, 0x72, 0x67, 0x2F, 0x31, 0x39, 0x39, 0x39, 0x2F, 0x30, 0x32, 0x2F, 0x32, 0x32, 0x2D, 0x72, 0x64, 0x66, 0x2D, 0x73, 0x79, 0x6E, 0x74, 0x61, 0x78, 0x2D, 0x6E, 0x73, 0x23, 0x22, 0x3E, 0x3C, 0x72, 0x64, 0x66, 0x3A, 0x44, 0x65, 0x73, 0x63, 0x72, 0x69, 0x70, 0x74, 0x69, 0x6F, 0x6E, 0x20, 0x72, 0x64, 0x66, 0x3A, 0x61, 0x62, 0x6F, 0x75, 0x74, 0x3D, 0x22, 0x75, 0x75, 0x69, 0x64, 0x3A, 0x66, 0x61, 0x66, 0x35, 0x62, 0x64, 0x64, 0x35, 0x2D, 0x62, 0x61, 0x33, 0x64, 0x2D, 0x31, 0x31, 0x64, 0x61, 0x2D, 0x61, 0x64, 0x33, 0x31, 0x2D, 0x64, 0x33, 0x33, 0x64, 0x37, 0x35, 0x31, 0x38, 0x32, 0x66, 0x31, 0x62, 0x22, 0x20, 0x78, 0x6D, 0x6C, 0x6E, 0x73, 0x3A, 0x64, 0x63, 0x3D, 0x22, 0x68, 0x74, 0x74, 0x70, 0x3A, 0x2F, 0x2F, 0x70, 0x75, 0x72, 0x6C, 0x2E, 0x6F, 0x72, 0x67, 0x2F, 0x64, 0x63, 0x2F, 0x65, 0x6C, 0x65, 0x6D, 0x65, 0x6E, 0x74, 0x73, 0x2F, 0x31, 0x2E, 0x31, 0x2F, 0x22, 0x2F, 0x3E, 0x3C, 0x72, 0x64, 0x66, 0x3A, 0x44, 0x65, 0x73, 0x63, 0x72, 0x69, 0x70, 0x74, 0x69, 0x6F, 0x6E, 0x20, 0x72, 0x64, 0x66, 0x3A, 0x61, 0x62, 0x6F, 0x75, 0x74, 0x3D, 0x22, 0x75, 0x75, 0x69, 0x64, 0x3A, 0x66, 0x61, 0x66, 0x35, 0x62, 0x64, 0x64, 0x35, 0x2D, 0x62, 0x61, 0x33, 0x64, 0x2D, 0x31, 0x31, 0x64, 0x61, 0x2D, 0x61, 0x64, 0x33, 0x31, 0x2D, 0x64, 0x33, 0x33, 0x64, 0x37, 0x35, 0x31, 0x38, 0x32, 0x66, 0x31, 0x62, 0x22, 0x20, 0x78, 0x6D, 0x6C, 0x6E, 0x73, 0x3A, 0x4D, 0x69, 0x63, 0x72, 0x6F, 0x73, 0x6F, 0x66, 0x74, 0x50, 0x68, 0x6F, 0x74, 0x6F, 0x3D, 0x22, 0x68, 0x74, 0x74, 0x70, 0x3A, 0x2F, 0x2F, 0x6E, 0x73, 0x2E, 0x6D, 0x69, 0x63, 0x72, 0x6F, 0x73, 0x6F, 0x66, 0x74, 0x2E, 0x63, 0x6F, 0x6D, 0x2F, 0x70, 0x68, 0x6F, 0x74, 0x6F, 0x2F, 0x31, 0x2E, 0x30, 0x2F, 0x22, 0x2F, 0x3E, 0x3C, 0x72, 0x64, 0x66, 0x3A, 0x44, 0x65, 0x73, 0x63, 0x72, 0x69, 0x70, 0x74, 0x69, 0x6F, 0x6E, 0x20, 0x72, 0x64, 0x66, 0x3A, 0x61, 0x62, 0x6F, 0x75, 0x74, 0x3D, 0x22, 0x75, 0x75, 0x69, 0x64, 0x3A, 0x66, 0x61, 0x66, 0x35, 0x62, 0x64, 0x64, 0x35, 0x2D, 0x62, 0x61, 0x33, 0x64, 0x2D, 0x31, 0x31, 0x64, 0x61, 0x2D, 0x61, 0x64, 0x33, 0x31, 0x2D, 0x64, 0x33, 0x33, 0x64, 0x37, 0x35, 0x31, 0x38, 0x32, 0x66, 0x31, 0x62, 0x22, 0x20, 0x78, 0x6D, 0x6C, 0x6E, 0x73, 0x3A, 0x64, 0x63, 0x3D, 0x22, 0x68, 0x74, 0x74, 0x70, 0x3A, 0x2F, 0x2F, 0x70, 0x75, 0x72, 0x6C, 0x2E, 0x6F, 0x72, 0x67, 0x2F, 0x64, 0x63, 0x2F, 0x65, 0x6C, 0x65, 0x6D, 0x65, 0x6E, 0x74, 0x73, 0x2F, 0x31, 0x2E, 0x31, 0x2F, 0x22, 0x3E, 0x3C, 0x64, 0x63, 0x3A, 0x73, 0x75, 0x62, 0x6A, 0x65, 0x63, 0x74, 0x3E, 0x3C, 0x72, 0x64, 0x66, 0x3A, 0x42, 0x61, 0x67, 0x20, 0x78, 0x6D, 0x6C, 0x6E, 0x73, 0x3A, 0x72, 0x64, 0x66, 0x3D, 0x22, 0x68, 0x74, 0x74, 0x70, 0x3A, 0x2F, 0x2F, 0x77, 0x77, 0x77, 0x2E, 0x77, 0x33, 0x2E, 0x6F, 0x72, 0x67, 0x2F, 0x31, 0x39, 0x39, 0x39, 0x2F, 0x30, 0x32, 0x2F, 0x32, 0x32, 0x2D, 0x72, 0x64, 0x66, 0x2D, 0x73, 0x79, 0x6E, 0x74, 0x61, 0x78, 0x2D, 0x6E, 0x73, 0x23, 0x22, 0x3E, 0x3C, 0x72, 0x64, 0x66, 0x3A, 0x6C, 0x69, 0x3E };	
  // puis à nouveau les tags sous un autre format, Vbat à l'index 22 (#.##), SOC: 63 (##.#), date: 106 (AAAAMMJJ), heure: 153 (HHMMSS), BDH: 196 (#####), Station: 242 (######)
  uint8_t exifInfo5[246] = { 0x56, 0x62, 0x61, 0x74, 0x3C, 0x2F, 0x72, 0x64, 0x66, 0x3A, 0x6C, 0x69, 0x3E, 0x3C, 0x72, 0x64, 0x66, 0x3A, 0x6C, 0x69, 0x3E, 0x31, 0x33, 0x2E, 0x37, 0x3C, 0x2F, 0x72, 0x64, 0x66, 0x3A, 0x6C, 0x69, 0x3E, 0x3C, 0x72, 0x64, 0x66, 0x3A, 0x6C, 0x69, 0x3E, 0x52, 0x53, 0x53, 0x49, 0x3C, 0x2F, 0x72, 0x64, 0x66, 0x3A, 0x6C, 0x69, 0x3E, 0x3C, 0x72, 0x64, 0x66, 0x3A, 0x6C, 0x69, 0x3E, 0x37, 0x38, 0x2E, 0x3C, 0x2F, 0x72, 0x64, 0x66, 0x3A, 0x6C, 0x69, 0x3E, 0x3C, 0x72, 0x64, 0x66, 0x3A, 0x6C, 0x69, 0x3E, 0x44, 0x61, 0x74, 0x65, 0x3C, 0x2F, 0x72, 0x64, 0x66, 0x3A, 0x6C, 0x69, 0x3E, 0x3C, 0x72, 0x64, 0x66, 0x3A, 0x6C, 0x69, 0x3E, 0x32, 0x30, 0x31, 0x39, 0x31, 0x32, 0x31, 0x38, 0x3C, 0x2F, 0x72, 0x64, 0x66, 0x3A, 0x6C, 0x69, 0x3E, 0x3C, 0x72, 0x64, 0x66, 0x3A, 0x6C, 0x69, 0x3E, 0x68, 0x65, 0x75, 0x72, 0x65, 0x3C, 0x2F, 0x72, 0x64, 0x66, 0x3A, 0x6C, 0x69, 0x3E, 0x3C, 0x72, 0x64, 0x66, 0x3A, 0x6C, 0x69, 0x3E, 0x31, 0x33, 0x30, 0x31, 0x30, 0x31, 0x3C, 0x2F, 0x72, 0x64, 0x66, 0x3A, 0x6C, 0x69, 0x3E, 0x3C, 0x72, 0x64, 0x66, 0x3A, 0x6C, 0x69, 0x3E, 0x42, 0x44, 0x48, 0x3C, 0x2F, 0x72, 0x64, 0x66, 0x3A, 0x6C, 0x69, 0x3E, 0x3C, 0x72, 0x64, 0x66, 0x3A, 0x6C, 0x69, 0x3E, 0x34, 0x34, 0x35, 0x31, 0x35, 0x3C, 0x2F, 0x72, 0x64, 0x66, 0x3A, 0x6C, 0x69, 0x3E, 0x3C, 0x72, 0x64, 0x66, 0x3A, 0x6C, 0x69, 0x3E, 0x53, 0x54, 0x41, 0x54, 0x49, 0x4F, 0x4E, 0x3C, 0x2F, 0x72, 0x64, 0x66, 0x3A, 0x6C, 0x69, 0x3E, 0x3C, 0x72, 0x64, 0x66, 0x3A, 0x6C, 0x69, 0x3E, 0x30, 0x31, 0x32, 0x32, 0x33, 0x33 }; 
  // D'autres infos inutiles...
  const uint8_t exif6[288] PROGMEM = { 0x3C, 0x2F, 0x72, 0x64, 0x66, 0x3A, 0x6C, 0x69, 0x3E, 0x3C, 0x2F, 0x72, 0x64, 0x66, 0x3A, 0x42, 0x61, 0x67, 0x3E, 0x0D, 0x0A, 0x9, 0x9, 0x9, 0x3C, 0x2F, 0x64, 0x63, 0x3A, 0x73, 0x75, 0x62, 0x6A, 0x65, 0x63, 0x74, 0x3E, 0x3C, 0x2F, 0x72, 0x64, 0x66, 0x3A, 0x44, 0x65, 0x73, 0x63, 0x72, 0x69, 0x70, 0x74, 0x69, 0x6F, 0x6E, 0x3E, 0x3C, 0x72, 0x64, 0x66, 0x3A, 0x44, 0x65, 0x73, 0x63, 0x72, 0x69, 0x70, 0x74, 0x69, 0x6F, 0x6E, 0x20, 0x72, 0x64, 0x66, 0x3A, 0x61, 0x62, 0x6F, 0x75, 0x74, 0x3D, 0x22, 0x75, 0x75, 0x69, 0x64, 0x3A, 0x66, 0x61, 0x66, 0x35, 0x62, 0x64, 0x64, 0x35, 0x2D, 0x62, 0x61, 0x33, 0x64, 0x2D, 0x31, 0x31, 0x64, 0x61, 0x2D, 0x61, 0x64, 0x33, 0x31, 0x2D, 0x64, 0x33, 0x33, 0x64, 0x37, 0x35, 0x31, 0x38, 0x32, 0x66, 0x31, 0x62, 0x22, 0x20, 0x78, 0x6D, 0x6C, 0x6E, 0x73, 0x3A, 0x4D, 0x69, 0x63, 0x72, 0x6F, 0x73, 0x6F, 0x66, 0x74, 0x50, 0x68, 0x6F, 0x74, 0x6F, 0x3D, 0x22, 0x68, 0x74, 0x74, 0x70, 0x3A, 0x2F, 0x2F, 0x6E, 0x73, 0x2E, 0x6D, 0x69, 0x63, 0x72, 0x6F, 0x73, 0x6F, 0x66, 0x74, 0x2E, 0x63, 0x6F, 0x6D, 0x2F, 0x70, 0x68, 0x6F, 0x74, 0x6F, 0x2F, 0x31, 0x2E, 0x30, 0x2F, 0x22, 0x3E, 0x3C, 0x4D, 0x69, 0x63, 0x72, 0x6F, 0x73, 0x6F, 0x66, 0x74, 0x50, 0x68, 0x6F, 0x74, 0x6F, 0x3A, 0x4C, 0x61, 0x73, 0x74, 0x4B, 0x65, 0x79, 0x77, 0x6F, 0x72, 0x64, 0x58, 0x4D, 0x50, 0x3E, 0x3C, 0x72, 0x64, 0x66, 0x3A, 0x42, 0x61, 0x67, 0x20, 0x78, 0x6D, 0x6C, 0x6E, 0x73, 0x3A, 0x72, 0x64, 0x66, 0x3D, 0x22, 0x68, 0x74, 0x74, 0x70, 0x3A, 0x2F, 0x2F, 0x77, 0x77, 0x77, 0x2E, 0x77, 0x33, 0x2E, 0x6F, 0x72, 0x67, 0x2F, 0x31, 0x39, 0x39, 0x39, 0x2F, 0x30, 0x32, 0x2F, 0x32, 0x32, 0x2D, 0x72, 0x64, 0x66, 0x2D, 0x73, 0x79, 0x6E, 0x74, 0x61, 0x78, 0x2D, 0x6E, 0x73, 0x23, 0x22, 0x3E, 0x3C, 0x72, 0x64, 0x66, 0x3A, 0x6C, 0x69, 0x3E }; 
  // Encore les mêmes tags...
  uint8_t exifInfo7[246] = { 0x56, 0x62, 0x61, 0x74, 0x3C, 0x2F, 0x72, 0x64, 0x66, 0x3A, 0x6C, 0x69, 0x3E, 0x3C, 0x72, 0x64, 0x66, 0x3A, 0x6C, 0x69, 0x3E, 0x31, 0x33, 0x2E, 0x37, 0x3C, 0x2F, 0x72, 0x64, 0x66, 0x3A, 0x6C, 0x69, 0x3E, 0x3C, 0x72, 0x64, 0x66, 0x3A, 0x6C, 0x69, 0x3E, 0x52, 0x53, 0x53, 0x49, 0x3C, 0x2F, 0x72, 0x64, 0x66, 0x3A, 0x6C, 0x69, 0x3E, 0x3C, 0x72, 0x64, 0x66, 0x3A, 0x6C, 0x69, 0x3E, 0x37, 0x38, 0x2E, 0x3C, 0x2F, 0x72, 0x64, 0x66, 0x3A, 0x6C, 0x69, 0x3E, 0x3C, 0x72, 0x64, 0x66, 0x3A, 0x6C, 0x69, 0x3E, 0x44, 0x61, 0x74, 0x65, 0x3C, 0x2F, 0x72, 0x64, 0x66, 0x3A, 0x6C, 0x69, 0x3E, 0x3C, 0x72, 0x64, 0x66, 0x3A, 0x6C, 0x69, 0x3E, 0x32, 0x30, 0x31, 0x39, 0x31, 0x32, 0x31, 0x38, 0x3C, 0x2F, 0x72, 0x64, 0x66, 0x3A, 0x6C, 0x69, 0x3E, 0x3C, 0x72, 0x64, 0x66, 0x3A, 0x6C, 0x69, 0x3E, 0x68, 0x65, 0x75, 0x72, 0x65, 0x3C, 0x2F, 0x72, 0x64, 0x66, 0x3A, 0x6C, 0x69, 0x3E, 0x3C, 0x72, 0x64, 0x66, 0x3A, 0x6C, 0x69, 0x3E, 0x31, 0x33, 0x30, 0x31, 0x30, 0x31, 0x3C, 0x2F, 0x72, 0x64, 0x66, 0x3A, 0x6C, 0x69, 0x3E, 0x3C, 0x72, 0x64, 0x66, 0x3A, 0x6C, 0x69, 0x3E, 0x42, 0x44, 0x48, 0x3C, 0x2F, 0x72, 0x64, 0x66, 0x3A, 0x6C, 0x69, 0x3E, 0x3C, 0x72, 0x64, 0x66, 0x3A, 0x6C, 0x69, 0x3E, 0x34, 0x34, 0x35, 0x31, 0x35, 0x3C, 0x2F, 0x72, 0x64, 0x66, 0x3A, 0x6C, 0x69, 0x3E, 0x3C, 0x72, 0x64, 0x66, 0x3A, 0x6C, 0x69, 0x3E, 0x53, 0x54, 0x41, 0x54, 0x49, 0x4F, 0x4E, 0x3C, 0x2F, 0x72, 0x64, 0x66, 0x3A, 0x6C, 0x69, 0x3E, 0x3C, 0x72, 0x64, 0x66, 0x3A, 0x6C, 0x69, 0x3E, 0x30, 0x31, 0x32, 0x32, 0x33, 0x33 };
  // encore un peu d'infos inutiles
  const uint8_t exif8[98] PROGMEM = { 0x3C, 0x2F, 0x72, 0x64, 0x66, 0x3A, 0x6C, 0x69, 0x3E, 0x3C, 0x2F, 0x72, 0x64, 0x66, 0x3A, 0x42, 0x61, 0x67, 0x3E, 0x0D, 0x0A, 0x9, 0x9, 0x9, 0x3C, 0x2F, 0x4D, 0x69, 0x63, 0x72, 0x6F, 0x73, 0x6F, 0x66, 0x74, 0x50, 0x68, 0x6F, 0x74, 0x6F, 0x3A, 0x4C, 0x61, 0x73, 0x74, 0x4B, 0x65, 0x79, 0x77, 0x6F, 0x72, 0x64, 0x58, 0x4D, 0x50, 0x3E, 0x3C, 0x2F, 0x72, 0x64, 0x66, 0x3A, 0x44, 0x65, 0x73, 0x63, 0x72, 0x69, 0x70, 0x74, 0x69, 0x6F, 0x6E, 0x3E, 0x3C, 0x2F, 0x72, 0x64, 0x66, 0x3A, 0x52, 0x44, 0x46, 0x3E, 0x3C, 0x2F, 0x78, 0x3A, 0x78, 0x6D, 0x70, 0x6D, 0x65, 0x74, 0x61, 0x3E, 0x0D, 0x0A };
  // Puis une série de 2048 "00"
  // Puis une balise de fin 
  const uint8_t exif9[20] PROGMEM = { 0x3C, 0x3F, 0x78, 0x70, 0x61, 0x63, 0x6B, 0x65, 0x74, 0x20, 0x65, 0x6E, 0x64, 0x3D, 0x27, 0x77, 0x27, 0x3F, 0x3E, 0xFF };
  //début de l'image

    int indexer = 0;
    const int Vbatindex1 = 10;
    const int Vbatindex2 = 21;
    const int RSSIindex1 = 30;
    const int RSSIindex2 = 63;
    const int DateIndex1 = 48;
    const int DateIndex2 = 104;
    const int HeureIndex1 = 78;
    const int HeureIndex2 = 151;
    const int BDHIndex1 = 100;
    const int BDHIndex2 = 194;
    const int StationIndex1 = 128;
    const int StationIndex2 = 240;
    char Vbatstr[4];
    char RSSIstr[3];
    char Datestr[7];
    char Heurestr[5];
    char BDHstr[4];
    char Stationstr[4];
    byte n = 0x00;
    byte n2 = 0x20;


////////////////////////Fin du Template EXIF////////////////////////////////////


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
PMIC pmic;

ParticleFtpClient ftp = ParticleFtpClient();

File root;

// The STARTUP call is placed outside of any other function
// What goes inside is any valid code that can be executed. Here, we use a function call.
// Using a single function is preferable to having several `STARTUP()` calls.
STARTUP(startup());

//STARTUP();

/*
* Définition des champs du fichier de configuration. (création d'un type Struct)
* Pour ajouter ou supprimer, ne pas oublier de modifier également
* La fonction "LoadConfiguration" et la fonction "SaveConfiguration"
*/
struct Config {
  char BDH[8] = "00000";
  char stationName[20] =  "UNDEF";
  char PublicIP[20] = "0.0.0.0";
  int captureMode = 0;  //0:3 img/jour, 1:12 img/jour (7-18), 2:24 img/jour
  int Batt_low_SP = 20;
  char ftp_hostname[50] = "142.213.166.184";
  char ftp_username[20] = "bp1";
  char ftp_password[20] = "BlyuHet7";
  char ftp_dir[50] = "/AutoCamDEH";
};

Config config; //Déclaration de l'objet struct de configuration nommé "config"

/////////////////////////////////////////////////////////////////////////////


/*
* Ceci est exécuté avant même void Setup
* V1.0 2019-11-14
*/
void startup() {
  System.enableFeature(FEATURE_RESET_INFO);
}


void WDevent() {
  log("Watchdog timeout", 1);
  System.reset();
}

/*
* Fonction servant à mettre à jour les variables du programme avec les paramètres spécifiées
* dans le fichier. V1.0 2019-11-14
*/
void loadConfiguration(const char *filename, Config &config) {
  File file = SD.open(filename); //Ouvrir le fichier 
  if (file) {
    StaticJsonDocument<512> doc; //Allocation de mémoire pour la désérialisation JSON des paramètres
    DeserializationError error = deserializeJson(doc, file); // Désérialisation
    if (!error) {
      // Copie des valeurs issues du fichier vers les champs associés du Struct config
      strlcpy(config.BDH,                  // <- destination
        doc["BDH"] | BDH,  // <- source
        sizeof(config.BDH));         // <- destination's capacity
      strlcpy(config.stationName,  doc["stationName"] |  stationName, sizeof(config.stationName));        
      strlcpy(config.PublicIP,     doc["PublicIP"] |     PublicIP,    sizeof(config.PublicIP));
      config.captureMode =         doc["Capture_Mode"] | captureMode; //méthode différente car type Int au lieu de String
      config.Batt_low_SP =         doc["Batt_low_SP"] |  Batt_low_SP;
      strlcpy(config.ftp_hostname, doc["ftp_hostname"] | hostname,    sizeof(config.ftp_hostname));  
      strlcpy(config.ftp_username, doc["ftp_username"] | username,    sizeof(config.ftp_username)); 
      strlcpy(config.ftp_password, doc["ftp_password"] | password,    sizeof(config.ftp_password)); 
      strlcpy(config.ftp_dir,      doc["ftp_dir"] |      ftp_dir,     sizeof(config.ftp_dir));   
    } else {
        Serial.println("Failed to parse config file, using default");
        Particle.publish("Status","Failed to parse config file, using default");
      }
    // On ferme le fichier sans quoi une corruption peut avoir lieu
    file.close();
    
  } else  {
      Serial.println("Failed to open config file on SD, using default");
      Particle.publish("Status","Failed to open config file on SD, using default");
    }
    //Copie des paramètres mis à jour vers leurs variables fonctionnelles
    BDH = config.BDH;
    stationName = config.stationName;
    PublicIP = config.PublicIP;
    captureMode = config.captureMode;
    Batt_low_SP = config.Batt_low_SP;
    hostname = config.ftp_hostname;
    username = config.ftp_username;
    password = config.ftp_password;
    ftp_dir = config.ftp_dir;

}

/*
* Fonction servant à Sauvegarder les variables du programme avec les paramètres spécifiées
* dans le fichier. V1.0 2019-11-14
*/ 
void saveConfiguration(const char *filename, const Config &config) {
  // On supprime le fichier de configuration original
  SD.remove(filename);

  // On crée et ouvre un nouveau fichier de configuration vide avec le même nom
  File file = SD.open(filename, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to create config file");
    Particle.publish("Satus","Failed to create config file");
    return;
  }

  // allocation de mémoire pour la sérialisation JSON
  StaticJsonDocument<512> doc;

  // On utilise les données présentes dans les variables fonctionnelles pour mettre à jour le fichier de config
  doc["BDH"] = BDH;
  doc["stationName"] = stationName;
  doc["PublicIP"] = PublicIP;
  doc["Capture_Mode"] = captureMode;
  doc["Batt_low_SP"] = Batt_low_SP;
  doc["ftp_hostname"] = hostname;
  doc["ftp_username"] = username;
  doc["ftp_password"] = password;
  doc["ftp_dir"] = ftp_dir;

  // Sérialisation
  if (serializeJson(doc, file) == 0) {
    log("Failed to write to file", 2);
  }

  // On ferme le fichier sans quoi les données ne seront pas enregistrées
  file.close();
}

/* 
* Fonction permettant de lire un fichier sur la carte SD et d'envoyer le tout sur le port série
* V1.0 2019-11-14 (non utilisée ACJ)
void printFile(const char *filename) {


  // Ouverture
  File file = SD.open(filename);
  if (!file) {
    Serial.println(F("Failed to read file"));
    return;
  }

  // chaque caractères est prélevé puis envoyé tel quel au port série
  while (file.available()) {
    Serial.print((char)file.read());
  }
  Serial.println();

  // On ferme le fichier
  file.close();
}
*/

/* 
* Fonction Callback étant appelée automatiquement lorsque des informations sont reçues du cloud Particle
* V1.0 2019-11-14 
*/
void handler(const char *topic, const char *data) {
 
    log("FPC " + String(topic) + ": " + String(data), 4);

    //Si le sujet (topic) de l'info a trait Au nom de l'appareil...  
    if(String(topic) == "particle/device/name") {
      BDH = data;
      }
    //Si le sujet (topic) de l'info a trait à l'adresse IP...  
    if(String(topic) == "particle/device/ip") {
      PublicIP = data;
      }
}

/* 
* Fonction qui initialise la Carte SD et allume le Led Bleu si une faute d'initialisation a lieu
* V1.0 2019-11-14 
*/

void initSD() {
  if (!SD.begin(SD_CS)) {
    Serial.println("SD Card initialization failed!! Resetting in 15 seconds");
    digitalWrite(statusLed, HIGH);
    delay(5000);
    digitalWrite(statusLed, LOW);
    delay(15000);
    delay(100);
    System.sleep(SS_Button, FALLING, 300);
    delay(1000);
    System.reset();
  }
}

/* 
* Fonction qui envoie un fichier sur le site ftp spécifié
* V1.1 2019-12-17, Cell reset after tryouts
* V1.2 2020-02-07, Ttype instead of isLog, 0= regular img, 1= log, 2= offline img (temp folder) 
*/
bool syncFTP(String SDfilename, bool retry, String dir, int Ttype) { 
  
  //Déclaration de variable temporaire
  static const size_t bufferSize = 256;// grosseur des blocs de données à envoyer 
  static uint8_t buffer[bufferSize] = {0xFF}; //Terminer le bloc par un 0xFF
  String DestFilename = "";
  char buffersprint1 [50]; // Variables temporaires pour générer les noms de fichiers et 
  char buffersprint2 [50]; // chemins d'arborescence
  uint32_t len = 0;

  // Connection à l'hôte FTP avec les infos fournies
  if (!ftp.open(config.ftp_hostname, port, timeout)) {
    log("Couldn't connect to " + String(config.ftp_hostname), 2);
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

  // On "navigue" vers le chemin de dossier passé en paramètre (dir)
  if (!ftp.cwd(dir)){
    log("could not set directory", 3);
    if(!ftp.mkd(dir)){ //S'il n'existe pas on le crée
      log("ftp mkdir failed: " + dir, 2);
      return 0; // retour de la fonction en erreur si le dossier n'a pu être créé
    }
    log("ftp mkdir success: " + dir, 3);
  }

  // Si le fichier passé en paramètre n'est pas un log (typiquement une image), 
  // on configure les chemins d'arborescence et les noms de fichiers
  if(Ttype == 0) {
    //Nom de l'image à destination
    sprintf(buffersprint1,"%s_%d%02d%s",stationName.c_str(), now.year(),now.month(),SDfilename.c_str());
    DestFilename = buffersprint1;

    //nom et chemin de l'image sur la carte SD 
    sprintf(buffersprint2,"/%s/%d/%d/%s",stationName.c_str(),now.year(), now.month(),SDfilename.c_str());
    SDfilename = buffersprint2;
  } else if(Ttype == 1)   {
      // S'il s'agit d'un log, le nom du fichier à destination ne change pas et l'arborescence 
      // dans la carte SD n'est pas la même (/LOGS/moisAnnée.txt)
      sprintf(buffersprint2,"/%s/%s", "LOGS",SDfilename.c_str());
      DestFilename = SDfilename;
      SDfilename = buffersprint2;
    } else if(Ttype == 2)   {
      //Nom de l'image à destination
      sprintf(buffersprint1,"%s_%d%02d%s",stationName.c_str(), now.year(),now.month(),SDfilename.c_str());
      DestFilename = buffersprint1;

      //nom et chemin de l'image sur la carte SD 
      sprintf(buffersprint2,"/%s/%d/%d/TEMP/%s",stationName.c_str(),now.year(), now.month(),SDfilename.c_str());
      SDfilename = buffersprint2;
    }

 
  //Voir si notre fichier existe déjà sur le serveur
  if (!ftp.list(DestFilename)) {
    log("could not find : " + DestFilename, 4);
  } else {
      ftp.data.flush();
      ftp.finish();
  }

  // Si le dernier transfert à échoué, tenter de supprimer le fichier avorté
  if(retry == true) {
    if(!ftp.dele(DestFilename)){
      log("could not delete file", 2);
    }
    log("Fd", 4);
  }

  //On ouvre le fichier dans la carte SD, si l'opération réussie, initier le transfert
  log("Tfile: " + DestFilename + " SD:" +  SDfilename, 4);
  File SDfile = SD.open(SDfilename);
  delay(100);
  if (SDfile) {
    if(Ttype == 0 || Ttype == 2 ) {
      if (!ftp.type("I")) { //Si image mettre type "I" pour binary transfer
        //log("Couldn't set file type", 2,1);
      } else if (!ftp.stor(DestFilename)) {
        //log("Couldn't STOR file: " + DestFilename, 2,1);
        SDfile.close();
        printNclearSDlogBuffer();
        return 0;
      }
    } else if (Ttype == 1) {
        if (!ftp.type("A")) { //Si log, utiliser le type "A" pour transfert standard
        //log("Couldn't set file type", 2,1);
      } else if (!ftp.stor(DestFilename)) { //Tenter de créer le fichier à destination
        //log("Couldn't STOR file: " + DestFilename, 2,1);
        SDfile.close();
        printNclearSDlogBuffer();
        return 0;
      }  
    }

    //Vérifier la taille du fichier, 524287 bytes est trop gros, 0 indique un fichier vide
    len = SDfile.size();
    if (len >= 0x07ffff) {
        //log("Over size file.", 2,1);
        SDfile.close();
        printNclearSDlogBuffer();
        return 0;
      } else if (len == 0 ){
        SDfile.close();
        //log("File size is 0.", 2,1);
        printNclearSDlogBuffer();
        return 0;
      } 

    //Variables temporaires pour la gestion du transfert
    int bytesRead = 0;
    int TotbytesRead = 0;
    int bytesWritten = 0;
    //int intdelay = 200;
    unsigned long previousMillis = millis();
    unsigned long transferTime = 0;
    int ClientTries = 10;
    //char buf;
    int bufsize = 256;
    uint8_t buf[bufsize];
    long retries = 0;
    int writeerror = 0;

    //Tant que le fichier n'est pas vide
    Particle.disconnect();
    while(SDfile.available() && TotbytesRead <= len) { 
      bytesRead += SDfile.read(buf,bufsize);
      TotbytesRead += bytesRead;
      wd.checkin();
      // Quand l'incrément est rendu au max, on transfère le contenu du buffer vers 
      // le site ftp, On fait scintiller la Led pour indiquer le transfert en cours
        digitalWrite(statusLed, LOW);
        bytesWritten = ftp.data.write(buf,bytesRead,0);
        writeerror = ftp.data.getWriteError();
        if(writeerror != 0) {
          ClientTries = 10;
          while(writeerror == 0 ) {
            Particle.process();
            delay(10 * (10 - ClientTries));
            bytesWritten = ftp.data.write(buf,bytesRead,0);
            writeerror = ftp.data.getWriteError();
            ClientTries -= 1 ;
            retries += 1;
            Serial.println("retry, err: " + String(writeerror) + " Bytes Written: " + String(bytesWritten));
            if(ClientTries == 0){
              //log("Packets tries ran out, resetting cell", 2,1);
              SDfile.close(); // On ferme notre fichier
              ftp.data.flush_buffer();
              //printNclearSDlogBuffer();
              Cellular.off();
              delay(5000);
              Cellular.on();
              while(!Particle.connected())  {   
                delay(500);
                wd.checkin();
              }
              return 0;
            }
          }
        }
        digitalWrite(statusLed, HIGH);
        bytesRead = 0;
        delay(200); //
        // Si on dépasse 600000 bytes, il y a un problème...
        if (TotbytesRead > 200000) {
            return 0;
        }     
      }
        
    SDfile.close(); // On ferme notre fichier
    transferTime = millis() - previousMillis;
    log("TT " + String(transferTime/1000) + " " + String(len/(transferTime/1000)) + " Bps, retries: " + String(retries), 4);
    Particle.process(); //On donne signe de vie au cloud
    //printNclearSDlogBuffer();
  } else {
    // Si nous n'avons pas pu ouvrir le fichier
    log("Could not open File on SD... Aborting", 2);
    return 0;
    }

    //On ferme le transfert, vide le contenu du port ftp et quitte la connexion 
    ftp.data.flush();
  if (!ftp.finish()) {
    log("Couldn't stop file upload", 2);
    return 0;
  }

    log("File size on server : " + String(ftp.size(dir + "/" + DestFilename)), 4);
    log("Local file size : " + String(len), 4);
      
  if (!ftp.quit()) {
    log("Couldn't quit FTP", 2);
    return 0;
  } else {
    digitalWrite(statusLed, LOW);
  }
  Particle.connect();
}
/* 
* Fonction qui récupère le fichier de configuration sur le site ftp et se met à jour
* V1.0 2019-11-14 
*/
bool getConfig(String dir) {
  // Connction au site ftp au les identifiants fournis
  log("up", 4);
  
  if (!ftp.open(hostname, port, timeout)) {
    log("Couldn't connect to ftp host",2);
    return 0;
  }
  if (!ftp.user(username)) {
    log("Bad username", 2);
    return 0;
  }
  if (!ftp.pass(password)) {
    log("Bad password", 2);
    return 0;
  }  

  // On "navigue" vers le chemin de dossier passé en paramètre (dir)
  if (!ftp.cwd(dir)){
    log("could not set directory", 3);
    if(!ftp.mkd(dir)){ // S'il n'existe pas, on le crée
      log("ftp mkdir failed: " + dir, 2);
      return 0;
    }
    log("ftp mkdir success: " + dir, 3);
  }

  // Tentative de récupération du fichier, après un retr réussi, l'info se trouve dans le buffer TCP
  if (!ftp.retr("/AutoCamDEH/config/" + BDH + "/CONFIG.JSN")) {
    log("Couldn't retrieve CONFIG.JSN", 2);
    return 0;
  } else { //l'opération a réussi, nous pouvons supprimer le fichier de configuration pour le mettre à jour
    if(!SD.remove("CONFIG.JSN")) { //S'il existe...
      Serial.println("file config.jsn doesn't exist");
    }

    //On crée un nouveau fichier de configuration et on vide le contenu du buffer TCP dans ce fichier.
    File file = SD.open("CONFIG.JSN",  FILE_WRITE); 
    if(file) {
      while (ftp.data.connected()) {
        while (ftp.data.available()) {
        file.write(ftp.data.read());
        }
      }
      } else {
        log("Failed to create new config",2);
    }
    // Fin du transfert, on ferme la connexion.
    file.close();
    delay(10);
    ftp.finish();
    ftp.quit();
    delay(10);

    log("up'ed", 4);

    // On met à jour la configuration de l'appareil avec les nouvelles données dans le fichier
    loadConfiguration("CONFIG.JSN", config);

  }
}

/* 
* Fonction qui prends une photo et la place sur la carte SD
* 
* /!\ LE NOM NE PEUT FAIRE PLUS QUE 8 CARACTÈRES!! /!\
*
* V1.1 2019-12-18 : EXIF injection from template
* V1.2 2020-02-07 : Temp folder for offline takes
*/
int grabPic(String Short_filename) {
  uint8_t vid,pid;
  uint8_t temp;
  uint8_t temp_last;
  
  //Vérification de la comm avec la caméra, le while(1) (provient de l'exemple de la librairie
  // à modifier éventuellement 
  while(1) {

    // initialisation du SPI1, la carte SD est sur le SPI de base et Arducam est sur SPI1 (conflit de CS)
    // Le SPI pour la carte SD est initialisé dans SD.begin()
    //SPI1.begin(SPI_CS);
    SPI1.begin(SPI_MODE_MASTER);
    //SPI.begin(SPI_MODE_SLAVE, SPI_CS);  
    // La caméra est alimentée par un "latching relay", attention, elle consomme beaucoup.

    digitalWrite(Cam_on, HIGH);
    //delay(2000);
    delay(100);
    myCAM.CS_LOW();
    //On vérifie si la caméra répond sur le bus SPI1
    myCAM.write_reg(ARDUCHIP_TEST1, 0x55);
    temp = myCAM.read_reg(ARDUCHIP_TEST1); //ce registre est dédié pour ça
    if(temp != 0x55)
    {
      log("SPI interface Error! resetting in 15 seconds, myCam.read_reg said " + String(temp), 1);
      Particle.publish("status", "SPI Error");
      delay(15000);
      System.sleep(SS_Button, FALLING, 1500);
      delay(1000);
      System.reset();
    }
    else {
      break; //on sort du while(1) si il n'y a pas de problème.
    }
    Particle.process(); //On donne signe de vie au cloud

  }

  // Vérifier le modèle de la caméra. Encore un while(1)...
  while(1){
    Particle.process();
    //Check if the camera module type is OV5642
    // myCAM.rdSensorReg8_8(OV2640_CHIPID_HIGH, &vid);
    // myCAM.rdSensorReg8_8(OV2640_CHIPID_LOW, &pid);

    myCAM.wrSensorReg8_8(0xff, 0x01);
    myCAM.rdSensorReg8_8(OV2640_CHIPID_HIGH, &vid);
    myCAM.rdSensorReg8_8(OV2640_CHIPID_LOW, &pid);

    //Ce registre devrait indiquer 2640
    if ((vid != 0x26 ) && ( pid != 0x41 ) || ( pid != 0x42 )){
      log("Can't find OV2640 module! camera says " + String::format("%d:%d", vid, pid), 1);
      delay(5000);
      System.sleep(SS_Button, FALLING, 1500);
    }
    else { 
      break;
    }
  }

  //myCAM.write_reg(ARDUCHIP_MODE, 0x01);		 	//Viens de la librairie, usage inconnu
  // On utilise la compression JPEG
  myCAM.set_format(JPEG);
  delay(10);
  // On initalise la caméra
  myCAM.InitCAM();
  delay(10);
  //myCAM.set_bit(ARDUCHIP_TIM, VSYNC_LEVEL_MASK);
  //myCAM.write_reg(ARDUCHIP_TIM, VSYNC_LEVEL_MASK);
  //delay(100);
  //myCAM.write_reg(ARDUCHIP_FRAMES,0x00);
   //Usage inconnu
  /*  while(1) {
   myCAM.read_reg(0x00);
   }*/
  // On sélectionne ici la résolution, à mettre à jour
  //myCAM.OV5642_set_JPEG_size(OV5642_320x240);   //works
  //myCAM.OV5642_set_JPEG_size(OV5642_640x480);   // works
  //myCAM.OV5642_set_JPEG_size(OV5642_1600x1200); // works
  //myCAM.OV5642_set_JPEG_size(OV5642_1280x960);  // works
  //myCAM.OV2640_set_JPEG_size(OV2640_1600x1200); //works
  myCAM.OV2640_set_JPEG_size(OV2640_1280x1024);
  delay(10);
  myCAM.set_Special_effects(Normal);
  delay(10);
  myCAM.set_Light_Mode(Auto);
  delay(10);

  // On flush toute info associée à une photo antérieure avant de prendre une photo
  myCAM.flush_fifo();
  delay(10);
  myCAM.clear_fifo_flag();
  delay(5000);
  myCAM.start_capture();
  delay(10);

  unsigned long start_time = millis(),
  last_publish = millis();

  // On attends que la photo soit prise
  while(!myCAM.get_bit(ARDUCHIP_TRIG , CAP_DONE_MASK)) {
      Particle.process(); // on oublie pas le cloud
      delay(10);

      unsigned long now = millis();
      if ((now - last_publish) > 1000) {
          last_publish = now;
      }
      // SI jamais c'est trop long, il y a un problème
      if ((now-start_time) > 30000) {
          log("Photo timeout", 1);
          break;
      }
  }
  delay(10);
  // Lecture de la taille de l'image, inscription dans le log
  int length = myCAM.read_fifo_length();
  log("Is " + String(length), 4);

  //Variables temporaires
  temp = 0xff, temp_last = 0;
  int bytesRead = 0;
  String Pic = "";

  // Si l'image est prête, on la prélève 
  if(myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK))
  {
    //d'autres variables temporaires
    byte buf[256];
    //uint8_t temp, temp_last;
    File file;
    String dir;
    //myCAM.CS_HIGH(); //On s'assure de taire la caméra sur le Bus SPI
    delay(10);
     
    dir = String(config.stationName) + "/";
    dir += String(now.year()) + "/";
    dir += String(now.month()) + "/";     
    if(offlineMode) {
      dir += "TEMP";
    } 
    // On crée le dossier s'il n'existe pas
    Serial.println("dir:" + dir);
    if(!SD.exists(dir)){
       //log("Directory" + dir + "does not exist, trying to create it...", 3,1);
      if(!SD.mkdir(dir)){
       //log("SD mkdir failed", 2,1);
       return false;
      } 
    } 

    // On génère le nom de l'arborescence + le nom du fichier pour le créer au bon endroit
    String namePlusDir = "";
    namePlusDir = dir + "/" + Short_filename;
    file = SD.open(namePlusDir, O_WRITE | O_CREAT | O_TRUNC);
    delay(10);

    // Si on a pas réussi à le créer, on détruit l'image dans le buffer de la caméra et on retourne en erreur
    if(!file){
      //log("open file failed: " + namePlusDir, 1,1);
      file.close();
      //printNclearSDlogBuffer();
      myCAM.flush_fifo();
      delay(100);
      myCAM.clear_fifo_flag();
      delay(5000);
      return false;
    }

    // Le fichier d'image doit commencer par 0xFF, étrangement ce n'est pas dans le buffer de la caméra
    //file.write(0xFF);

    CellularSignal sig = Cellular.RSSI();

    //Serial.println("Creating EXIF header");
    sprintf(Vbatstr, "%1.2f", fuel.getVCell());
    //Serial.write(Vbatstr);
    sprintf(RSSIstr, "%2d", sig.rssi);
    //Serial.write(RSSIstr);
    sprintf(Datestr, "%04d%02d%02d", now.year(),now.month(),now.day());
   // Serial.write(Datestr);
    sprintf(Heurestr, "%02d%02d%02d", now.hour(),now.minute(),now.second());
    //Serial.write(Heurestr);
    //Serial.write(BDH);  
    //Serial.write(stationName);

    //file.write(buffer, tx_buffer_index);
    tx_buffer_index = 0;
    Particle.process();     

    file.write(exif1,86);
    for (size_t i = 0; i <= 2054; i++) {
        file.write(n);
      }
    file.write(exif2,23);

    int i = 0;
   for (indexer = Vbatindex1; indexer <= 6+Vbatindex1 ; indexer++) {
        exifInfo3[indexer] = Vbatstr[i];
        indexer++;
        exifInfo3[indexer] = 0x00;
        i++;
      }
    i = 0;
    for (indexer = Vbatindex2; indexer <= 3+Vbatindex2 ; indexer++) {
        exifInfo5[indexer] = Vbatstr[i];
        exifInfo7[indexer] = Vbatstr[i];
        i++;
      }
    
    i = 0;
    for (indexer = RSSIindex1; indexer <= 4+RSSIindex1 ; indexer++) {
        exifInfo3[indexer] = RSSIstr[i];
        indexer++;
        exifInfo3[indexer] = 0x00;
        i++;
      }
    i = 0;
    for (indexer = RSSIindex2; indexer <= 2+RSSIindex2 ; indexer++) {
        exifInfo5[indexer] = RSSIstr[i];
        exifInfo7[indexer] = RSSIstr[i];
        i++;
      }      

    i = 0;
    for (indexer = DateIndex1; indexer <= 14+DateIndex1 ; indexer++) {
        exifInfo3[indexer] = Datestr[i];
        indexer++;
        exifInfo3[indexer] = 0x00;
        i++;
      }   
    i = 0;
    for (indexer = DateIndex2; indexer <= 7+DateIndex2 ; indexer++) {
        exifInfo5[indexer] = Datestr[i];
        exifInfo7[indexer] = Datestr[i];
        i++;
      }         

    i = 0;
    for (indexer = HeureIndex1; indexer <= 10+HeureIndex1 ; indexer++) {
        exifInfo3[indexer] = Heurestr[i];
        indexer++;
        exifInfo3[indexer] = 0x00;
        i++;
      }
    i = 0;
    for (indexer = HeureIndex2; indexer <= 5+HeureIndex2 ; indexer++) {
        exifInfo5[indexer] = Heurestr[i];
        exifInfo7[indexer] = Heurestr[i];
        i++;
      }      

    i = 0;
    for (indexer = BDHIndex1; indexer <= 10+BDHIndex1 ; indexer++) {
        exifInfo3[indexer] = BDH.c_str()[i];
        indexer++;
        exifInfo3[indexer] = 0x00;
        i++;
      } 
    i = 0;
     for (indexer = BDHIndex2; indexer <= 5+BDHIndex2 ; indexer++) {
        exifInfo5[indexer] = BDH.c_str()[i];
        exifInfo7[indexer] = BDH.c_str()[i];
        i++;
      }      

    i = 0;     
   for (indexer = StationIndex1; indexer <= 8+StationIndex1 ; indexer++) {
        exifInfo3[indexer] = Stationstr[i];
        indexer++;
        exifInfo3[indexer] = 0x00;
        i++;
      } 
    i = 0;
    for (indexer = StationIndex2; indexer <= 5+StationIndex2 ; indexer++) {
        exifInfo5[indexer] = stationName.c_str()[i];
        exifInfo7[indexer] = stationName.c_str()[i];
        i++;
      }
    indexer = 0;

    for (size_t j = 0; j <= 2055; j++) {
      file.write(n);
    }   
    file.write(exifInfo3,138);
    file.write(exif4,637);                              
    file.write(exifInfo5,245);
    file.write(exif6,288);
    file.write(exifInfo7,245);
    file.write(exif8,98);
    for (size_t i = 0; i <= 2047; i++) {
      file.write(n2);
    }          
    file.write(exif9,20);
    
    Serial.println(" Exif injection done");    

    //On s'adresse maintenant à la caméra
    myCAM.CS_LOW();
    delay(10);
    //log("Capture Done.", 4);
    Particle.publish("status", "Capture done");
    // On veut lire l'image dans le buffer bit par bit
    myCAM.set_fifo_burst();
    delay(10);

    tx_buffer_index = 0;
//    temp = 0;
      for (size_t i = 0; i < 20; i++)
    {
      temp = myCAM.read_fifo();
    }   

    //delay(10);
    // Tant que les deux derniers octets ne sont pas 0xFF et 0xD9, nous ne sommes pas à la fin du fichier
    // 0xFF et 0xD9 sont les marqueurs standards de la fin d'un JPG
    while( (temp != 0xD9) || (temp_last != 0xFF) )
    //while(bytesRead< length-20)
    {
      
      temp_last = temp;
      temp = myCAM.read_fifo(); // Le fifo est la stack de mémoire du buffer de la caméra, first in first out
      bytesRead++;
      buffer[tx_buffer_index++] = temp; // on écrit octets par octets

      // Si on a rempli le buffer, on le vide dans le fichier de la carte SD
      if (tx_buffer_index >= TX_BUFFER_MAX) {
          file.write(buffer, tx_buffer_index);
          tx_buffer_index = 0;
          Particle.process();
      }
      // Après un certain nombre d'octets, il y a un problème
      if (bytesRead > 600000) {
          // failsafe
          file.close();
          return 0;
      }
    }

    // Il reste des octets dans le buffer, on vide le tout dans le fichier
    if (tx_buffer_index != 0) {
        file.write(buffer, tx_buffer_index);
    }          
    // On a finit avec le fifo, on le libère
    myCAM.clear_fifo_flag();
    myCAM.CS_HIGH();

    // On ferme le fichier 
    file.close();

    //printNclearSDlogBuffer();

    //On éteint la caméra pour économiser le plus d'énergie possible
    digitalWrite(Cam_on, LOW);

    return 1;
  }
}

/* 
* Fonctions inscrivant les entrées dans le log
* V1.1 2019-12-05
*/
void log(String msg, int Loglevel) {
  // Tout ce qui est envoyé au log est également envoyé au port série
  Serial.println(msg);

  // Dans le dossier logs/
  String dir ="LOGS/" ;
  char buffer[20];
  // On prend l'heure actuelle
  DateTime logtime = rtc.now();
  String logFileName = dir + String(monthOfTheYear[logtime.month()-1]) + String(logtime.year()) + ".TXT";
  File file;

  // on crée le répertoire s'il n'existe pas
  file = SD.open(dir);
  if(file) {
    if(!file.isDirectory()) {
      Serial.println("Error in log dir...");
      file.close();
      SD.remove(dir);
      if(!SD.mkdir(dir)) {
        Serial.println("SD mkdir failed");
      }         
    } else {
        file.close();
      }
  } else {
      Serial.println("No Log dir.");
      if(!SD.mkdir(dir)) {
        Serial.println("SD mkdir failed");
      }
    }
  file = SD.open(logFileName, FILE_WRITE);
  if(!file) {
    Serial.println("could not open log file: " + logFileName);
    return;
  } else {

    /* 4 niveaux de log
    * Log levels:
    * 1: Critical error, erreur critique menant nécessairement au redémarrage de l'appareil
    * 2: error, erreur affectant le bon fonctionnement de l'appareil
    * 3: warning, situation anormale pouvant aider le dépannage
    * 4: info, information générale pouvant aider le dépannage
    */

    switch (Loglevel)
    {
      case 1:
        file.print("CRITICAL ERROR    ;");
        break;

      case 2:
        file.print("ERROR    ;");
        break;

      case 3:
        file.print("WARNING    ;");
        break;

      case 4:
        file.print("INFO    ;");
        break;

      default:
        break;
    }
    
    //On imprime dans le log sous le format suivant
    //"LOGLEVEL"     ;AAAAMMDD;HHMMSS; "MSG"
    sprintf(buffer,"%04d%02d%02d;%02d%02d%02d;", logtime.year(),logtime.month(), logtime.day(), logtime.hour(), logtime.minute(), logtime.second());
    file.print(buffer);
    file.println(msg);
    
    file.close();
  }
}

/*void log(String msg, int Loglevel, bool toBuf) {
  // Tout ce qui est envoyé au log est également envoyé au port série
  if (logBufIndex >= 49)
  Serial.println("Log buffer full");

  Serial.println(msg);

  char buffer[20];
  // On prend l'heure actuelle
  DateTime logtime = rtc.now();

   4 niveaux de log
  * Log levels:
  * 1: Critical error, erreur critique menant nécessairement au redémarrage de l'appareil
  * 2: error, erreur affectant le bon fonctionnement de l'appareil
  * 3: warning, situation anormale pouvant aider le dépannage
  * 4: info, information générale pouvant aider le dépannage
  

  switch (Loglevel)
  {
  case 1:
    logbuffer[logBufIndex] = "CRITICAL ERROR    ;";
    break;

  case 2:
    logbuffer[logBufIndex] = "ERROR    ;";
    break;

  case 3:
    logbuffer[logBufIndex] = "WARNING    ;";
    break;

  case 4:
    logbuffer[logBufIndex] = "INFO    ;";
    break;

  default:
    break;
  }
  
  //On imprime dans le log sous le format suivant
  //"LOGLEVEL"     ;AAAAMMDD;HHMMSS; "MSG"
  sprintf(buffer,"%04d%02d%02d;%02d%02d%02d;", logtime.year(),logtime.month(), logtime.day(), logtime.hour(), logtime.minute(), logtime.second());
  logbuffer[logBufIndex] += buffer;
  logbuffer[logBufIndex] += msg;
  logBufIndex++;
}*/

void printNclearSDlogBuffer() {
  if(logBufIndex > 0) {
    String dir ="LOGS/" ;
    File file;
    DateTime logtime = rtc.now();
    String logFileName = "/" + dir + String(monthOfTheYear[logtime.month()-1]) + String(logtime.year()) + ".TXT";  

    // on crée le répertoire s'il n'existe pas
    if(!SD.exists(dir)){
        Serial.println("Directory does not exist, trying to create it...PNSDLB");
      if(!SD.mkdir(dir)){
        Serial.println("SD mkdir failed PNSDLB");
      } 
    }
    file = SD.open(logFileName,  FILE_WRITE);
    if(!file) {
      Serial.println("could not open log file");
      Serial.println(logFileName);
      return;
    } else {
      while (logBufIndex > 0)
      {
        file.println(logbuffer[LogEntries]);
        logbuffer[LogEntries] = "";
        logBufIndex--;
      }
      file.close();
    }
  }
}

/* 
* Fonction concierge pour éviter le trop plein de données
*
*/
int cleanSD(String Dummy) {
  int FolderRmvd = 0;
  int FileRmvd = 0;
  Particle.publish("status", "Cleaning old photos on SD...");
  log("clSD", 4);
  File root = SD.open(stationName + "/");
  File entry = root.openNextFile();
  int nowyear = now.year();

  while (entry) {
    String entryname = entry.name();
    Serial.println(String(entryname.toInt()));
    if (entry.isDirectory()) {
      if(entryname.toInt() != nowyear && entryname.toInt() != nowyear-1){
          File entry1 = entry.openNextFile();
          Serial.println(entry1.name());
          while(entry1){
            if(entry1.isDirectory()){
              File entry2 = entry1.openNextFile();
              Serial.println(entry2.name());
              while(entry2) {
                if(!SD.remove(stationName + "/" + entry.name() + "/" + entry1.name() + "/" + entry2.name())){
                    //Serial.println("removal of " + stationName + "/" + entry.name() + "/" + entry1.name() + "/" + entry2.name() + " failed");
                  } else {
                    //Serial.println("removal of " + stationName + "/" + entry.name() + "/" + entry1.name() + "/" + entry2.name() + " Succeed");
                    FileRmvd++;
                    }
                entry2.close();                  
                entry2 = entry1.openNextFile();
                //Serial.println(entry2.name());
              }
              SD.rmdir(stationName + "/" + entry.name() + "/" + entry1.name());
              FolderRmvd++;
            }
            entry1.close();
            entry1 = entry.openNextFile();
            //Serial.println(entry1.name());
          }
          SD.rmdir(stationName + "/" + entry.name());
          FolderRmvd++;
        }
    }
    entry.close();
    entry =  root.openNextFile();
  }
  root.close();
  Particle.publish("status", "Rd " + String(FolderRmvd) + ", Rf" + String(FileRmvd));
  log("Rd " + String(FolderRmvd) + ", Rf" + String(FileRmvd), 4);



  // retirer les logs qui datent de plus d'un an
  
  FolderRmvd = 0;
  FileRmvd = 0;
  Particle.publish("status", "Cleaning old logs on SD...");
  log("clL", 4);
  root = SD.open("LOGS/");
  entry = root.openNextFile();
  while (entry) {
    String entryname = entry.name();
    if(entryname.endsWith(String(nowyear-2))){
        if(!SD.remove("LOGS/" + String(entry.name()))){
          Serial.println("failed");
        } else {
          FileRmvd++;
          }
        }
    entry.close();
    entry =  root.openNextFile();
  }
  root.close();
  Particle.publish("status", "Rd " + String(FolderRmvd) + ", Rf" + String(FileRmvd));  
  log("Rd " + String(FolderRmvd) + ", Rf" + String(FileRmvd), 4);
  return 1;
}

/* 
* Fonction d'initialisation qui s'exécute une seule fois au démarrage de l'appareil
* V1.1 2019-12-17 
*/
void setup() { 
  //On configure les variables, fonctions et souscriptions disponibles 
  // dans le cloud ainsi que d'autres paramètres
  //Particle.keepAlive(120);
  Cellular.on();
  fuel.wakeup();   
  fuel.begin();
  pmic.enableBuck();
  Particle.process();
  Particle.function("Grab Pic", grabPic);
  Particle.function("CleanSD", cleanSD);
  Particle.subscribe("particle/device/ip", handler, MY_DEVICES);
  Particle.subscribe("particle/device/name", handler);  
  Particle.publishVitals();
  Particle.publishVitals(300);
  Time.zone(-5);
  // On initalise les GPIO 
  pinMode(Cam_on, OUTPUT);
  pinMode(statusLed, OUTPUT);
  pinMode(D8, INPUT);
  pinMode(D7, OUTPUT);
  pinMode(SS_Button, INPUT);
  //pinMode(D5, OUTPUT);
  //pinMode(SD_CS, OUTPUT);  

  //Initialisation I2C (RTC et Arducam)
  Wire.setSpeed(CLOCK_SPEED_100KHZ);
  Wire.begin();
  
  initSD();

  // Est-ce qu'on sort d'un sleep mode? si oui on reset le système
  // System.sleep(); permet de garder en mémoire les variables, 
  // toutefois le système à été conçu pour être en "deep_Sleep" ce qui désalimente la RAM
  // Pour une meilleure économie, mais nécessite un signal HIGH sur D8 pour redémarrer 
  // le controleur. Le system.reset est donc un patch avant de trouver une meilleure solution
  /*
  SleepResult result = System.sleepResult();
  if(result.wokenUpByRtc()) {
    log("Wu", 4);
    delay(1000);
    System.reset();
  }
  if(result.wokenUpByPin() == D8) {
    log("Wu RTC", 4);
    delay(1000);
    System.reset();
  }
*/
  if((fuel.getVCell()*100) < Batt_low_SP) {
    log("Battery Low, waiting for recharge: " + String(fuel.getVCell()) + " V", 1);
    digitalWrite(Cam_on, LOW);
    delay(1000);
    goToSleep(3600);
    }  

  log("Reset Code: " + String(System.resetReason()),4);

  //Initialisation de divers périphériques
  rtc.begin();
  Serial.begin(115200);
  rtc.clear_rtc_interrupt_flags();
  log("Init " + String(VERSION_SLUG), 4);
  
  // Si le rtc n'est pas en fonction, utiliser la date et l'heure 
  // de compilation du programme pour le démarrer puis redémarer pour vérifier à nouveau
  if (!rtc.isrunning()) {
    log("RTC is NOT running!", 2);
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
    delay(5000);
    System.reset();
  } else { //Sinon paramétrer le RTC, ceci était utilisé pour l'alarme. non fonctionnel ACJ
      rtc.config();
      rtc.clear_rtc_interrupt_flags();
    }

  // On met à jour nos variables de configuration
  loadConfiguration(configFileName,config);
}

/* 
* Le loop se répète en boucle automatiquement
* V1.1 2019-12-17
*/
void loop() {
  // Variables temporaires propre à cette boucle
  loop:
  //Cellular.on();
  //Particle.connect();
  int secstoTimeout = timeout;
  int SSButton_longpress = 0;
  String Nametocard;
  uint8_t buf[4];
  Particle.process(); // on oublie pas le cloud
  SleepResult result = System.sleepResult();

  if(result.wokenUpByRtc()) {
    log("Wu", 4);
    delay(1000);
    System.reset();
  }    

  if(result.wokenUpByPin()) {
  secstoTimeout = timeout;
  RGB.mirrorTo( NULL, statusLed, NULL);
  Cellular.connect();
  log("Wu SS_Button", 4);

  while(!Cellular.ready()) {
    secstoTimeout--;
    delay(1000);
    if(secstoTimeout <= 140) {
      RGB.mirrorDisable();
      pinMode(statusLed, OUTPUT);
      delay(100);
      log("could not connect", 4);
      digitalWrite(statusLed, HIGH);
      delay(5000);
      digitalWrite(statusLed, LOW);
      delay(1000);
      System.sleep(SS_Button, FALLING, 1500);
      delay(1000);
      goto loop;
    }
  }

  standby:
  secstoTimeout = timeout * 300;
  SSButton_longpress = 0;
  
  while(SSButton_longpress <= 2000 && secstoTimeout > 0) {
    SSButton_longpress = 0;
    if(digitalRead(SS_Button) == LOW) {
      while (digitalRead(SS_Button) == LOW) {
        SSButton_longpress += 100;
        delay(100);
      }
      
      Particle.publish("status", "SSButton_longpress >= 10000, " + String(secstoTimeout));
      CellularSignal sig = Cellular.RSSI();
      log("Signal Query: " + String(sig.rssi) + "dB" , 4);
      RGB.mirrorDisable();
      pinMode(statusLed, OUTPUT);
      delay(100);     
      for (size_t i = 0; i <= -(sig.rssi/10) ; i++) {
        digitalWrite(statusLed, LOW);
        delay(500);
        digitalWrite(statusLed, HIGH);
        delay(500);
      }
      secstoTimeout = timeout;
    }

    RGB.mirrorTo(NULL, statusLed, NULL);
    Particle.process();
    delay(100);
    secstoTimeout -= 1;
    wd.checkin();
  }

  if(SSButton_longpress >= 10000) {
    RGB.mirrorDisable();
    pinMode(statusLed, OUTPUT);
    Particle.connect();
    secstoTimeout = timeout;
    while(!Particle.connected()) {
      digitalWrite(statusLed, LOW);
      delay(250);
      digitalWrite(statusLed, HIGH);
      delay(250);
      if(secstoTimeout <= 140) {
        RGB.mirrorDisable();
        pinMode(statusLed, OUTPUT);
        delay(100);
        log("could not connect to cloud", 4);
        digitalWrite(statusLed, HIGH);
        delay(5000);
        digitalWrite(statusLed, LOW);
        delay(1000);
        System.sleep(SS_Button, FALLING, 1500);
        delay(1000);
        goto loop;
      }  
      secstoTimeout--;  
      wd.checkin();  
    }
    Particle.publishVitals();
    RGB.mirrorTo( statusLed, statusLed, statusLed);
    goto loop;
  }
  
  RGB.mirrorDisable();
  pinMode(statusLed, OUTPUT);
  digitalWrite(statusLed,LOW);
  delay(10);
  goToSleep(600);
  delay(1000);
  goto loop;
  
}  
  
  // Avant de procéder aux opération nécessitant du réseau, on attends d'être connecté
  log("WCC", 4); 
  unsigned long previousMillis = millis();
  unsigned long thatTook = 0;
  while(!Cellular.ready()){
      delay(1000);
      Particle.process();
      secstoTimeout--;
      if (secstoTimeout <= 0) { // Si c'est trop long, on passe en mode offline
        offlineMode = true;
        log("Timeout, OM", 3);
        break;
      }
    }
  secstoTimeout = timeout;

  previousMillis = millis();
  thatTook = 0;
  while(!Particle.connected() && !offlineMode){
    delay(1000);
    Particle.process();
    secstoTimeout--;
    if (secstoTimeout <= 170) { // Si c'est trop long, on passe en mode cloud outage
      cloudOutage = true;
      Particle.disconnect();
      log("Timeout, CO", 3);
      break;
    }
  }
  
  secstoTimeout = timeout;
  // Décompte du temps de connection en secondes
  thatTook =  (millis() - previousMillis)/1000;
  log("TT " + String(thatTook), 4);

  // On obtient le niveau de batterie et de signal cell pour le log
  String SOC = String(fuel.getSoC());
  String VCell = String(fuel.getVCell());
  CellularSignal sig = Cellular.RSSI();
  log("SOC; " + SOC + ";Vb;" + VCell + ";RSSI;" + String(sig.rssi) + ";Qual; " + String(sig.getQuality()) + ";", 4);  
  if (sig.getQuality() < 25) offlineMode = true ; // Si la qualité du signal est sous 25, on considère que nous sommes hors ligne

  // Opérations à faire seulement si nous avons une connection réseau confirmée
  if(!offlineMode) {
    if(!cloudOutage) {
      Particle.publish("particle/device/ip", PRIVATE);
      Particle.publish("particle/device/name");
      Particle.publish("Batt_SOC",SOC);
      Particle.publish("RSSI",String(sig.rssi));

      // On force un synchronisation du temps de l'horloge interne avec le cloud
      Particle.syncTime();
      while(!Particle.syncTimeDone() && secstoTimeout >= 170) {
        delay(500);
        secstoTimeout--;
      }

      // Si on a reçu confirmation avant la fin du timeout, on synchronise l'horloge interne avec la RTC
      if(secstoTimeout >= 170 && Time.isValid()) {
        rtc.adjust(DateTime(Time.year(), Time.month(), Time.day(), Time.hour(), Time.minute(), Time.second()));
        Particle.syncTimeDone();
        Particle.publish("Status","Time Synced!");
        log("TSync", 4);
      } else {
        log("Time Sync failed!", 2);
      }
      secstoTimeout = timeout;

      // On attends de recevoir les infos du cloud
      while(stationName == "undef" && secstoTimeout > 0){
        delay(1000);
        secstoTimeout--;
      }
      secstoTimeout = timeout;
    }

    getConfig("/AutoCamDEH/config/" + BDH + "/");

    // On sauvegarde la nouvelle configuration
    saveConfiguration(configFileName,config);
  }

  // On prélève l'heure actuelle et on l'affiche sur le port série
  now = rtc.now();
  Serial.print(String(now.year()));
  Serial.print('/');
  Serial.print(String(now.month()));
  Serial.print('/');
  Serial.print(String(now.day()));
  Serial.print(' ');
  Serial.print(String(now.hour()));
  Serial.print(':');
  Serial.print(String(now.minute()));
  Serial.print(':');
  Serial.print(String(now.second()));
  Serial.println();
  char buffer[40];

  if (now.day() == 1 && now.hour() == 15) {
    cleanSD("");
  }

  // On génère le nopm du fichier à partir de l'heure actuelle, on force 2 caractères par entrée
  // exemple: 02 au lieu de 2
  sprintf(buffer,"%02d%02d%02d%02d.jpg", now.day(),now.hour(), now.minute(), now.second());
  Nametocard = buffer;

  // On déclanche la prise de photo avec le nom généré précédemment
  if(!grabPic(Nametocard)){
    log("GrabPic failed, name of file: " + Nametocard, 1);
    delay(5000);
    goToSleep(300);
    goto loop;
  }   

  if((fuel.getVCell()*100) < Batt_low_SP) {
    log("Battery Low, waiting for recharge: " + String(fuel.getVCell()) + " V", 1);
    digitalWrite(Cam_on, LOW);
    delay(1000);
    goToSleep(3600);
    delay(1000);
    goto loop;
  }

  // On déclenche la synchronisation FTP de l'image (à désactiver en cas de mode offline) 
  // 3 essais avant d'abandonner, 5 secondes entre les essais
  if(!offlineMode){
    int attempts = 0;
    if(!syncFTP(Nametocard, false,ftp_dir, 0)){
        log("Isync fail", 2);
        while(!Cellular.ready()){
          delay(1000);
        }
        while(!syncFTP(Nametocard, true,ftp_dir, 0) && attempts < 1){
          log("Isync fail again", 2);
          Particle.process();
          attempts++;
          while(Cellular.connecting()) {
            delay(500);
          }
        }
      } else {
        log("ISuc", 4);
      }   

    attempts = 0;
    // On déclanche la synchronisation FTP du Log (à désactiver en cas de mode offline) 3 essais...
    if(!syncFTP(String(monthOfTheYear[now.month()-1]) + String(now.year()) + ".txt", true,"/AutoCamDEH/config/" + BDH, 1)){
        log("Lsync fail", 2);
        delay(5000);
        while(!Cellular.ready());
        while(!syncFTP(String(monthOfTheYear[now.month()-1]) + String(now.year()) + ".txt", true,"/AutoCamDEH/config/" + BDH, 1) && attempts < 1){
          log("Lsync fail again", 2);
          attempts++;
          while(!Cellular.ready());
        }
      } else {
        log("LSuc", 4);
        attempts = 0;
      }   
    sprintf(buffer,"%s/%d/%d/temp/",stationName.c_str(),now.year(), now.month());
    Serial.println("looking for: " + String(buffer));
    if(SD.exists(buffer)) {
      Serial.println("folder temp exists");
      root = SD.open(buffer);
      File BuPic = root.openNextFile();
      while(BuPic && !BuPic.isDirectory()){
        Nametocard = BuPic.name() ;
        Serial.println("Name of offline pic: " + Nametocard);
        BuPic.close();
        while(!syncFTP(Nametocard,false,ftp_dir,2) && attempts < 1) {
          attempts++;
          while(!Cellular.ready());
        }
        SD.remove(buffer + Nametocard);
        BuPic = root.openNextFile();
      }
      root.close();
      Serial.println("Backup Pic dump Done");
      SD.rmdir(buffer);
    } else {

      Serial.println("folder temp doesn't exists");
    }
    Particle.publish("status", "Sleeping");
  }   

  
  // prélèvement de l'heure actuelle pour calculer le nombre de secondes pour "dormir"
  now = rtc.now();
  rtc.clear_rtc_interrupt_flags();
  int secsToWakeup = 0;


  // 3 modes d'acquisitions
  
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

  case 1: //12 img/jour
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
  case 2: //24 img/jour
        DateTime wakeupTime(DateTime(now.year(),now.month(),now.day(),now.hour(),0,0) + TimeSpan(0,1,0,0));
        secsToWakeup = wakeupTime.secondstime() - now.secondstime();    
    break;   

  }
  /* // Envoi de la configuration d'alarme sur la RTC... non fonctionelle ACJ
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
 

  log("S " + String(secsToWakeup), 4);
  // On dort pour le nombre prédéterminé de secondes. 
  // On peut aussi le repartir avec un front descendant sur la pin D8, 
  // possible ajout d'un bouton de prise d'Image?
  goToSleep(secsToWakeup);
  //goToSleep(30);
  //System.sleep(SLEEP_MODE_DEEP);
  //System.reset();
}

/* 
* GoToSleep, séquence qui minimise la consommation de courant entre les photos
* V1.2 2020-03-18, ajout de BLE.off()
*/
void goToSleep(long TbeforeWake) {
  digitalWrite(statusLed, LOW);
  fuel.sleep();
  SD.end();
  BLE.off();
  Particle.disconnect();
  softDelay(5000);
  Cellular.off();
  pmic.disableBuck();
  softDelay(5000); // Step through the process safely to ensure the lowest Modem Power.
  System.sleep(SS_Button, FALLING, TbeforeWake);
  softDelay(1000);
}

inline void softDelay(uint32_t t) {
  for (uint32_t ms = millis(); millis() - ms < t; Particle.process());  //  safer than a delay()
}