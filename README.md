# VST Noise Suppression Plugin

A real-time noise suppression VST and LV2 plugin for voice based on [Xiph's RNNoise](https://github.com/xiph/rnnoise). [More info about the base library](https://people.xiph.org/~jm/demo/rnnoise/).

## About

The plugin is meant to suppress a wide range of noise origins ([from original paper](https://arxiv.org/pdf/1709.08243.pdf)): computer fans, office, crowd, airplane, car, train, construction. 

From my tests mild background noise is always suppressed, loud sounds, like clicking of mechanical keyboard, are suppressed while there is no voice however they are only reduced in volume when voice is present. 

The plugin is made to work with 1 channel, 16 bit, 48000 Hz audio input. Other sample rates may work, or not...

## How-to

### Windows + Equalizer APO
To check or change mic settings go to "Recording devices" -> "Recording" -> "Properties" of the target mic -> "Advanced".

To enable the plugin in Equalizer APO select "Plugins" -> "VST Plugin" and specify the plugin dll.

### Linux

Testing required.

## Status

The plugin is tested with Equalizer APO v1.2 x64 (open source system-wide equalizer for Windows). It is a minimal proof-of-concept work.

I'm not associated with the original work in any way and have only superficial understanding of it. The original author will probably create something better out of their work but for now I don't see any analogs with similar capabilities so I have created a usable one.

## Developing

The plugin is built with gcc in mingw environment for x32 and x64 architectures, the current cmake project is made for such environment and may need changes in order to support other ones.

VST sdk files aren't shipped here due to their license. You need to download VST sdk and copy several files to src/pluginterfaces/vst2.x/ and to src/vst2.x/. You can find sdk [here](https://www.steinberg.net/en/company/developers.html).

LV2 sdk files are 

## ☑ TODO

- [X] Create LV2 plugin.
- [ ] Try to train the net with data for specific cases and see if will do better for them.

## License

This project is licensed under the GNU General Public License v3.0 - see the LICENSE file for details.