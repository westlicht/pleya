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
#else
  #error "Unsupported hardware platform! Use a Adafruit Feather 32u4 instead."
#endif

#define DEBUG 0

class Button {
public:
  void init(int pin) {
    _pin = pin;
    pinMode(_pin, INPUT_PULLUP);
  }

  bool pressed() {
    bool state = !digitalRead(_pin);
    if (state) {
      if (++_counter == DEBOUNCE) {
        return true;
      } else {
        _counter = min(DEBOUNCE, _counter);
      }
    } else {
      _counter = 0;
    }
    return false;
  }

private:
  static const byte DEBOUNCE = 50;
  int _pin;
  byte _counter = 0;
};

// Number of playlists (maximum is 9)
static const int playlistCount = 6;

// Pin connected to a variable voltage divider used for volume control
static const int volumePin = 18; // A0

// Pins connected to buttons used for playback control
static const int playlistPins[playlistCount] = { 13, 21, 19, 12, 22, 20 };
static const int backwardPin = 11;
static const int forwardPin = 23;

// Buttons
static Button playlistButtons[playlistCount];
static Button backwardButton;
static Button forwardButton;

int currentPlaylist = -1;
int currentTrack = -1;
bool isPlaying = false;

File currentDir;
File currentFile;

Adafruit_VS1053_FilePlayer musicPlayer =
  Adafruit_VS1053_FilePlayer(VS1053_RESET, VS1053_CS, VS1053_DCS, VS1053_DREQ, CARDCS);

// Write playlist/track to EEPROM
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

// Read playlist/track from EEPROM
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

// Play next track in playlist or the specified track if track != -1
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

  // Open next file
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
      // Skip small files that are artifacts from long file names
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

// Go backward in current track
void backward() {
#if DEBUG
  Serial.println("backward()");
#endif
  playTrack(currentPlaylist, currentTrack);
}

// Go forward in current track
void forward() {
#if DEBUG
  Serial.println("forward()");
  Serial.print("file size: "); Serial.println(musicPlayer.fileSize());
  Serial.print("file position: "); Serial.println(musicPlayer.filePosition());
  Serial.print("file progress: "); Serial.println((100L * musicPlayer.filePosition()) / musicPlayer.fileSize());
#endif
  musicPlayer.fileSeek(musicPlayer.filePosition() + (1024L * 256L));
}

void handleSerial() {
  // Handle keystroke over serial to simulate push buttons
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
}

void handleButtons() {
  for (int i = 0; i < playlistCount; ++i) {
    if (playlistButtons[i].pressed()) {
      playTrack(i);
    }
  }
  if (backwardButton.pressed()) {
    backward();
  }
  if (forwardButton.pressed()) {
    forward();
  }  
}

static int volumeFiltered = 0;

void handleVolume() {
  // Read volume pot (0..1023)
  int volumeValue = analogRead(volumePin);
  // Map to 0..100
  int volume = volumeValue / 10;
  volume = volume > 100 ? 100 : volume;
  volume = 100 - volume;
  // Lowpass filter to prevent volume jittering
  volumeFiltered = (volumeFiltered * 10 + volume) / 11;
  // Set volume
  musicPlayer.setVolume(volumeFiltered, volumeFiltered);
}

void handleNextTrack() {
  // Go to next track if current track finished
  if (isPlaying && musicPlayer.stopped()) {
#if DEBUG
    Serial.println("track done playing");
#endif
    playTrack(currentPlaylist);
  }  
}

void setup() {
  // Init buttons
  for (int i = 0; i < playlistCount; ++i) {
    playlistButtons[i].init(playlistPins[i]);
  }
  backwardButton.init(backwardPin);
  forwardButton.init(forwardPin);

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
  
  // Timer interrupts are not suggested, better to use DREQ interrupt!
  // but we don't have them on the 32u4 feather...
  musicPlayer.useInterrupt(VS1053_FILEPLAYER_TIMER0_INT); // timer int

  // Play last played track if available or start first playlist
  int initialPlaylist;
  int initialTrack;
  if (readEEPROM(initialPlaylist, initialTrack)) {
    playTrack(initialPlaylist, initialTrack);
  } else {
    playTrack(0);
  }
}

void loop() {
#if DEBUG
  handleSerial();
#endif
  handleButtons();
  
  static int counter;
  if (++counter % 100 == 0) {
    handleVolume();
  }

  handleNextTrack();
}







