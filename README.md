<h1 align="center" style="line-height:0;">Real-time Noise Suppression Plugin</h1>
<h2 align="center" >VST2, VST3, LV2, LADSPA, AU, AUv3</h2>

<div align="center">

<a href="">[![Licence][licence]][licence-url]</a>
<a href="">[![Latest][version]][version-url]</a>

</div>

[licence]: https://img.shields.io/badge/License-GPLv3-blue.svg
[licence-url]: https://www.gnu.org/licenses/gpl-3.0
[version]: https://img.shields.io/github/v/release/werman/noise-suppression-for-voice?label=Latest&style=flat
[version-url]: https://github.com/werman/noise-suppression-for-voice/releases

A real-time noise suppression plugin for voice based on [Xiph's RNNoise](https://github.com/xiph/rnnoise). [More info about the base library](https://people.xiph.org/~jm/demo/rnnoise/).

The plugin is meant to suppress a wide range of noise origins ([from original paper](https://arxiv.org/pdf/1709.08243.pdf)): computer fans, office, crowd, airplane, car, train, construction. 

From my tests mild background noise is always suppressed, loud sounds, like clicking of mechanical keyboard, are suppressed while there is no voice however they are only reduced in volume when voice is present. 

Please note that this plugin could not improve the voice quality with bad microphone, it even could make things worse by misclassifying the voice as a noise which would reduce already not-so-good voice quality.  

The plugin works with one or more channels, 16 bit, 48000 Hz audio input.

:exclamation: :exclamation: :exclamation: Do NOT use any other sample rates, use ONLY 48000 Hz, make sure your audio source is 48000 Hz and force it to be 48000 Hz if it is not.

There is a minimalistic GUI with all parameters and diagnostic stats:

<div align="center">
    <img src="https://i.imgur.com/xPkoqlU.png" alt="GUI of the plugin">
</div>

## Releases

[Latest releases](https://github.com/werman/noise-suppression-for-voice/releases)

## How-to

### Plugin Settings

- `VAD Threshold (%)` - if probability of sound being a voice is lower than this threshold - it will be silenced.
  In most cases the threshold between 85% - 95% would be fine.
  Without the VAD some loud noises may still be a bit audible when there is no voice.
- `VAD Grace Period (ms)` - for how long after the last voice detection the output won't be silenced. This helps when ends of words/sentences are being cut off.
- `Retroactive VAD Grace Period (ms)` - similar to `VAD Grace Period (ms)` but for starts of words/sentences. :warning: This introduces latency!

### Windows + Equalizer APO (VST2)

To check or change mic settings go to "Recording devices" -> "Recording" -> "Properties" of the target mic -> "Advanced".

To enable the plugin in Equalizer APO select "Plugins" -> "VST Plugin" and specify the plugin dll.

See [detailed guide](https://medium.com/@bssankaran/free-and-open-source-software-noise-cancelling-for-working-from-home-edb1b4e9764e) provided by  [@bssankaran](https://github.com/bssankaran).

- v1.0: Now there is a GUI, so it became easy to change parameters. 

### Linux

#### PipeWire

Since version `0.3.45` PipeWire uses [Split-File Configuration](https://gitlab.freedesktop.org/pipewire/pipewire/-/wikis/Config-PipeWire#split-file-configuration), making it extremely easy to set up plugins and tweak configuration.

For older PipeWire version you'd have to copy `/usr/share/pipewire/pipewire.conf` into `~/.config/pipewire/pipewire.conf` and then append the configuration below to already existing `context.modules`.

For PipeWire >= `0.3.45` you should:

- Create config directory: `~/.config/pipewire/pipewire.conf.d/`
- Create config for plugin: `~/.config/pipewire/pipewire.conf.d/99-input-denoising.conf`
- Paste configuration:
```
context.modules = [
{   name = libpipewire-module-filter-chain
    args = {
        node.description =  "Noise Canceling source"
        media.name =  "Noise Canceling source"
        filter.graph = {
            nodes = [
                {
                    type = ladspa
                    name = rnnoise
                    plugin = /path/to/librnnoise_ladspa.so
                    label = noise_suppressor_mono
                    control = {
                        "VAD Threshold (%)" = 50.0
                        "VAD Grace Period (ms)" = 200
                        "Retroactive VAD Grace (ms)" = 0
                    }
                }
            ]
        }
        capture.props = {
            node.name =  "capture.rnnoise_source"
            node.passive = true
            audio.rate = 48000
        }
        playback.props = {
            node.name =  "rnnoise_source"
            media.class = Audio/Source
            audio.rate = 48000
        }
    }
}
]
```

- Change `/path/to/librnnoise_ladspa.so` to actual library path
- If you are **absolutely** sure that you need stereo output - change `noise_suppressor_mono` -> `noise_suppressor_stereo`. Even if your mic says that it is stereo - you probably don't need stereo output. It also would consume 2x resources.
- Configure plugin parameters: `VAD Threshold (%)`, ...
- Restart PipeWire: `systemctl restart --user pipewire.service`
- Now you should be able to select `Noise Canceling source` as input device

For more information consult PipeWire documentation on [Filter-Chains](https://docs.pipewire.org/page_module_filter_chain.html)

Troubleshooting:
- TODO, how to change sample rate for mic.

Alternative solutions for PipeWire/PulseAudio configuration which also use RNNoise:
- [EasyEffects](https://github.com/wwmm/easyeffects) - a general solution for audio effects GUI for PipeWire. Easy to set up and use. Fewer settings for denoising. Available on Flathub.
- [NoiseTorch](https://github.com/noisetorch/NoiseTorch) - easy to set up, works with PulseAudio and Pipewire. Fewer settings for denoising.

#### PulseAudio

TLDR: Use PipeWire... or follow the instructions below.

<details>
<summary>Instructions (click me)</summary>

The idea is:

- Create a sink from which apps will take audio later and which will be the end sink in the chain.
- Load the plugin which outputs to already created sink (`sink_master` parameter) and has input sink (`sink_name` parameter, sink will be created).
- Create loopback from microphone (`source`) to input sink of plugin (`sink`) with 1 channel.

For example, to create a new mono device with noise-reduced audio from your microphone, first, find your mic name using e.g.:
```sh
pactl list sources short
```

Then, create the new device using:
```sh
pacmd load-module module-null-sink sink_name=mic_denoised_out rate=48000
pacmd load-module module-ladspa-sink sink_name=mic_raw_in sink_master=mic_denoised_out label=noise_suppressor_mono plugin=/path/to/librnnoise_ladspa.so control=50,20,0,0,0
pacmd load-module module-loopback source=<your_mic_name> sink=mic_raw_in channels=1 source_dont_move=true sink_dont_move=true
```

This needs to be executed every time PulseAudio is launched.
You can automate this by creating file in `~/.config/pulse/default.pa` with the content:

```
.include /etc/pulse/default.pa

load-module module-null-sink sink_name=mic_denoised_out rate=48000
load-module module-ladspa-sink sink_name=mic_raw_in sink_master=mic_denoised_out label=noise_suppressor_mono plugin=/path/to/librnnoise_ladspa.so control=50,200,0,0,0
load-module module-loopback source=your_mic_name sink=mic_raw_in channels=1 source_dont_move=true sink_dont_move=true

set-default-source mic_denoised_out.monitor
```

The order of settings in `control=50,200,0,0,0` is: `VAD Threshold (%)`, `VAD Grace Period (ms)`, `Retroactive VAD Grace Period (ms)`, `Placeholder1`, `Placeholder2`.

If you are absolutely sure that you want a stereo input use these options instead:

- `label=noise_suppressor_stereo`
- `channels=2`

If you have problems with audio crackling or high / periodically increasing latency, adding `latency_msec=1` to the loopback might help:
```
load-module module-loopback source=your_mic_name sink=mic_raw_in channels=1 source_dont_move=true sink_dont_move=true latency_msec=1
```

:warning: Chrome and other Chromium based browsers will ignore monitor devices and you will not be able to select the "Monitor of Null Output".
To work around this, either use pavucontrol to assign the input to Chrome, or remap this device in PulseAudio to create a regular source:

```sh
pacmd load-module module-remap-source source_name=denoised master=mic_denoised_out.monitor channels=1
```

You may still need to set correct input for application, this can be done in audio mixer panel (if you have one) in 'Recording' tab where you should set 'Monitor of Null Output' as source.

Further reading:

- Useful detailed info about PulseAudio logic [toadjaune/pulseaudio-config](https://github.com/toadjaune/pulseaudio-config).
- The [thread](https://bugs.freedesktop.org/show_bug.cgi?id=101043) which helped me with how to post-process mic output and make it available to applications.

</details>

### MacOS

TODO, contributions are welcomed!

## Status

The plugin is tested with:
- Equalizer APO v1.2 x64 (open source system-wide equalizer for Windows)
- PipeWire on Arch Linux
- Carla (on Linux)
- Audacity (on Linux)

I'm not associated with the original RNNoise work and do NOT have any understanding of recurrent neural networks it is based upon.

## Contributing

External dependencies are vendored via [git-subrepo](https://github.com/ingydotnet/git-subrepo). So that there is no need to use submodules, and patching subrepos is easy (at the moment we have several patches for JUCE).

Improvements are welcomed! Though if you want to contribute anything sizeable - open an issue first.

### Compiling

Compiling for x64:
```sh
cmake -Bbuild-x64 -H. -GNinja -DCMAKE_BUILD_TYPE=Release
ninja -C build-x64
```

Compiling for x32:
```sh
cmake -D CMAKE_CXX_FLAGS=-m32 -D CMAKE_C_FLAGS=-m32 -Bbuild-x32 -H. -GNinja -DCMAKE_BUILD_TYPE=Release
ninja -C build-x32
```

Cross-compiling for Windows x64 (MinGW builds are failing at the moment due to certain incompatibilities in JUCE):
```sh
cmake -Bbuild-mingw64 -H. -GNinja -DCMAKE_TOOLCHAIN_FILE=toolchains/toolchain-mingw64.cmake -DCMAKE_BUILD_TYPE=Release
ninja -C build-mingw64
```

#### Compiling only selected plugins

By default, all plugins supported for a platform are being built.
You can deliberately turn off plugins with the following CMake flags:

- `BUILD_LADSPA_PLUGIN`
- `BUILD_VST_PLUGIN`
- `BUILD_VST3_PLUGIN`
- `BUILD_LV2_PLUGIN`
- `BUILD_AU_PLUGIN` (macOS only)
- `BUILD_AUV3_PLUGIN` (macOS only)

For example:

```sh
cmake -DBUILD_VST_PLUGIN=OFF -DBUILD_LV2_PLUGIN=OFF
```

## License

This project is licensed under the GNU General Public License v3.0 - see the LICENSE file for details.

Used libraries:
- [JUCE](https://github.com/juce-framework/JUCE) is used under GPLv3 license
- [FST](https://git.iem.at/zmoelnig/FST/) - GPLv3
- [catch2](https://github.com/catchorg/Catch2) - BSL-1.0
