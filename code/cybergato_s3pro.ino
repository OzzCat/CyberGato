#include <HardwareSerial.h>
#include <ESPAsyncWebServer.h>
#include <RTClib.h>
#include <WiFi.h>
#include <TinyGPSPlus.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"

RTC_DS3231 rtc;

TinyGPSPlus gps;
unsigned long previousGPSMillis = 0;
double previousLat = 0;
double previousLong = 0;

const char* ap_ssid = "CiberGato_2";
const char* ap_pwd = "12345678";

bool SDCardStatus = false;
bool RTCStatus = false;
bool GPSStatus = false;

String currentCatchFileName;
String currentCoordinateFileName;
File currentCatchFile;
File currentCoordinateFile;

String lastCatch = "No one came yet";

String webpage = "";

AsyncWebServer server(80);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  
  Serial.println("Hello world!");

  initRTC();

  initSD();

  initAP();

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    Home();
    request->send(200, "text/html", webpage);
  });

  server.on("/settime", HTTP_GET, [](AsyncWebServerRequest *request){
    SetDT();
    request->send(200, "text/html", webpage);
  });

  server.on("/settime", HTTP_POST, [](AsyncWebServerRequest *request){
    int year, month, day, hour, minute, second;
    if (request->hasParam("year", true)) {
      year = request->getParam("year", true)->value().toInt(); }
    if (request->hasParam("month", true)) {
      month = request->getParam("month", true)->value().toInt(); }
    if (request->hasParam("day", true)) {
      day = request->getParam("day", true)->value().toInt(); }
    if (request->hasParam("hour", true)) {
      hour = request->getParam("hour", true)->value().toInt(); }
    if (request->hasParam("minute", true)) {
      minute = request->getParam("minute", true)->value().toInt(); }
    if (request->hasParam("second", true)) {
      second = request->getParam("second", true)->value().toInt(); }
    else {
      second = 0; }
    rtc.adjust(DateTime(year, month, day, hour, minute, second));
    initFiles();
    request->redirect("/");
  });

  server.on("/catchfilespage", HTTP_GET, [](AsyncWebServerRequest *request){
    CatchFilesPage();
    request->send(200, "text/html", webpage);
  });

  server.on("/opencatchfile", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String file = "/CatchData/";
    if (request->hasParam("filename")) {
      file += request->getParam("filename")->value();
    }
    request->send(SD, file, "text/plain", false);
  });

  server.on("/downloadcatchfile", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String file = "/CatchData/";
    if (request->hasParam("filename")) {
      file += request->getParam("filename")->value();
    }
    request->send(SD, file, String(), true);
  });

  server.on("/deletecatchfile", HTTP_POST, [] (AsyncWebServerRequest *request) {
    String file = "/CatchData/";
    if (request->hasParam("filename", true)) {
      file += request->getParam("filename", true)->value();
      SD.remove(file);
    }
    request->redirect("/catchfilespage");
  });

  server.on("/coordinatefilespage", HTTP_GET, [](AsyncWebServerRequest *request){
    CoordinateFilesPage();
    request->send(200, "text/html", webpage);
  });

  server.on("/opencoordinatefile", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String file = "/CoordinateData/";
    if (request->hasParam("filename")) {
      file += request->getParam("filename")->value();
    }
    request->send(SD, file, "text/plain", false);
  });

  server.on("/downloadcoordinatefile", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String file = "/CoordinateData/";
    if (request->hasParam("filename")) {
      file += request->getParam("filename")->value();
    }
    request->send(SD, file, String(), true);
  });

  server.on("/deletecoordinatefile", HTTP_POST, [] (AsyncWebServerRequest *request) {
    String file = "/CoordinateData/";
    if (request->hasParam("filename", true)) {
      file += request->getParam("filename", true)->value();
      SD.remove(file);
    }
    request->redirect("/coordinatefilespage");
  });

  server.on("/reconnectsd", HTTP_GET, [] (AsyncWebServerRequest *request) {
    initSD();
    request->redirect("/");
  });

  server.on("/reconnectrtc", HTTP_GET, [] (AsyncWebServerRequest *request) {
    initRTC();
    request->redirect("/");
  });

  server.begin();

  Serial1.begin(9600, SERIAL_8N1, 38, 41);

  Serial2.begin(9600, SERIAL_8N1, 16, 17);

}

void loop() {
  // put your main code here, to run repeatedly:
  unsigned long currentMillis = millis();

  if (currentMillis > 600000 && (currentMillis - previousGPSMillis) > 300000)
  {
    while (Serial1.available() > 0)
    if (gps.encode(Serial1.read()))
    {
      GPSStatus = true;

      if (gps.hdop.hdop() < 2 && gps.satellites.value() > 4 && gps.location.isUpdated())
      {
        currentCoordinateFile = SD.open(currentCoordinateFileName, FILE_APPEND);
        printGPSInfo(currentCoordinateFile);
        currentCoordinateFile.close();
        previousGPSMillis = currentMillis;
        previousLat = gps.location.lat();
        previousLong = gps.location.lng();
      }
    }   
  } 

  while (Serial2.available() > 0)
  {
    String chipData = Serial2.readString();
    if (checkChipString(chipData))
    {
      //Serial.println(chipData.substring(2, 17));
      currentCatchFile = SD.open(currentCatchFileName, FILE_APPEND);
      printCatchInfo(currentCatchFile, chipData);
      currentCatchFile.close();
    }
  } 
}

void initAP(){
  WiFi.softAP(ap_ssid, ap_pwd);
  IPAddress myIP = WiFi.softAPIP();  
  Serial.println("Access point started");
  Serial.print("AP IP address: ");
  Serial.println(myIP);
}

void initRTC()
{
  if (! rtc.begin() ) 
  {
    Serial.println("RTC module is NOT found");
  }
  else
  {
    Serial.println("RTC module started");
    RTCStatus = true;
  }
}

void initSD(){
  if (!SD.begin()) {
    Serial.println("Error preparing Filing System...");
    return;
  }
  uint8_t cardType = SD.cardType();
  if(cardType == CARD_NONE){
    Serial.println("No SD card attached");
    return;
  }

  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);

  if (SD.mkdir("/CatchData")) {
    Serial.println("Catch dir ok");
  } else {
    Serial.println("FS on SD failed");
    return;
  }
  if (SD.mkdir("/CoordinateData")) {
    Serial.println("Coord dir ok");
  } else {
    Serial.println("FS on SD failed");
    return;
  }

  initFiles();

  SDCardStatus = true;
}

void initFiles(){
  String Name =  getDatetimeFileFormat();

  currentCatchFileName = "/CatchData/" + Name + " Catch.txt";
  if(!SD.exists(currentCatchFileName))
  {
    Serial.println("Creating catch file...");
    currentCatchFile = SD.open(currentCatchFileName, FILE_WRITE, true);
    currentCatchFile.print("Datetime RFID \n");
    currentCatchFile.close();
  }
  else
  {
    Serial.println("Catch file already exists");
  }

  currentCoordinateFileName = "/CoordinateData/" + Name + " Coordinate.txt";
  if(!SD.exists(currentCoordinateFileName))
  {
    Serial.println("Creating coordinate file...");
    currentCoordinateFile = SD.open(currentCoordinateFileName, FILE_WRITE, true);
    currentCoordinateFile.print("Datetime Latitude Longitude HDOP DistanceFromPreviousMeasure \n");
    currentCoordinateFile.close();
  }
  else
  {
    Serial.println("Coordinate file already exists");
  }
}

void Home() {
  webpage = "<!DOCTYPE HTML><html><head>";
  webpage += "<title>CiberGato</title>";
  webpage += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  webpage += "</head><body>";
  webpage += "<h1>CiberGato</h1>";

  if (RTCStatus)
  {
    webpage += "<p>Current device time: ";
    webpage += getDatetimeUserFormat();
    webpage += "</p>";
    webpage += "<a href='/settime'> Set time </a><br>";
  }
  else
  {
    webpage += "<p>Real time clock module is not working</p>";
    webpage += "<a href='/reconnectrtc'> Try to connect real time clock again? </a><br>";
  }

  if (GPSStatus)
  {
    webpage += "<p>GPS Module is working good</p>";
    webpage += "<p>Current coordinates:</p>";
    webpage += "<p>" + String(previousLat, 6) + ", " + String(previousLong, 6) + "</p>";
  }
  else if (millis() < 600000)
  {
    webpage += "<p>GPS Module is warming up</p>";
  }
  else
  {
    webpage += "<p>GPS Module is not working</p>";
  }

  if (SDCardStatus)
  {
    webpage += "<p>SD Card is working good</p>";
    webpage += "<a href='/catchfilespage'> Show catch files </a><br>";
    webpage += "<a href='/coordinatefilespage'> Show coordinate files </a>";
  }
  else
  {
    webpage += "<p>SD Card is not working</p>";
    webpage += "<a href='/reconnectsd'> Try to connect to SD card again? </a><br>";
  }
  webpage += "<p>Last catch:</p>";
  webpage += "<p>" + lastCatch + "</p>";
  
  webpage += "</body></html>";
  return;
}

void SetDT() {
  webpage = "<!DOCTYPE HTML><html><head>";
  webpage += "<title>CiberGato</title>";
  webpage += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  webpage += "<style>input { margin: 5px;  }</style>";
  webpage += "<script language = 'JavaScript'>function syncTime() {";
  webpage += "data = new FormData(); ";
  webpage += "let d = new Date();";
  webpage += "data.append('day', d.getDate() );";
  webpage += "data.append('month', d.getMonth() + 1 );";
  webpage += "data.append('year', d.getFullYear() );";
  webpage += "data.append('hour', d.getHours() );";
  webpage += "data.append('minute', d.getMinutes() );";
  webpage += "data.append('second', d.getSeconds() );";
  webpage += "let result = fetch('/settime', {method:'post', body:data } );";
  webpage += "window.location.href='/';";
  webpage += "} </script>";
  webpage += "</head><body>";
  webpage += "<a href='/'> Return to main page </a><br>";
  webpage += "<p>Sync date and time with the device:</p>";
  webpage += "<input type='button' value='Sync time' onclick='syncTime()'>";
  webpage += "<p>Or set date and time manually:</p>";
  webpage += "<form action='/settime' method='post'>";
  webpage += "Day: <input type='number' min='1' max='31' name='day'><br>";
  webpage += "Month: <input type='number' min='1' max='12' name='month'><br>";
  webpage += "Year: <input type='number' min='0' max='2100' name='year'><br>";
  webpage += "Hour: <input type='number' min='0' max='23' name='hour'><br>";
  webpage += "Minute: <input type='number' min='0' max='59' name='minute'><br>";
  webpage += "<input type='submit' value='Set time'></form>";
  webpage += "</body></html>";
  return;
}

void CatchFilesPage() {
  webpage = "<!DOCTYPE HTML><html><head>";
  webpage += "<title>CiberGato</title>";
  webpage += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  webpage += "<style>table, th, td { border: 1px solid black; border-collapse: collapse; }";
  webpage += "td { padding: 5px;  } </style>";
  webpage += "</head><body>";
  webpage += "<h1>List of catch files</h1>";
  webpage += "<a href='/'> Return to main page </a><br><br>";

  File root = SD.open("/CatchData");
  if(!root || !root.isDirectory()){
    webpage += "<h1>Error opening directory, check SD card</h1>";
    webpage += "</body></html>";
    return;
  }

  webpage += "<table>";
  
  File file = root.openNextFile();
  while(file){
    webpage += "<tr><td>" + String(file.name()) + "</td>";
    webpage += "<td><form action='/opencatchfile'>";
    webpage += "<button type='submit' name='filename'";
    webpage += "value='" + String(file.name()) + "'>Open</button></form></td>";
    webpage += "<td><form action='/downloadcatchfile'>";
    webpage += "<button type='submit' name='filename'";
    webpage += "value='" + String(file.name()) + "'>Download</button></form></td>";
    webpage += "<td><form action='/deletecatchfile' method='post' ";
    webpage += "onsubmit=\"return confirm('Are you sure you want to delete file " + String(file.name()) + "?');\">";
    webpage += "<button type='submit' name='filename'";
    webpage += "value='" + String(file.name()) + "'>Delete</button></form></td>";
    webpage += "</tr>";
    file = root.openNextFile();
  }

  webpage += "</table>";

  file.close();

  webpage += "</body></html>";
  return;
}

void CoordinateFilesPage() {
  webpage = "<!DOCTYPE HTML><html><head>";
  webpage += "<title>CiberGato</title>";
  webpage += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  webpage += "<style>table, th, td { border: 1px solid black; border-collapse: collapse; }";
  webpage += "td { padding: 5px;  } </style>";
  webpage += "</head><body>";
  webpage += "<h1>List of coordinate files</h1>";
  webpage += "<a href='/'> Return to main page </a><br><br>";

  File root = SD.open("/CoordinateData");
  if(!root || !root.isDirectory()){
    webpage += "<h1>Error opening directory, check SD card</h1>";
    webpage += "</body></html>";
    return;
  }

  webpage += "<table>";
  
  File file = root.openNextFile();
  while(file){
    webpage += "<tr><td>" + String(file.name()) + "</td>";
    webpage += "<td><form action='/opencoordinatefile'>";
    webpage += "<button type='submit' name='filename'";
    webpage += "value='" + String(file.name()) + "'>Open</button></form></td>";
    webpage += "<td><form action='/downloadcoordinatefile'>";
    webpage += "<button type='submit' name='filename'";
    webpage += "value='" + String(file.name()) + "'>Download</button></form></td>";
    webpage += "<td><form action='/deletecoordinatefile' method='post' ";
    webpage += "onsubmit=\"return confirm('Are you sure you want to delete file " + String(file.name()) + "?');\">";
    webpage += "<button type='submit' name='filename'";
    webpage += "value='" + String(file.name()) + "'>Delete</button></form></td>";
    webpage += "</tr>";
    file = root.openNextFile();
  }

  webpage += "</table>";

  file.close();

  webpage += "</body></html>";
  return;
}

String getDatetimeUserFormat() 
{
  String date;
  DateTime now = rtc.now();
  date = String(now.day());
  date += '/';
  date += String(now.month());
  date += '/';
  date += String(now.year());
  date += ' ';
  date += String(now.hour());
  date += ':';
  date += String(now.minute());
  date += ':';
  date += String(now.second());
  return date;
}

String getDatetimeFileFormat() 
{
  String date;
  DateTime now = rtc.now();
  date = String(now.day());
  date += '-';
  date += String(now.month());
  date += '-';
  date += String(now.year());
  date += '_';
  date += String(now.hour());
  date += '-';
  date += String(now.minute());
  return date;
}

void printGPSInfo(File coordinateData)
{
  String time = getDatetimeUserFormat();
  coordinateData.print(time);
  coordinateData.print(" ");
  if (gps.location.isValid())
  {
    coordinateData.print(gps.location.lat(), 6);
    coordinateData.print(" ");
    coordinateData.print(gps.location.lng(), 6);
    coordinateData.print(" ");
  }
  else
  {
    coordinateData.print("INVALID ");
  }

  if (gps.hdop.isValid())
  {
    coordinateData.print(gps.hdop.hdop());
    coordinateData.print(" ");
  }
  else
  {
    coordinateData.print("INVALID ");
  }

  coordinateData.print(gps.distanceBetween(previousLat, previousLong, gps.location.lat(), gps.location.lng()));

  coordinateData.print("\n");
}

void printCatchInfo(File catchData, String chipData)
{
  String time = getDatetimeUserFormat();
  catchData.print(time);
  catchData.print(" ");
  catchData.print(chipData.substring(2, 17));
  catchData.print("\n");
  lastCatch = time + " " + chipData.substring(2, 17);
}

bool checkChipString(String chipData)
{
  if (chipData.length() != 40)
    return false;
  if (chipData[0] != '$')
    return false;
  if (chipData[19] != '#')
    return false;
  return true;
}
