# Real-time Noise Suppression Plugin (VST2, LV2, LADSPA)

A real-time noise suppression plugin for voice based on [Xiph's RNNoise](https://github.com/xiph/rnnoise). [More info about the base library](https://people.xiph.org/~jm/demo/rnnoise/).

## About

The plugin is meant to suppress a wide range of noise origins ([from original paper](https://arxiv.org/pdf/1709.08243.pdf)): computer fans, office, crowd, airplane, car, train, construction. 

From my tests mild background noise is always suppressed, loud sounds, like clicking of mechanical keyboard, are suppressed while there is no voice however they are only reduced in volume when voice is present. 

The VST plugin is made to work with 2 channels, 16 bit, 48000 Hz audio input. It seems to work also with mono channels and 44100 Hz. Other sample rates may work, or not...

## How-to

### Windows + Equalizer APO (VST2)

To check or change mic settings go to "Recording devices" -> "Recording" -> "Properties" of the target mic -> "Advanced".

To enable the plugin in Equalizer APO select "Plugins" -> "VST Plugin" and specify the plugin dll.

### Linux

#### Pulseaudio

The idea is:

- Create a sink from which apps will take audio later and which will be the end think in the chain.
- Load the plugin which outputs to already created sink ('sink_master' parameter) and has input sink ('sink_name' parameter, sink will be created). 
- Create loopback from microphone ('source') to input sink of plugin ('sink') with 1 channel.
 

```
pacmd load-module module-null-sink sink_name=mic_denoised_out  

pacmd load-module module-ladspa-sink sink_name=mic_raw_in sink_master=mic_denoised_out label=noise_suppressor plugin=librnnoise_ladspa_x64.so

pacmd load-module module-loopback source=your_mic_name sink=mic_raw_in channels=1
```

This should be executed every time pulse audio is launched. This can be done by creating file in ~/.config/pulse/default.pa with:

```
.include /etc/pulse/default.pa

load-module module-null-sink sink_name=mic_denoised_out  
load-module module-ladspa-sink sink_name=mic_raw_in sink_master=mic_denoised_out label=noise_suppressor plugin=librnnoise_ladspa_x64.so
load-module module-loopback source=your_mic_name sink=mic_raw_in channels=1

set-default-source mic_denoised_out.monitor
```

Your mic name can be found by:

```
pacmd list-sources
```

You may still need to set correct input for application, this can be done in audio mixer panel (if you have one) in 'Recording' tab where you should set 'Monitor of Null Output' as source.

Useful detailed info about pulseaudio logic [toadjaune/pulseaudio-config](https://github.com/toadjaune/pulseaudio-config).

The [thread](https://bugs.freedesktop.org/show_bug.cgi?id=101043) which helped me with how to post-process mic output and make it available to applications.

## Status

The plugin is tested with Equalizer APO v1.2 x64 (open source system-wide equalizer for Windows) and tested with pulse audio on arch linux.

I'm not associated with the original work in any way and have only superficial understanding of it. The original author will probably create something better out of their work but for now I don't see any analogs with similar capabilities so I have created a usable one.

## Developing

VST sdk files aren't shipped here due to their license. You need to download VST sdk and copy several files to src/pluginterfaces/vst2.x/ and to src/vst2.x/. You can find sdk [here](https://www.steinberg.net/en/company/developers.html).

LV2 and LADSPA sdk files are in repository.

All improvements are welcomed!

## ☑ TODO

- [X] Create LV2 plugin. (Untested)
- [X] Create LADSPA plugin.
- [X] Create correct setup with pulseaudio.
- [ ] Create package for linux distros (I don't here experience here so help is highly appreciated).
- [ ] Try to train the net with data for specific cases and see if will do better for them.

## License

This project is licensed under the GNU General Public License v3.0 - see the LICENSE file for details.