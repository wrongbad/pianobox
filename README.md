# pianobox

A service (e.g. for RasPi) to auto record all music played in your studio.

```bash
cmake -S . -B build
cmake --build build -j

mkdir record
build/pianobox ./record PATTERN1 PATTERN2 ...

# for example if audio device is 'CP88/CP73: USB Audio'
# and midi device is 'CP88/CP73 MIDI' - you can share a pattern like this
/home/pi/pianobox/build/pianobox /media/pi/0FB6-A297 CP73 &
# on raspberry pi you add the (modified) above line to /etc/rc.local and you're ready!
```

This application will auto connect to audio and midi sources matching the specified patterns

If the devices go offline, it will search for them and reconnect when possible

When audio from a connected device exceeds a "silence" threshold, recording begins

Recording is a stereo wav file with first channel representing audio  
and second channel containing midi bytes (see below).

When audio goes silent again for a specified duration, the recording file is closed.

The midi channel of the wav uses int16 wav format. Padding samples are -1 value.
Midi packet bytes are saved 1 byte per sample (0-255) value.
The padding samples are inserted such that midi packets are accurately aligned to 
the audio stream in the first channel.

This uses the [audioplus](https://github.com/wrongbad/audioplus) library for modern c++ interfacing to audio, midi, and wav files.