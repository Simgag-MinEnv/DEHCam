//./build-local.sh tcp_photographer
//particle flash --usb target/1.4.2/boron/OVFork.bin
SYSTEM_MODE(AUTOMATIC);
SYSTEM_THREAD(ENABLED);

#include "ArduCAM.h"
#include "memorysaver.h"
#include "ParticleFTPClient.h"
#include "SD.h"
#include "PCF8523.h"
#include "ArduinoJson.h"
#include "ov2640_regs.h"

const int statusLed = D7; 
const int SPI_CS = A5;
const int SD_CS = D5;
const int Set_rly = A0;
const int Reset_rly = D6;
FuelGauge fuel;

ApplicationWatchdog wd(90000, WDevent);

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

#define VERSION_SLUG "V0.3"

#define TX_BUFFER_MAX 512
uint8_t buffer[TX_BUFFER_MAX + 1];
int tx_buffer_index = 0;

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

File root;

/*
* Définition des champs du fichier de configuration. (création d'un type Struct)
* Pour ajouter ou supprimer, ne pas oublier de modifier également
* La fonction "LoadConfiguration" et la fonction "SaveConfiguration"
*/
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

Config config; //Déclaration de l'objet struct de configuration nommé "config"

/////////////////////////////////////////////////////////////////////////////

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
  StaticJsonDocument<512> doc; //Allocation de mémoire pour la désérialisation JSON des paramètres
  DeserializationError error = deserializeJson(doc, file); // Désérialisation

  if (error)
  log("Failed to read file, using default configuration", 3);

  // Copie des valeurs issues du fichier vers les champs associés du Struct config
  strlcpy(config.BDH,                  // <- destination
    doc["BDH"] | BDH,  // <- source
    sizeof(config.BDH));         // <- destination's capacity

  strlcpy(config.stationName,
    doc["stationName"] | stationName,
    sizeof(config.stationName));
    
  strlcpy(config.PublicIP,
    doc["PublicIP"] | PublicIP,
    sizeof(config.PublicIP));

  config.captureMode = doc["Capture_Mode"] | captureMode; //méthode différente car type Int au lieu de String
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


  // On ferme le fichier sans quoi une corruption peut avoir lieu
  file.close();
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
    log("Failed to create config file", 2);
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
*/ 
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

/* 
* Fonction Callback étant appelée automatiquement lorsque des informations sont reçues du cloud Particle
* V1.0 2019-11-14 
*/
void handler(const char *topic, const char *data) {
 
    log("received from Particle cloud " + String(topic) + ": " + String(data), 4);

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
    Serial.println("SD Card initialization failed!! Resetting in 5 seconds");
    digitalWrite(statusLed, HIGH);
    delay(5000);
    System.reset();
  }
}

/* 
* Fonction qui envoie un fichier sur le site ftp spécifié
* V1.0 2019-11-14 
*/
bool syncFTP(String SDfilename, bool retry, String dir, bool islog) {
  
  //Déclaration de variable temporaire
  static const size_t bufferSize = 512;// grosseur des blocs de données à envoyer 
  static uint8_t buffer[bufferSize] = {0xFF}; //Terminer le bloc par un 0xFF
  String DestFilename = "";
  char buffersprint1 [50]; // Variables temporaires pour générer les noms de fichiers et 
  char buffersprint2 [50]; // chemins d'arborescence

  // Connection à l'hôte FTP avec les infos fournies
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

  // On "navigue" vers le chemin de dossier passé en paramètre (dir)
  if (!ftp.cwd(dir)){
    log("could not set directory", 3);
    if(!ftp.mkd(dir)){ //S'il n'existe pas on le crée
      log("ftp mkdir failed: " + dir, 2);
      return 0; // retour de la fonction en erreur si le dossier n'a pu être créé
    }
    log("ftp mkdir success: " + dir, 3);
  }

  // Si le fichier passé en paramètre n'ets pas un log (typiquement une image), 
  // on configure les chemins d'arborescence et les noms de fichiers
  if(!islog) {
    //Nom de l'image à destination
    sprintf(buffersprint1,"%s_%d%02d%s",stationName.c_str(), now.year(),now.month(),SDfilename.c_str());
    DestFilename = buffersprint1;

    //nom et chemin de l'image sur la carte SD 
    sprintf(buffersprint2,"%s/%d/%02d/%s",stationName.c_str(),now.year(), now.month(),SDfilename.c_str());
    SDfilename = buffersprint2;
  } else  {
      // S'il s'agit d'un log, le nom du fichier à destination ne change pas et l'arborescence 
      // dans la carte SD n'est pas la même (LOGS/moisAnnée.txt)
      sprintf(buffersprint2,"/%s/%s", "LOGS",SDfilename.c_str());
      DestFilename = SDfilename;
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
  log("Tfile: " + DestFilename , 4);
  File SDfile = SD.open(SDfilename);
  delay(100);
  if (SDfile) {
    if(!islog) {
      if (!ftp.type("I")) { //Si image mettre type "I" pour binary transfer
        //log("Couldn't set file type", 2);
      } else if (!ftp.stor(DestFilename)) {
        //log("Couldn't STOR file: " + DestFilename, 2);
        return 0;
      }
    } else if (islog) {
        if (!ftp.type("A")) { //Si log, utiliser le type "A" pour transfert standard
        //log("Couldn't set file type", 2);
      } else if (!ftp.stor(DestFilename)) { //Tenter de créer le fichier à destination
        //log("Couldn't STOR file: " + DestFilename, 2);
        return 0;
      }  
    }

    //Vérifier la taille du fichier, 524287 bytes est trop gros, 0 indique un fichier vide
    size_t len = SDfile.size();
    if (len >= 0x07ffff) {
        //log("Over size file.", 2);
        return 0;
      } else if (len == 0 ){
        //log("File size is 0.", 2);
        return 0;
      } 

      //Variables temporaires pour la gestion du transfert
      //log("length of file is: " + len, 4);
      uint8_t temp_last;
      uint8_t temp;
      int bytesRead = 0;
      unsigned long previousMillis = 0; 
      const long interval = 1000;
      tx_buffer_index = 0;

      //Tant que le fichier n'est pas vide
      while(SDfile.available()) {
        Particle.process(); //Donner signe de vie au cloud
        temp_last = temp;
        temp = SDfile.read();
        bytesRead++;
        buffer[tx_buffer_index++] = temp;

        // Quand l'incrément est rendu au max, on transfère le contenu du buffer vers 
        // le site ftp, On fait scintiller la Led pour indiquer le transfert en cours
        if (tx_buffer_index >= TX_BUFFER_MAX) {
            digitalWrite(statusLed, LOW);
            ftp.data.write(buffer, tx_buffer_index);
            tx_buffer_index = 0;
            Particle.process();
            digitalWrite(statusLed, HIGH);
        }
      delayMicroseconds(2000); //Il faut laisser le temps à la chip cellulaire de transférer le bloc

      // Si on dépasse 600000 bytes, il y a un problème...
      if (bytesRead > 600000) {
          // failsafe
          return 0;
      }
      // S'assurer que nous sommes bien encore connectés durant le transfert
      if (!ftp.connected()) {
        //log("connection failed", 2);
        return 0;
      }       
    }
    //Si il reste des données dans le buffer, on les transfère
    if (tx_buffer_index != 0) {
          ftp.data.write(buffer, tx_buffer_index);
    }        
    Particle.process(); //On donne signe de vie au cloud
    SDfile.close(); // On ferme notre fichier
  } else {
    // Si nous n'avons pas pu ouvrir le fichier
    log("Could not open File on SD... Aborting", 2);
    return 0;
    }

    //On ferme le transfert, vide le contenu du port ftp et quitte la connexion 
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

/* 
* Fonction qui récupère le fichier de configuration sur le site ftp et se met à jour
* V1.0 2019-11-14 
*/
bool getConfig(String dir) {
  // Connction au site ftp au les identifiants fournis
  log("up", 4);
  
  if (!ftp.open(hostname, port, timeout)) {
    log("Couldn't connect to ftp host",2);
    while (1);
  }
  if (!ftp.user(username)) {
    log("Bad username", 2);
    while (1);
  }
  if (!ftp.pass(password)) {
    log("Bad password", 2);
    while (1);
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
  if (!ftp.retr(ftp_dir + "/config/" + BDH + "/CONFIG.JSN")) {
    log("Couldn't retrieve CONFIG.JSN", 2);
    return 0;
  } else { //l'opération a réussi, nous pouvons supprimer le fichier de configuration pour le mettre à jour
    if(SD.exists("CONFIG.JSN")) { //S'il existe...
      SD.remove("CONFIG.JSN");
    }

    //On crée un nouveau fichier de configuration et on vide le contenu du buffer TCP dans ce fichier.
    File file = SD.open("CONFIG.JSN",  FILE_WRITE); 
    while (ftp.data.connected()) {
      while (ftp.data.available()) {
      file.write(ftp.data.read());
      }
    }

    // Fin du transfert, on ferme la connexion.
    file.close();
    ftp.finish();
    ftp.quit();

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
* V1.0 2019-11-14 
*/
int grabPic(String Short_filename) {
  uint8_t vid,pid;
  uint8_t temp;
  uint8_t temp_last;
  
  //Vérification de la comm avec la caméra, je n'aime pas le while(1) (provient de l'exemple de la librairie)
  // à modifier éventuellement 
  while(1) {
    // La caméra est alimentée par un "latching relay", attention, elle consomme beaucoup.
    digitalWrite(Set_rly, HIGH);
    delay(200);
    digitalWrite(Set_rly, LOW);
    delay(500);  

    //On vérifie si la caméra répond sur le bus SPI1
    myCAM.write_reg(ARDUCHIP_TEST1, 0x55);
    temp = myCAM.read_reg(ARDUCHIP_TEST1); //ce régistre est déié pour ça
    if(temp != 0x55)
    {
      log("SPI interface Error! resetting in 5 seconds, myCam.read_reg said " + String(temp), 1);
      delay(5000);
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

    //Ce registre devrait indiquer 2641 ou 2642
    if ((vid != 0x26 ) && (( pid != 0x41 ) || ( pid != 0x42 ))){
      log("Can't find OV2640 module! camera says " + String::format("%d:%d", vid, pid), 1);
      delay(5000);
      continue;
    }
    else { 
      break;
    }
  }

  //myCAM.write_reg(ARDUCHIP_MODE, 0x01);		 	//Viens de la librairie, usage inconnu

  // On utilise la compression JPEG
  myCAM.set_format(JPEG);
  delay(100);

  // On initalise la caméra
  myCAM.InitCAM();
  delay(100);

  //myCAM.set_bit(ARDUCHIP_TIM, VSYNC_LEVEL_MASK); //Non utilisé pour le modèle OV2640
  //myCAM.write_reg(ARDUCHIP_TIM, VSYNC_LEVEL_MASK);
  //delay(100);

  //myCAM.write_reg(ARDUCHIP_FRAMES,0x00); //Usage inconnu
  //delay(100);


  // On sélectionne ici la résolution, à mettre à jour
  //myCAM.OV5642_set_JPEG_size(OV5642_320x240);   //works
  //myCAM.OV5642_set_JPEG_size(OV5642_640x480);   // works
  //myCAM.OV5642_set_JPEG_size(OV5642_1600x1200); // works
  //myCAM.OV5642_set_JPEG_size(OV5642_1280x960);  // works
  myCAM.OV2640_set_JPEG_size(OV2640_1600x1200); //works
  //myCAM.OV2640_set_JPEG_size(OV2640_1280x1024);

  // On flush toute info associée à une photo antérieure avant de prendre une photo
  myCAM.clear_fifo_flag();
  myCAM.flush_fifo();
  delay(100);
  myCAM.clear_fifo_flag();
  delay(100);
  myCAM.start_capture();
  delay(100);

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
  delay(100);
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
    static int k = 0;
    uint8_t temp, temp_last;
    File file;
    String dir;
    myCAM.CS_HIGH(); //On s'assure de taire la caméra sur le Bus SPI
    delay(100);
    
    // Si on est offline, on tente de récupérer le nom de station à partir du dossier de photo
    // Obsolet depuis l'ajout du fichier de config? 
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

    // On crée le dossier s'il n'existe pas
    if(!SD.exists(dir)){
      //log("Directory does not exist, trying to create it...", 3);
      if(!SD.mkdir(dir)){
      //log("SD mkdir failed", 2);
      } 
    } 

    // On génère le nom de l'arborescence + le nom du fichier pour le créer au bon endroit
    String namePlusDir = "";
    namePlusDir = dir + Short_filename;
    file = SD.open(namePlusDir, O_WRITE | O_CREAT | O_TRUNC);
    delay(100);

    // Si on a pas réussi à le créer, on détruit l'image dans le buffer de la caméra et on retourne en erreur
    if(!file){
      //log("open file failed: " + namePlusDir, 1);
      myCAM.flush_fifo();
      delay(100);
      myCAM.clear_fifo_flag();
      delay(5000);
      return false;
    }

    // Le fichier d'image doit commencer par 0xFF, étrangement ce n'est pas dans le buffer de la caméra
    file.write(0xFF);

    //On s'adresse maintenant à la caméra
    myCAM.CS_LOW();
    delay(100);
    //log("Capture Done.", 4);
    Particle.publish("status", "Capture done");

    // On veut lire l'image dans le buffer bit par bit
    myCAM.set_fifo_burst();

    tx_buffer_index = 0;
    temp = 0;

    // Tant que les deux derniers octets ne sont pas 0xFF et 0xD9, nous ne sommes pas à la fin du fichier
    // 0xFF et 0xD9 sont les marqueurs standards de la fin d'un JPG
    while( (temp != 0xD9) | (temp_last != 0xFF) )
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

    //On éteint la caméra pour économiser le plus d'énergie possible
    digitalWrite(Reset_rly, HIGH);
    delay(200);
    digitalWrite(Reset_rly, LOW);
    return 1;
  }
}

/* 
* Fonction inscrivant les entrées dans le log
* V1.0 2019-11-14
*/
void log(String msg, int Loglevel) {
  // Tout ce qui est envoyé au log est également envoyé au port série
  Serial.println(msg);

  // Dans le dossier logs/
  String dir ="logs/" ;
  // On prend l'heure actuelle
  DateTime logtime = rtc.now();
  String logFileName = dir + String(monthOfTheYear[logtime.month()-1]) + String(logtime.year()) + ".txt";
  File file;

  // on crée le répertoire s'il n'existe pas
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
    
    //On imprime dans le log sous le format suivant
    //"LOGLEVEL"     :AAAAMMDDHHMMSS: "MSG"
    file.print(String(logtime.year()) + String(logtime.month()) + String(logtime.day()) + String(logtime.hour()) + String(logtime.minute()) + String(logtime.second()) +": ");
    file.println(msg);
    
    file.close();
  }
}


/* 
* Fonction concierge pour éviter le trop plein de données
*
*/
void cleanSD() {
  int FolderRmvd = 0;
  int FileRmvd = 0;
  log("clSD", 4);
  File root = SD.open(stationName + "/");
  File entry = root.openNextFile();

  while (entry) {
    String entryname = entry.name();
    int nowyear = now.year();
    Serial.println(entryname.toInt());
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
  log("Rd " + String(FolderRmvd), 4);
  log("Rf " + String(FileRmvd), 4);
  // retirer les logs qui datent de plus d'un an
}


/* 
* Fonction d'initialisation qui s'exécute une seule fois au démarrage de l'appareil
* V1.0 2019-11-14 
*/
void setup() {

  //On configure les variables, fonctions et souscriptions disponibles 
  // dans le cloud ainsi que d'autres paramètres
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
  // On initalise les GPIO 
  pinMode(Set_rly, OUTPUT);
  pinMode(Reset_rly, OUTPUT);
  pinMode(statusLed, OUTPUT);
  pinMode(D8, INPUT_PULLUP);

  //Initialisation I2C (RTC et Arducam)
  Wire.setSpeed(CLOCK_SPEED_100KHZ);
  Wire.begin();
  
  //Initialisation de divers périphériques
  rtc.begin();
  Serial.begin(115200);
  rtc.clear_rtc_interrupt_flags();
  initSD();
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
  
  // Configuration des ports "Chip select" des bus SPI (Carte SD, Arducam et périphériques internes)
  pinMode(D7, OUTPUT);
  pinMode(SPI_CS, OUTPUT);
  digitalWrite(SPI_CS, HIGH);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);

  // initialisation du SPI1, la carte SD est sur le SPI de base et Arducam est sur SPI1 (conflit de CS)
  // Le SPI pour la carte SD est initialisé dans SD.begin()
  SPI1.begin(SPI_CS);
  //SPI.begin(SPI_MODE_MASTER);
  //SPI.begin(SPI_MODE_SLAVE, SPI_CS);

  // On met à jour nos variables de configuration
  loadConfiguration(configFileName,config);
}

/* 
* Le loop se répète en boucle automatiquement
* V1.0 2019-11-14
*/
void loop() {
  // Variables temporaires propre à cette boucle
  int secstoTimeout = timeout;
  String Nametocard;
  uint8_t buf[4];
  Particle.process(); // on oublie pas le cloud
   
  // Est-ce qu'on sort d'un sleep mode? si oui on reset le système
  // System.sleep(); permet de garder en mémoire les variables, 
  // toutefois le système à été conçu pour être en ""deep_Sleep" ce qui désalimente la RAM
  // Pour une meilleure économie, mais nécessite un signal HIGH sur D8 pour redémarrer 
  // le controleur. Le system.reset est donc un patch avant de trouver une meilleure solution
  SleepResult result = System.sleepResult();

  if(result.wokenUpByRtc() || result.wokenUpByPin() ){
    log("Wu", 4);
    System.reset();
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
        offlinemode = true;
        log("Timeout, Setting mode to offline", 3);
        break;
      }
    }

  // Décompte du temps de connection en secondes
  thatTook =  (millis() - previousMillis)/1000;
  log("TT " + String(thatTook), 4);

  getConfig(ftp_dir + "/config/" + BDH + "/");
  // Opérations à faire suelement si nous avons une connection réseau confirmée
  // /!\ Attention, Le mode offline ne supporte pas encore l'envoie de photo en différé 
  if(!offlinemode) {
    Particle.publish("particle/device/ip", PRIVATE);
    Particle.publish("particle/device/name");

    // On obtient le niveau de batterie et de signal cell pour le log
    String SOC = String(fuel.getSoC());
    String VCell = String(fuel.getVCell());
    CellularSignal sig = Cellular.RSSI();
    Particle.publish("Batt_SOC",SOC);
    log("SOC; " + SOC + ";Vb;" + VCell + ";RSSI;" + String(sig.rssi) + ";Qual; " + String(sig.getQuality()) + ";", 4);
    // ... et pour le cloud
    Particle.publish("RSSI",String(sig.rssi));

    // On force un synchronisation du temps de l'horloge interne avec le cloud
    Particle.syncTime();
    while(!Particle.syncTimeDone() && timeout > 0) {
      delay(500);
      timeout --;
    }

    // Si on a reçu confirmation avant la fin du timeout, on synchronise l'horloge interne avec la RTC
    if(timeout > 0) {
      rtc.adjust(DateTime(Time.year(), Time.month(), Time.day(), Time.hour(), Time.minute(), Time.second()));
      Particle.syncTimeDone();
      Particle.publish("Status","Time Synced!");
      log("TSync", 4);
     } else {
       log("Time Sync failed!", 2);
     }
    
    // On attends de recevoir les infos du cloud
    while(BDH == "undef"){
      Particle.process();
    }

    // On sauvegarde la nouvelle configuration
    saveConfiguration(configFileName,config);
  }

  // On prélève l'heure actuelle et on l'affiche sur le port série
  now = rtc.now();
  Serial.print(now.year());
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
  char buffer[40];

  cleanSD();

  // On génère le nopm du fichier à partir de l'heure actuelle, on force 2 caractères par entrée
  // exemple: 02 au lieu de 2
  sprintf(buffer,"%02d%02d%02d%02d.jpg", now.day(),now.hour(), now.minute(), now.second());
  Nametocard = buffer;

  // On déclanche la prise de photo avec le nom généré précédemment
  if(!grabPic(Nametocard)){
    log("GrabPic failed, name of file: " + Nametocard, 1);
    delay(5000);
    System.reset(); // Reset si un échec
  }   

  // On déclanche la synchronisation FTP de l'image (à désactiver en cas de mode offline) 
  // 3 essais avant d'abandonner, 5 secondes entre les essais
  int attempts = 0;
  if(!syncFTP(Nametocard, false,ftp_dir, 0)){
      log("Isync fail", 2);
      while(!syncFTP(Nametocard, true,ftp_dir, 0) && attempts < 3){
        log("Isync fail again", 2);
        Particle.process();
        attempts++;
        delay(5000);
      }
    } else {
      log("ISuc", 4);
    }   

  attempts = 0;
  // On déclanche la synchronisation FTP du Log (à désactiver en cas de mode offline) 3 essais...
  if(!syncFTP(String(monthOfTheYear[now.month()-1]) + String(now.year()) + ".txt", true,ftp_dir + "/config/" + BDH, 1)){
      log("Lsync fail", 2);
      while(!syncFTP(String(monthOfTheYear[now.month()-1]) + String(now.year()) + "txt", true,ftp_dir + "/config/" + BDH, 1) && attempts < 3){
        log("Lsync fail again", 2);
        Particle.process();
        attempts++;
        delay(5000);
      }
    } else {
      log("LSuc", 4);
      attempts = 0;
    }    

  Particle.publish("status", "Sleeping");
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
  // On ferme le modem cellulaire, si on ne le fait pas, il va rester en STANDBY, Consommant trop d'énergie
  Cellular.off();

  // On dort pour le nombre prédéterminé de secondes. 
  // On peut aussi le repartir avec un front descendant sur la pin D8, 
  // possible ajout d'un bouton de prise d'Image?
  System.sleep(D8,FALLING,secsToWakeup);
}