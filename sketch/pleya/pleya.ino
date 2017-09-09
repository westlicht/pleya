// pleya - Kids mp3 player

#include <SPI.h>
#include <SD.h>
#include <EEPROM.h>
#include <Adafruit_VS1053.h>

// These are the pins used
#define VS1053_RESET   -1     // VS1053 reset pin (not used!)

// Feather M0 or 32u4
#if defined(__AVR__) || defined(ARDUINO_SAMD_FEATHER_M0)
  #define VS1053_CS       6     // VS1053 chip select pin (output)
  #define VS1053_DCS     10     // VS1053 Data/command select pin (output)
  #define CARDCS          5     // Card chip select pin
  // DREQ should be an Int pin *if possible* (not possible on 32u4)
  #define VS1053_DREQ     9     // VS1053 Data request, ideally an Interrupt pin

// Feather ESP8266
#elif defined(ESP8266)
  #define VS1053_CS      16     // VS1053 chip select pin (output)
  #define VS1053_DCS     15     // VS1053 Data/command select pin (output)
  #define CARDCS          2     // Card chip select pin
  #define VS1053_DREQ     0     // VS1053 Data request, ideally an Interrupt pin

// Feather ESP32
#elif defined(ESP32)
  #define VS1053_CS      32     // VS1053 chip select pin (output)
  #define VS1053_DCS     33     // VS1053 Data/command select pin (output)
  #define CARDCS         14     // Card chip select pin
  #define VS1053_DREQ    15     // VS1053 Data request, ideally an Interrupt pin

// Feather Teensy3
#elif defined(TEENSYDUINO)
  #define VS1053_CS       3     // VS1053 chip select pin (output)
  #define VS1053_DCS     10     // VS1053 Data/command select pin (output)
  #define CARDCS          8     // Card chip select pin
  #define VS1053_DREQ     4     // VS1053 Data request, ideally an Interrupt pin

// WICED feather
#elif defined(ARDUINO_STM32_FEATHER)
  #define VS1053_CS       PC7     // VS1053 chip select pin (output)
  #define VS1053_DCS      PB4     // VS1053 Data/command select pin (output)
  #define CARDCS          PC5     // Card chip select pin
  #define VS1053_DREQ     PA15    // VS1053 Data request, ideally an Interrupt pin

#elif defined(ARDUINO_FEATHER52)
  #define VS1053_CS       30     // VS1053 chip select pin (output)
  #define VS1053_DCS      11     // VS1053 Data/command select pin (output)
  #define CARDCS          27     // Card chip select pin
  #define VS1053_DREQ     31     // VS1053 Data request, ideally an Interrupt pin
#endif

#define DEBUG 1

// Number of playlists (maximum is 9)
static const int playlistCount = 9;

// Pin connected to a variable voltage divider used for volume control
static const int volumePin = 18; // A0

// Pins connected to buttons used for playback control
static const int playlistPins[playlistCount] = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
static const int backwardPin = 10;
static const int forwardPin = 10;

int currentPlaylist = -1;
int currentTrack = -1;
bool isPlaying = false;

File currentDir;
File currentFile;

Adafruit_VS1053_FilePlayer musicPlayer =
  Adafruit_VS1053_FilePlayer(VS1053_RESET, VS1053_CS, VS1053_DCS, VS1053_DREQ, CARDCS);

// write playlist/track to EEPROM
void writeEEPROM(int playlist, int track) {
#if DEBUG
  Serial.print("writeEEPROM("); Serial.print(playlist); Serial.print(", "); Serial.print(track); Serial.println(")");
#endif
  EEPROM.update(0, 0);
  EEPROM.update(1, 0xba);
  EEPROM.update(2, playlist);
  EEPROM.update(3, track);
  EEPROM.update(0, 0xab);
}

// read playlist/track from EEPROM
bool readEEPROM(int &playlist, int &track) {
#if DEBUG
  Serial.println("readEEPROM()");
#endif
  if (EEPROM.read(0) != 0xab || EEPROM.read(1) != 0xba) {
    return false;
  }
  playlist = EEPROM.read(2);
  track = EEPROM.read(3);
  return true;
}

// play next track in playlist or the specified track if track != -1
void playTrack(int playlist, int track = -1) {
#if DEBUG
  Serial.print("playTrack("); Serial.print(playlist); Serial.print(", "); Serial.print(track); Serial.println(")");
#endif

  if (isPlaying) {
    isPlaying = false;
    musicPlayer.stopPlaying();
    while (!musicPlayer.stopped()) {
      delay(1);
    }
  }

  char *path = "/x";
  path[1] = '0' + playlist;

  if (playlist != currentPlaylist || track != -1) {
    // open directory
    if (currentDir) {
      currentDir.close();
    }
    currentPlaylist = playlist;
    currentTrack = -1;
    currentDir = SD.open(path);
    currentDir.rewindDirectory();
  }

  if (!currentDir) {
#if DEBUG
    Serial.print("path '"); Serial.print(path); Serial.println("' does not exist!");
#endif
    return;
  }

  // open next file
  while (true) {
    currentFile = currentDir.openNextFile();
    if (!currentFile) {
      if (track != -1) {
        currentDir.rewindDirectory();
        currentTrack = -1;
        track = -1;
        continue;
      }
      if (currentTrack == -1) {
#if DEBUG
        Serial.print("path '"); Serial.print(path); Serial.println("' does not contain playable files!");
#endif
        return;
      }
      currentDir.rewindDirectory();
      currentTrack = -1;
      continue;
    }
    if (currentFile.size() <= 4096) {
      // skip small files that are artifacts from long file names
#if DEBUG
      Serial.print("skipping file '"); Serial.print(currentFile.name()); Serial.println("'");
#endif
      currentFile.close();
      continue;
    }
    ++currentTrack;
    if (track == -1) {
      break;
    } else {
      if (currentTrack == track) {
        break;      
      }
    }
    currentFile.close();
  }

#if DEBUG
  Serial.print("track index "); Serial.println(currentTrack);
#endif

  writeEEPROM(currentPlaylist, currentTrack);

  char trackName[64];
  trackName[0] = '0' + playlist;
  trackName[1] = '/';
  trackName[2] = '\0';
  strncat(trackName, currentFile.name(), sizeof(trackName));

  currentFile.close();

  isPlaying = true;
  if (!musicPlayer.startPlayingFile(trackName)) {
#if DEBUG
    Serial.print("failed to start playing "); Serial.println(trackName);
    isPlaying = false;
#endif
  }
}

// go backward in current track
void backward() {
#if DEBUG
  Serial.println("backward()");
#endif
  playTrack(currentPlaylist, currentTrack);
}

// go forward in current track
void forward() {
#if DEBUG
  Serial.println("forward()");
#endif
}


void setup() {
  // if you're using Bluefruit or LoRa/RFM Feather, disable the BLE interface
  //pinMode(8, INPUT_PULLUP);

#if DEBUG
  Serial.begin(115200);
  // Wait for serial port to be opened
  while (!Serial) {
    delay(1);
  }
  Serial.println("\n\npleya - Kids mp3 player");
#endif

  if (!musicPlayer.begin()) { 
#if DEBUG
     Serial.println("VS1053 not available!");
#endif
     while (1);
  }

  musicPlayer.setVolume(20, 20);
  
  if (!SD.begin(CARDCS)) {
#if DEBUG
    Serial.println("SD card not found!");
#endif
    musicPlayer.sineTest(0x44, 500);
    while (1);
  }
  
#if defined(__AVR_ATmega32U4__) 
  // Timer interrupts are not suggested, better to use DREQ interrupt!
  // but we don't have them on the 32u4 feather...
  musicPlayer.useInterrupt(VS1053_FILEPLAYER_TIMER0_INT); // timer int
#elif defined(ESP32)
  // no IRQ! doesn't work yet :/
#else
  // If DREQ is on an interrupt pin we can do background
  // audio playing
  musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT);  // DREQ int
#endif

  // play last played track if available or start first playlist
  int initialPlaylist;
  int initialTrack;
  if (readEEPROM(initialPlaylist, initialTrack)) {
    playTrack(initialPlaylist, initialTrack);
  } else {
    playTrack(0);
  }
}

void loop() {
  if (Serial.available()) {
    char c = Serial.read();
    switch (c) {
      case 'q': backward(); break;
      case 'w': forward(); break;
      case '1': playTrack(0); break;
      case '2': playTrack(1); break;
      case '3': playTrack(2); break;
      case '4': playTrack(3); break;
      case '5': playTrack(4); break;
      case '6': playTrack(5); break;
      case '7': playTrack(6); break;
      case '8': playTrack(7); break;
      case '9': playTrack(8); break;
      default: break;
    }
  }
  
  // handle volume pot
  int volumeValue = analogRead(volumePin);
  int volume = volumeValue / 10;
  volume = volume > 100 ? 100 : volume;
  volume = 100 - volume;
  musicPlayer.setVolume(volume, volume);
  delay(50);

  if (isPlaying && musicPlayer.stopped()) {
#if DEBUG
    Serial.println("track done playing");
#endif
    playTrack(currentPlaylist);
  }
}







