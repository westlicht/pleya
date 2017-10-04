# pleya - a kids mp3 player

## Introduction

This project started because I really liked the concept of [Hoerbert](http://hoerbert.com), but didn't like the price of it. A quick search for cheap and accessible hardware to implement a simple mp3 player lead me to [Adafruit's Feather](https://www.adafruit.com/feather) platform. This repo contains the Arduino project for the player and some additional information on how to build your own.

## Features

Based on the [VS1053b](https://www.sparkfun.com/datasheets/Components/SMD/vs1053.pdf) decoder chip, pleya supports the following audio codecs:

- MP1, MP2, MP3
- Ogg Vorbis
- AAC
- WMA
- RIFF WAV

Being made for the little ones, the user interface is kept very simple. The controls are:

- Power switch
- Volume knob
- Forward/backward button
- N playlist buttons

Pressing a playlist button will start playing the next track in that playlist. Forward/backward buttons allow to fast forward through the playing track and reset to it's beginning. The current playing playlist/track is saved to the EEPROM in order for pleya to automatically play the last played track when turned off and on again.

## Hardware

The hardware is based on the following Adafruit Feather Boards:

- [Adafruit Feather 32u4 Basic Proto](http://adafru.it/2771)
- [Adafruit Music Maker FeatherWing w/Amp](http://adafru.it/3436)

To complete the pleya hardware the following additional parts are required:

- Push buttons
- Potentiometer (volume control)
- Switch for power on/off
- Speaker (4 or 8 Ohm)
- 3.7V Li-Po battery
- MicroSD card

## Hardware Assembly

TBD

## Software Installation

Follow these steps to install the software on the hardware:

- Download and install the latest [Arduino software](https://www.arduino.cc/en/Main/Software)
- Install the [Adafruit_VS1053_Library](https://github.com/westlicht/Adafruit_VS1053_Library) fork
- Open `sketch/pleya/pleya.ino` in Arduino
- Adjust the following variables in `pleya.ino` in case you don't use the default hardware configuration
    - `playlistCount` to set the number of playlist buttons
    - `playlistPins` to define the digital pins connected to the playlist buttons
    - `backwardPin` and `forwardPin` to define the digital pins connected to the backward/forward buttons
- Select _Adafruit 32u4 Breakout_ in the _Tools_ / _Boards_ menu
- Connect the feather via USB
- Press _Upload_ to upload the sketch to the feather

## MicroSD Card Preparation

Follow these steps to prepare a MicroSD for pleya:

- Format the card using the FAT filesystem
- Create playlist folders `0` .. `N-1` where `N` is the number of playlists (corresponding `playlistCount` in the software)
- Copy audio files to the playlist folders
