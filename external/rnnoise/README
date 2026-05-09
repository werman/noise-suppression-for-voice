RNNoise is a noise suppression library based on a recurrent neural network.
A description of the algorithm is provided in the following paper:

J.-M. Valin, A Hybrid DSP/Deep Learning Approach to Real-Time Full-Band Speech
Enhancement, Proceedings of IEEE Multimedia Signal Processing (MMSP) Workshop,
arXiv:1709.08243, 2018.
https://arxiv.org/pdf/1709.08243.pdf

An interactive demo of version 0.1 is available at: https://jmvalin.ca/demo/rnnoise/

To compile, just type:
% ./autogen.sh
% ./configure
% make

Optionally:
% make install

It is recommended to either set -march= in the CFLAGS to an architecture
with AVX2 support or to add --enable-x86-rtcd to the configure script
so that AVX2 (or SSE4.1) can at least be used as an option.
Note that the autogen.sh script will automatically download the model files
from the Xiph.Org servers, since those are too large to put in Git.

While it is meant to be used as a library, a simple command-line tool is
provided as an example. It operates on RAW 16-bit (machine endian) mono
PCM files sampled at 48 kHz. It can be used as:

% ./examples/rnnoise_demo <noisy speech> <output denoised>

The output is also a 16-bit raw PCM file.
NOTE AGAIN, THE INPUT and OUTPUT ARE IN RAW FORMAT, NOT WAV.

The latest version of the source is available from
https://gitlab.xiph.org/xiph/rnnoise .  The GitHub repository
is a convenience copy.

== Training ==

The models distributed with RNNoise are now trained using only the publicly
available datasets listed below and using the training precedure described
here. Exact results will still depend on the the exact mix of data used,
on how long the training is performed and on the various random seeds involved.

To train an RNNoise model, you need both clean speech data, and noise data.
Both need to be sampled at 48 kHz, in 16-bit PCM format (machine endian).
Clean speech data can be obtained from the datasets listed in the datasets.txt
file, or by downloaded the already-concatenation of those files in
https://media.xiph.org/rnnoise/data/tts_speech_48k.sw
For noise data, we suggest the background_noise.sw and foreground_noise.sw
(or later versions) noise files from https://media.xiph.org/rnnoise/data/
The foreground_noise.sw file contains noise signals that are meant to be added
to the background noise (e.g. keyboard sounds). Optionally, the foreground noise
file can even be denoised with a traditional denoiser (e.g. libspeexdsp) to
keep only the transient components. For background noise, the data from the
original RNNoise noise collection have now been sufficiently filtered to
provide good results -- either alone or in combination with the
background_noise.sw file. The dataset can be downloaded (updated Jan 30th 2025)
from: https://media.xiph.org/rnnoise/rnnoise_contributions.tar.gz

The first step is to take the speech and noise, and mix them in a variety of
ways to simulate real life conditions (including pauses, filtering and more).
Assuming the files are called speech.pcm and noise.pcm, start by generating
the training feature data with:

% ./dump_features speech.pcm background_noise.pcm foreground_noise.pcm features.f32 <count>
where <count> is the number of sequences to process. The number of sequences
should be at least 10000, but the more the better (200000 or more is
recommended).

Optionally, training can also simulate reverberation, in which case room impulse
responses (RIR) are also needed. Limited RIR data is available at:
https://media.xiph.org/rnnoise/data/measured_rirs-v2.tar.gz
The format for those is raw 32-bit floating-point (files are little endian).
Assuming a list of all the RIR files is contained in a rir_list.txt file,
the training feature data can be generated with:

% ./dump_features -rir_list rir_list.txt speech.pcm background_noise.pcm foreground_noise.pcm features.f32 <count>

To make the feature generation faster, you can use the script provided in
script/dump_features_parallel.sh (you will need to modify the script if you
want to add RIR augmentation).

To use it:
% script/dump_features_parallel.sh ./dump_features speech.pcm background_noise.pcm foreground_noise.pcm features.f32 <count> rir_list.txt
which will run nb_processes processes, each for count sequences, and
concatenate the output to a single file.

Once the feature file is computed, you can start the training with:
% python3 train_rnnoise.py features.f32 output_directory

Choose a number of epochs (using --epochs) that leads to about 75000 weight
updates. The training will produce .pth files, e.g. rnnoise_50.pth .
The next step is to convert the model to C files using:

% python3 dump_rnnoise_weights.py --quantize rnnoise_50.pth rnnoise_c

which will produce the rnnoise_data.c and rnnoise_data.h files in the
rnnoise_c directory.

Copy these files to src/ and then build RNNoise using the instructions above.

For slightly better results, a trained model can be used to remove any noise
from the "clean" training speech, before restaring the denoising process
again (no need to do that more than once).

== Loadable Models ==

The model format has changed since v0.1.1. Models now use a binary
"machine endian" format. To output a model in that format, build RNNoise
with that model and use the dump_weights_blob executable to output a
weights_blob.bin binary file. That file can then be used with the
rnnoise_model_from_file() API call. Note that the model object MUST NOT
be deleted while the RNNoise state is active and the file MUST NOT
be closed.

To avoid including the default model in the build (e.g. to reduce download
size) and rely only on model loading, add -DUSE_WEIGHTS_FILE to the CFLAGS.
To be able to load different models, the model size (and header file) needs
to patch the size use during build. Otherwise the model will not load
We provide a "little" model with half as an alternative. To use the smaller
model, rename rnnoise_data_little.c to rnnoise_data.c. It is possible
to build both the regular and little binary weights and load any of them
at run time since the little model has the same size as the regular one
(except for the increased sparsity).
