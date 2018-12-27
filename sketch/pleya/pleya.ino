// pleya - Kids mp3 player
// version 0.2
// written by Simon Kallweit 2017-2018

// Set this to 1 to enable debug mode
#define DEBUG 0

// Maximum volume (0-127)
static const int maxVolume = 127;

// Number of playlists (maximum is 9)
static const int playlistCount = 6;

// Pin connected to a variable voltage divider used for volume control
static const int volumePin = 18; // A0

// Pins connected to buttons used for playback control
static const int playlistPins[playlistCount] = { 13, 21, 19, 12, 22, 20 };
static const int backwardPin = 11;
static const int forwardPin = 23;

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

// Debugging
#if DEBUG
  #define dbg_print(_x_) Serial.print(_x_)
  #define dbg_println(_x_) Serial.println(_x_)
#else
  #define dbg_print(_x_)
  #define dbg_println(_x_)
#endif

// Copy a string and make sure dst is null-terminated
void strcopy(char *dst, const char *src, int length) {
  while (*src != '\0' && length > 1) {
    *dst++ = *src++;
    --length;
  }
  *dst = '\0';
}

// Button debouncer
class Button {
public:
  void init(int pin) {
    _pin = pin;
    pinMode(_pin, INPUT_PULLUP);
  }

  // Returns true if button was pressed
  bool pressed(bool repeats = false) {
    bool state = !digitalRead(_pin);
    if (state != _state) {
      _state = state;
      _time = millis();
    } else {
      if (_debouncedState != _state) {
        if ((millis() - _time) >= DEBOUNCE) {
          _debouncedState = _state;
          if (_debouncedState) {
            return true;
          }
        }
      }
    }
    return false;
  }

private:
  static const int DEBOUNCE = 100;
  int _pin;
  bool _state = false;
  bool _debouncedState = false;
  unsigned long _time;
};

// Sorted list of tracks in a playlist
class TrackList {
public:
  int numTracks() const { return _numTracks; }

  // Open a playlist and populate sorted track list
  void open(int playlist) {
    _playlist = playlist;
    _numTracks = 0;

    char path[3] = { '/', '0' + _playlist, '\0' };
    File dir = SD.open(path);
    dir.rewindDirectory();
    if (dir && dir.isDirectory()) {
      while (_numTracks < MAXTRACKS) {
        File file = dir.openNextFile();
        if (!file) {
          break;
        }
        // Skip directories and small files that are artifacts from long file names
        if (!file.isDirectory() && file.size() > 4096) {
          dbg_println(file.name());
          strcopy(_tracks[_numTracks++], file.name(), MAXFILENAME);
        }
        file.close();
      }
    }
    dir.close();

    sort();
  }

  // Get absolute track filename
  bool trackFilename(int index, char *filename, int length) {
    if (index < 0 || index >= _numTracks || length < 3) {
      return false;
    }
    filename[0] = '0' + _playlist;
    filename[1] = '/';
    filename[2] = '\0';
    strncat(filename, _tracks[index], length);
    return true;
  }

  void dump() {
    dbg_println("Tracklist:");
    dbg_println("------------");
    for (int i = 0; i < _numTracks; ++i) {
      dbg_println(_tracks[i]);
    }
    dbg_println("------------");
  }

private:
  void sort() {
    for (int i = 1; i < _numTracks; i++) {
      for (int j = i; j > 0 && strcmp(_tracks[j-1], _tracks[j]) > 0; j--) {
        char tmp[MAXFILENAME];
        strcopy(tmp, _tracks[j-1], MAXFILENAME);
        strcopy(_tracks[j-1], _tracks[j], MAXFILENAME);
        strcopy(_tracks[j], tmp, MAXFILENAME);
      }
    }
  }

  static const int MAXTRACKS = 50;
  static const int MAXFILENAME = 13;

  int _playlist = -1;
  char _tracks[MAXTRACKS][MAXFILENAME];
  int _numTracks = 0;
};

// Buttons
static Button playlistButtons[playlistCount];
static Button backwardButton;
static Button forwardButton;

static TrackList trackList;

int currentPlaylist = -1;
int currentTrack = -1;
bool isPlaying = false;

unsigned long lastVolumeUpdate = 0;
unsigned long lastPlayStateWrite = 0;

Adafruit_VS1053_FilePlayer musicPlayer =
  Adafruit_VS1053_FilePlayer(VS1053_RESET, VS1053_CS, VS1053_DCS, VS1053_DREQ, CARDCS);

static const char *playStateFilename = "/STATE.DAT";
static const char playStateHeader[4] = "PLST";

// Write playlist/track to SD card
bool writePlayState(int playlist, int track, unsigned long position) {
  dbg_print("writePlayState("); dbg_print(playlist); dbg_print(", "); dbg_print(track); dbg_print(", "); dbg_print(position); dbg_println(")");

  File file = SD.open(playStateFilename, FILE_WRITE);
  if (!file) {
    return false;
  }

  file.seek(0);
  file.write(playStateHeader, sizeof(playStateHeader));
  file.write((const uint8_t *)(&playlist), sizeof(playlist));
  file.write((const uint8_t *)(&track), sizeof(track));
  file.write((const uint8_t *)(&position), sizeof(position));
  file.close();

  return true;
}

// Read playlist/track from SD card
bool readPlayState(int &playlist, int &track, unsigned long &position) {
  dbg_println("readPlayState()");

  File file = SD.open(playStateFilename);
  if (!file) {
    return false;
  }

  char header[sizeof(playStateHeader)];
  file.read(header, sizeof(header));
  if (memcmp(header, playStateHeader, sizeof(header)) != 0) {
    file.close();
    return false;
  }

  file.read((uint8_t *)(&playlist), sizeof(playlist));
  file.read((uint8_t *)(&track), sizeof(track));
  file.read((uint8_t *)(&position), sizeof(position));
  file.close();

  return true;
}

void stopPlaying() {
  dbg_println("stopPlaying()");

  if (isPlaying) {
    isPlaying = false;
    musicPlayer.stopPlaying();
    while (!musicPlayer.stopped()) {
      delay(1);
    }
  }
}

// Play next track in playlist or the specified track if track != -1
void playTrack(int playlist, int track = -1, unsigned long position = 0) {
  dbg_print("playTrack("); dbg_print(playlist); dbg_print(", "); dbg_print(track); dbg_println(")");

  stopPlaying();

  if (playlist != currentPlaylist) {
    trackList.open(playlist);
    trackList.dump();
    currentPlaylist = playlist;
    currentTrack = -1;
  }

  if (track >= 0 && track < trackList.numTracks()) {
    currentTrack = track;
  } else if (trackList.numTracks() > 0) {
    currentTrack = (currentTrack + 1) % trackList.numTracks();
  } else {
    currentTrack = -1;
  }

  if (currentTrack == -1) {
    return;
  }

  char trackName[64];
  trackList.trackFilename(currentTrack, trackName, sizeof(trackName));

  dbg_print("track index "); dbg_println(currentTrack);
  dbg_print("track filename "); dbg_println(trackName);

  isPlaying = true;
  if (!musicPlayer.startPlayingFile(trackName)) {
    dbg_print("failed to start playing "); dbg_println(trackName);
    isPlaying = false;
  }
  if (isPlaying && position > 0) {
    musicPlayer.fileSeek(position);
  }

  writePlayState(currentPlaylist, currentTrack, position);
}

// Go backward in current track
void backward() {
  dbg_println("backward()");

  playTrack(currentPlaylist, currentTrack);
}

// Go forward in current track
void forward() {
  dbg_println("forward()");
  dbg_print("file size: "); dbg_println(musicPlayer.fileSize());
  dbg_print("file position: "); dbg_println(musicPlayer.filePosition());
  dbg_print("file progress: "); dbg_println((100L * musicPlayer.filePosition()) / musicPlayer.fileSize());

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

static long volumeFiltered = 0;

void handleVolume() {
  // Read volume pot (0..1023)
  long volumeValue = analogRead(volumePin);
  // Map to 0..maxVolume
  long volume = (volumeValue * maxVolume) / 1000;
  volume = volume > maxVolume ? maxVolume : volume;
  volume = 127 - volume;
  // Lowpass filter to prevent volume jittering
  volumeFiltered = (volumeFiltered * 9 + volume) / 10;
  // Set player volume, turn off when really low
  int volumePlayer = volumeFiltered >= 117 ? 255 : volumeFiltered;
  musicPlayer.setVolume(volumePlayer, volumePlayer);
}

void handleNextTrack() {
  // Go to next track if current track finished
  if (isPlaying && musicPlayer.stopped()) {
    dbg_println("track done playing");
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
#endif
  dbg_println("\n\npleya - Kids mp3 player");

  if (!musicPlayer.begin()) {
     dbg_println("VS1053 not available!");
     while (1);
  }

  musicPlayer.setVolume(20, 20);

  if (!SD.begin(CARDCS)) {
    dbg_println("SD card not found!");
    musicPlayer.sineTest(0x44, 500);
    while (1);
  }

  // Play last played track if available or start first playlist
  int initialPlaylist;
  int initialTrack;
  unsigned long initialPosition;
  if (readPlayState(initialPlaylist, initialTrack, initialPosition)) {
    playTrack(initialPlaylist, initialTrack, initialPosition);
  } else {
    playTrack(0);
  }
}

void loop() {
#if DEBUG
  handleSerial();
#endif
  handleButtons();

  unsigned long currentTime = millis();

  // Update volume
  if (currentTime - lastVolumeUpdate > 10) {
    lastVolumeUpdate = currentTime;
    handleVolume();
  }

  // Feed data to the music player
  if (musicPlayer.readyForData()) {
    musicPlayer.feedBuffer();
    delay(5);
    // Write state every 10 seconds but just after filling play buffer to avoid underrun
    if (currentTime - lastPlayStateWrite > 10000) {
      lastPlayStateWrite = currentTime;
      if (isPlaying) {
        writePlayState(currentPlaylist, currentTrack, musicPlayer.filePosition());
      }
    }
  }

  handleNextTrack();
}

