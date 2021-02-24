# Real-time Noise Suppression Plugin (VST2, LV2, LADSPA)

A real-time noise suppression plugin for voice based on [Xiph's RNNoise](https://github.com/xiph/rnnoise). [More info about the base library](https://people.xiph.org/~jm/demo/rnnoise/).

## About

The plugin is meant to suppress a wide range of noise origins ([from original paper](https://arxiv.org/pdf/1709.08243.pdf)): computer fans, office, crowd, airplane, car, train, construction. 

From my tests mild background noise is always suppressed, loud sounds, like clicking of mechanical keyboard, are suppressed while there is no voice however they are only reduced in volume when voice is present. 

The plugin is made to work with 1 channel and/or 2 channels (ladspa plugin), 16 bit, 48000 Hz audio input. Other sample rates may work, or not...

## How-to

### Windows + Equalizer APO (VST2)

To check or change mic settings go to "Recording devices" -> "Recording" -> "Properties" of the target mic -> "Advanced".

To enable the plugin in Equalizer APO select "Plugins" -> "VST Plugin" and specify the plugin dll.

See [detailed guide](https://medium.com/@bssankaran/free-and-open-source-software-noise-cancelling-for-working-from-home-edb1b4e9764e) provided by  [@bssankaran](https://github.com/bssankaran).

### Linux

#### PulseAudio

The idea is:

- Create a sink from which apps will take audio later and which will be the end think in the chain.
- Load the plugin which outputs to already created sink (`sink_master` parameter) and has input sink (`sink_name` parameter, sink will be created).
- Create loopback from microphone (`source`) to input sink of plugin (`sink`) with 1 channel.

For example, to create a new mono device with noise-reduced audio from your microphone, first, find your mic name using e.g.:
```sh
pactl list sources short
```

Then, create the new device using:
```sh
pacmd load-module module-null-sink sink_name=mic_denoised_out rate=48000
pacmd load-module module-ladspa-sink sink_name=mic_raw_in sink_master=mic_denoised_out label=noise_suppressor_mono plugin=/path/to/librnnoise_ladspa.so control=50
pacmd load-module module-loopback source=<your_mic_name> sink=mic_raw_in channels=1 source_dont_move=true sink_dont_move=true
```

This needs to be executed every time PulseAudio is launched.
You can automate this by creating file in `~/.config/pulse/default.pa` with the content:

```
.include /etc/pulse/default.pa

load-module module-null-sink sink_name=mic_denoised_out rate=48000
load-module module-ladspa-sink sink_name=mic_raw_in sink_master=mic_denoised_out label=noise_suppressor_mono plugin=/path/to/librnnoise_ladspa.so control=50
load-module module-loopback source=your_mic_name sink=mic_raw_in channels=1 source_dont_move=true sink_dont_move=true

set-default-source mic_denoised_out.monitor
```

If you have a stereo input use these options instead:

- `label=noise_suppressor_stereo`
- `channels=2`

:warning: Chrome and other Chromium based browsers will ignore monitor devices and you will not be able to select the "Monitor of Null Output".
To work around this, either use pavucontrol to assign the input to Chrome, or remap this device in PulseAudio to create a regular source:

```sh
pacmd load-module module-remap-source source_name=denoised master=mic_denoised_out.monitor channels=1
```

Additional notes:
- You can get `librnnoise_ladspa.so` either by downloading latest release or by compiling it yourself.
- You may still need to set correct input for application, this can be done in audio mixer panel (if you have one) in 'Recording' tab where you should set 'Monitor of Null Output' as source.

Plugin settings:

- VAD Threshold (%) - if probability of sound being a voice is lower than this threshold - silence will be returned.
  By default VAD threshold is 50% which should work with any mic. For most mics higher threshold `control=95` would be fine.
  Without the VAD some loud noises may still be a little bit audible when there is no voice.
- There is also an implicit grace period of 200 milliseconds, meaning that after the last voice detection - output won't be silenced for 200 ms.

Further reading:

- Useful detailed info about PulseAudio logic [toadjaune/pulseaudio-config](https://github.com/toadjaune/pulseaudio-config).
- The [thread](https://bugs.freedesktop.org/show_bug.cgi?id=101043) which helped me with how to post-process mic output and make it available to applications.

#### Ecasound

[Ecasound](https://ecasound.seul.org/ecasound/) is a software package designed for multitrack auto processing and makes it easy to chain together processing blocks. Packages are available for most distributions.

You may need to make sure that the LADSPA plugin is copied to the correct directory for your distribution. Check the plugin has been registered with:

```echo "ladspa-register" | ecasound -c
```

If the noise_suppressor_mono and noise_supressor_stereo plugins are not visible, ensure its directory is in the plugin path:

```export LADSPA_PATH=$LADSPA_PATH:/path_to_ladspa.so
```

To process a file:

```ecasound -i infile.wav -o outfile.wav -el:noise_suppressor_stereo,n
```

Where n is the VAD threshold as described above.

To process in realtime using the ASLA default input and output devices (e.g. a USB sound card):

```ecasound -i alsa -o alsa -el:noise_suppressor_stereo,n
```

A small device such as a Raspberry Pi model B can easily process a stereo signal in realtime. The plugin can be compiled on the device using the x64 instructions below.

### MacOS

You will need to compile library yourself following steps below.

It is reported that VST plugin works with Reaper after removing underscore from lib name.

## Status

The plugin is tested with Equalizer APO v1.2 x64 (open source system-wide equalizer for Windows) and tested with pulse audio on arch linux.

I'm not associated with the original work in any way and have only superficial understanding of it. The original author will probably create something better out of their work but for now I don't see any analogs with similar capabilities so I have created a usable one.

## Developing

VST sdk files aren't shipped here due to their license. You need to download VST sdk and copy several files to src/pluginterfaces/vst2.x/ and to src/vst2.x/. You can find sdk [here](https://www.steinberg.net/en/company/developers.html).

LV2 and LADSPA sdk files are in repository.

All improvements are welcomed!

### Compiling

For Windows you either need mingw or hope it works with visual studio cmake generator.

For MacOS steps are the same as for Linux.

If you did not download and place VST sdk files - VST plugin won't be built.

Compiling for x64:
```
cmake -Bbuild-x64 -H. -DCMAKE_BUILD_TYPE=Release
cd build-x64
make 
```

Compiling for x32:
```
cmake -D CMAKE_CXX_FLAGS=-m32 -D CMAKE_C_FLAGS=-m32 -Bbuild-x32 -H. -DCMAKE_BUILD_TYPE=Release
cd build-x32
make
```

Cross-compiling for Windows x64:
```
cmake -Bbuild-mingw64 -H.  -DCMAKE_TOOLCHAIN_FILE=toolchains/toolchain-mingw64.cmake -DCMAKE_BUILD_TYPE=Release
cd build-mingw64
make
```

### Turning plugins on and off

By default, all three plugins are being built if the necessary libraries are present.
You can deliberately turn off plugins by using the following CMake flags:

- `BUILD_VST_PLUGIN`
- `BUILD_LV2_PLUGIN`
- `BUILD_LADSPA_PLUGIN`

For example:

```sh
cmake -DBUILD_VST_PLUGIN=OFF
```

## License

This project is licensed under the GNU General Public License v3.0 - see the LICENSE file for details.
