REVERSE ENGINEERING the FST audio plugin API
============================================

# Part0: used software

our development system used for reverse-engineering is a
Debian GNU/Linux (amd64) box, running Debian testing/unstable.


## general software
Apart from the general system utilities (like `emacs`, `vi`, `sed`, `grep`,...)
found on any ordinary Linux system, we also use:

- Compiler: `gcc` (Debian 8.2.0-20) 8.2.0
- Debugger: `gdb` (Debian 8.2.1-1) 8.2.1
- Memory Debugger: `valgrind`
- Hexeditor: `bless`
- Diff-viewers: `meld`, `vimdiff`, `vbindiff`


## VST related
all VST-related software was accessed on 2019-02-16 unless otherwise indicated.


### Frameworks

- JUCE-5.4.1: https://d30pueezughrda.cloudfront.net/juce/juce-5.4.1-linux.zip
- JUCE-6.0.7: https://github.com/juce-framework/JUCE/releases/download/6.0.7/juce-6.0.7-linux.zip (accessed 2021-02-03)


### Plugins

- U-he Protoverb: https://u-he.com/products/protoverb/
- InstaLooper: https://www.audioblast.me/instalooper.html
- BowEcho: https://ineardisplay.com/plugins/legacy/
- Danaides: https://ineardisplay.com/plugins/legacy/
- Digits: http://www.extentofthejam.com/
- Hypercyclic: http://www.mucoder.net/en/hypercyclic/
- Tonespace: http://www.mucoder.net/en/tonespace/
- GVST: https://www.gvst.co.uk/index.htm

### Hosts

- Reaper: https://www.reaper.fm/download.php (reaper5965_linux_x86_64.tar.xz)
- MrsWatson: http://teragonaudio.com/MrsWatson.html (MrsWatson-0.9.7)

# Part1: unknown symbols

## starting
We use JUCE-5.4.1 to compile a VST2-compliant plugin host, but without VST-headers.

- edit `JUCE/extras/AudioPluginHost/AudioPluginHost.jucer` and enable VST, by
renaming the `JUCE_PLUGINHOST_VST3` variable:

~~~
JUCE_PLUGINHOST_VST="1"
~~~

- resave the Project `JUCE/Projucer -resave JUCE/extras/AudioPluginHost/AudioPluginHost.jucer`
- and try to build it:

~~~bash
cd JUCE/extras/AudioPluginHost/Builds/LinuxMakefile
make
~~~

- JUCE will complain about missing `<pluginterfaces/vst2.x/aeffect.h>` (and later
`<pluginterfaces/vst2.x/aeffectx.h>`) headers.
We will just create a single header `fst.h`, and have stub-implementations of `aeffect.h`
and `aeffectx.h` that simply include the real header.

~~~bash
mkdir FST
touch FST/fst.h
for i in aeffect.h aeffectx.h ; do
cat > $i <<EOF
#pragma once
#include "fst.h"
EOF
~~~

- finally, make JUCE use our headers:

~~~bash
ln -s $(pwd)/FST JUCE/modules/juce_audio_processors/format_types/VST3_SDK/pluginterfaces/vst2.x
~~~

## getting our first enum names

building the AudioPluginHost  gets us a lot of errors (about 501).
these errors are interesting, as this is what we want to implement in the `fst.h` headers.
luckily JUCE will attempt to put everything in the `Vst2` namespace, making it easier to find the
interesting broken things.

So lets start, by extracting all the symbols that should be part of the `Vst2` namespace (but are not):

~~~bash
make -k -C JUCE/extras/AudioPluginHost/Builds/LinuxMakefile 2>&1 \
| grep error:                                                    \
| grep "is not a member of .Vst2.$"                              \
| sed -e 's|.* error: ||' | sed -e 's|^.||' -e 's|. .*||'        \
| sort -u
~~~

This gives us the name `ERect` and symbols starting with the following prefixes:
- `audioMaster*`
- `eff*`
- `kPlug*`
- `kSpeaker*`
- `kVst*`
- `Vst*`

As a first draft, let's just add all those symbols as enumerations to our `fst.h` header:

~~~C
##define FST_UNKNOWN(x) x
enum {
    FST_UNKNOWN(audioMasterAutomate),
    FST_UNKNOWN(audioMasterBeginEdit),
    // ..
    FST_UNKNOWN(audioMasterWillReplaceOrAccumulate)
};
// ...
~~~

We wrap all the values into the `FST_UNKNOWN` macro, because we don't know their actual values yet.

We even might go as far as assign some pseudo-random value that is far off to each enum,
in order to not accidentally trigger a real enum:

~~~
FST_UNKNOWN_ENUM(x) x = 98765 + __LINE__
~~~

## some types
if we now re-compile the AudioPluginHost against our new headers, we get a lot less errors (about 198).
It's still quite a lot of errors though...

Look at the output of

~~~bash
make -k -C JUCE/extras/AudioPluginHost/Builds/LinuxMakefile 2>&1 | less
~~~

There's a number of errors that are `type` related, using these symbols:

- `AEffect`
- `VstEvent`
- `VstEvents`
- `VstPlugCategory`
- `VstSpeakerArrangement`
- `VstTimeInfo`


### VstEvent
A quick `rgrep VstEvent` on the JUCE sources leads us to `modules/juce_audio_processors/format_types/juce_VSTMidiEventList.h`, which reveals that
`VstEvent` is a structure with at least two members `type` and `byteSize`.
The later is of a type compatible with `size_t` (a `sizeof()` value is assigned to it),
the former is assigned a value of `kVstMidiType`, and compared to `kVstSysExType`. Most likely these two are in a separate enum `VstEventType` or somesuch.

There's also related types `VstMidiEvent` and `VstMidiSysexEvent` which can be case to `VstEvent`.
The sysex-type features a `sysexDump` member, that is a dynamically allocated array (most likely of some `BYTE` type).

~~~C
typedef enum {
  FST_UNKNOWN_ENUM(kVstMidiType),
  FST_UNKNOWN_ENUM(kVstSysExType),
} t_fstEventType;

##define FSTEVENT_COMMON t_fstEventType type; int byteSize
typedef struct VstEvent_ {
 FSTEVENT_COMMON;
} VstEvent;

typedef struct VstMidiEvent_ {
 FSTEVENT_COMMON;
 /* FIXXXME: unknown member order */
 FST_UNKNOWN(int) noteLength;
 FST_UNKNOWN(int) noteOffset;
 FST_UNKNOWN(int) detune;
 FST_UNKNOWN(int) noteOffVelocity;
 FST_UNKNOWN(int) deltaFrames;
 char midiData[4];
} VstMidiEvent;
typedef struct VstMidiSysexEvent_ {
 FSTEVENT_COMMON;
 /* FIXXXME: unknown member order */
 char*sysexDump;
 FST_UNKNOWN(int) dumpBytes;
 FST_UNKNOWN(int) deltaFrames;
 FST_UNKNOWN(int) flags;
 FST_UNKNOWN(int) resvd1, resvd2;
} VstMidiSysexEvent;
~~~

`VstEvent`, `VstMidiEvent` and `VstSysexEvent` are removed from our previous enum.


### VstEvents
Since `VstEvents` is supposed to be a type, let's naively set it to `int`.
This reveals, that JUCE really wants a struct with members `numEvents` (`int`)
and `events` (an array with elements to be cast to `*VstEvent` and the like)

~~~C
typedef struct VstEvents_ {
  int numEvents;
  VstEvent**events;
} VstEvents;
~~~


### VstSpeakerArrangement
Since `VstSpeakerArrangement` is supposed to be a type, let's naively set it to `int`.
This will yield errors about not being able to access members in a non-class type.

~~~bash
make -C JUCE/extras/AudioPluginHost/Builds/LinuxMakefile 2>&1 \
| grep VstSpeakerArrangement | grep "error:"                  \
| sed -e 's|.* error: request for member ||' -e 's| in .*||'  \
| sort -u
~~~

Gives us three members, `type`, `speakers`, `numChannels`.
To get the types of these members, let's set it experimentally to something stupid, like `void*`.
That reveleals that we need to be able to convert `type` to `int32` (but more likely some enumeration),
`numChannels` to `int`, and that `speakers` really is an array to `VstSpeakerProperties`.

Somewhere in `modules/juce_audio_processors/format_types/juce_VSTCommon.h`, we can see that
the `type` is populated by `int channelSetToVstArrangementType()` which really returns values like
`kSpeakerArrStereo`.

I'd rather assign the `kSpeakerArr*` enumeration a name `t_fstSpeakerArrangementType` and use that for `type`,
but alas, C++ does not allow use to implicitly cast `int` to `enum`, so we need to use `int` instead.

~~~C
typedef struct VstSpeakerArrangement_ {
  int type;
  int numChannels;
  VstSpeakerProperties speakers[];
} VstSpeakerArrangement;
~~~

#### sidenote: varsized arrays
The `VstSpeakerArrangement.speakers` could be a pointer (`VstSpeakerProperties*`) or an array (`VstSpeakerProperties[]`).
Because they are often used almost synonymously, my first attempt used a pointer.
Much later, i tried compiling and *running* JUCE's AudioPluginHost and it would spectacularily segfault
when allocating memory for `VstSpeakerArrangement` and assigning values to it.
It turned out, that using a var-sized array instead of the pointer fixes this issue.

the `type var[]` syntax is C99, for older C-implementations use `type var[0]` instead.


### VstSpeakerProperties
Resolving `VstSpeakerArrangement`, leaves us with a new type `VstSpeakerProperties`.
We play the above game again, and get:

~~~C
typedef struct VstSpeakerProperties_ {
  int type;
} VstSpeakerProperties;
~~~

This is highly unlikely: a struct with only a single member would usually be just replaced by the member itself.
So the `VstSpeakerProperties` will *most likely* have more members, of which we don't know the names nor types.

### VstPlugCategory
A value of this type is compared to `kPlugCategShell`, so we typedef the enumeration
with the `kPlug*` names to `VstPlugCategory`.

This also adds the `kPlugSurroundFx` value to this new type, if which I am not sure
yet.


### VstTimeInfo
Again playing the game by setting `VstTimeInfo` to `int`, gives us members of the struct.
So far, the types are guessed based on the values they are assigned to (If a floating-point value is assigned,
we use `double`, for integer types we use `int`):

~~~C
typedef struct VstTimeInfo_ {
  FST_UNKNOWN(double) tempo;
  FST_UNKNOWN(int) timeSigNumerator;
  FST_UNKNOWN(int) timeSigDenominator;
  FST_UNKNOWN(double) sampleRate;// = rate;
  FST_UNKNOWN(int) samplePos;
  FST_UNKNOWN(int) flags;// = Vst2::kVstNanosValid
  FST_UNKNOWN(double) nanoSeconds;

  FST_UNKNOWN(double) ppqPos; // (double)position.ppqPosition;
  FST_UNKNOWN(double) barStartPos; // (double)ppqPositionOfLastBarStart;
  FST_UNKNOWN(double) cycleStartPos; // (double)ppqLoopStart;
  FST_UNKNOWN(double) cycleEndPos; // (double)ppqLoopEnd;
  FST_UNKNOWN(int) smpteFrameRate; //int32
  FST_UNKNOWN(int) smpteOffset; //int32
} VstTimeInfo;
~~~

### AEffect
rinse & repeat.

we need a few helpers:

- `VSTCALLBACK` this seems to be only a decorator, we use `#define VSTCALLBACK` to ignore it for now
- `audioMasterCallback` ??? (that's probably a pointer to the dispatcher function)

        effect = module->moduleMain ((Vst2::audioMasterCallback) &audioMaster);

- a pointer-sized-int, for now `typedef long t_fstPtrInt;` will do


after that the biggest issue is that the `AEffect` struct contains a few function-pointers, namely
- `dispatcher`
- `getParameter` & `setParameter`
- `process`, `processReplacing` and `processDoubleReplacing`

luckily JUCE maps those functions quite directly, so we get:
~~~
t_fstPtrInt dispatcher(AEffect*, int opcode, int index, t_fstPtrInt ivalue, void* const ptr, float fvalue);
void setParameter(AEffect*, int index, float value);
float getParameter(AEffect*, int index);
void process(AEffect*, float**indata, float**outdata, int sampleframes);
void processReplacing(AEffect*, float**indata, float**outdata, int sampleframes);
void processReplacingDouble(AEffect*, double**indata, double**outdata, int sampleframes);
~~~

And we end up with something like the following:

~~~C
typedef long t_fstPtrInt; /* pointer sized int */
##define VSTCALLBACK

 /* dispatcher(effect, opcode, index, ivalue, ptr, fvalue) */
typedef t_fstPtrInt (t_fstEffectDispatcher)(struct AEffect_*, int, int, t_fstPtrInt, void* const, float);
typedef void (t_fstEffectSetParameter)(struct AEffect_*, int, float);
typedef float (t_fstEffectGetParameter)(struct AEffect_*, int);
typedef void (t_fstEffectProcess)(struct AEffect_*, float**indata, float**outdata, int sampleframes);
typedef void (t_fstEffectProcessInplace)(struct AEffect_*, float**indata, float**outdata, int sampleframes);
typedef void (t_fstEffectProcessInplaceDbl)(struct AEffect_*, double**indata, double**outdata, int sampleframes);

typedef FST_UNKNOWN(t_fstEffectDispatcher*) audioMasterCallback;

typedef struct AEffect_ {
  FST_UNKNOWN(int) magic; /* 0x56737450 */
  FST_UNKNOWN(int) uniqueID;
  FST_UNKNOWN(int) version;

  FST_UNKNOWN(void*) object; // FIXXXME

  t_fstEffectDispatcher* dispatcher; // (AEffect*, Vst2::effClose, 0, 0, 0, 0);
  t_fstEffectGetParameter* getParameter; // float(Aeffect*, int)
  t_fstEffectSetParameter* setParameter; // (Aeffect*, int, float)
  t_fstEffectProcess* process;
  t_fstEffectProcessInplace* processReplacing;
  t_fstEffectProcessInplaceDbl* processDoubleReplacing;

  FST_UNKNOWN(int) numPrograms;
  FST_UNKNOWN(int) numParams;
  FST_UNKNOWN(int) numInputs;
  FST_UNKNOWN(int) numOutputs;
  FST_UNKNOWN(int) flags;
  FST_UNKNOWN(int) initialDelay;

  FST_UNKNOWN(t_fstPtrInt) resvd1;
  FST_UNKNOWN(t_fstPtrInt) resvd2;
} AEffect;
~~~


### VstPinProperties
this is also a type rather than an enum.
play the typedef game again, and we get:

~~~C
typedef struct VstPinProperties_ {
    FST_UNKNOWN(int) arrangementType;
    char*label;
    int flags;
} VstPinProperties;
~~~


### ERect
the last remaining type we missed is `ERect`.

rinse & repeat and we have:

~~~C
typedef struct ERect_ {
  int left;
  int right;
  int top;
  int bottom;
} ERect;
~~~

## more symbols

The JUCE wrapper is not entirely symmetric, so only deriving missing
symbols from AudioPluginHost is not enough.
We also need to compile a (minimal) Audio Plugin using JUCE,
to get additional symbols.
Just creating a new *Audio Plugin* project using Projucer will
give us some additional symbols.

Apart from an additional `VstPinProperties.shortLabel`,
we still miss the following types:

~~~
AEffectDispatcherProc
AEffectSetParameterProc
AEffectGetParameterProc
AEffectProcessProc
AEffectProcessDoubleProc
~~~

These function types replace our old `t_fstEffectProcess*` types:

| OLD                            | NEW                        |
|--------------------------------|----------------------------|
| `t_fstEffectDispatcher`        | `AEffectDispatcherProc`    |
| `t_fstEffectGetParameter`      | `AEffectGetParameterProc`  |
| `t_fstEffectSetParameter`      | `AEffectSetParameterProc`  |
| `t_fstEffectProcess`           | `AEffectProcessProc`       |
| `t_fstEffectProcessInplace`    | `AEffectProcessProc`       |
| `t_fstEffectProcessInplaceDbl` | `AEffectProcessDoubleProc` |

And we also miss some constants:
~~~
kVstMaxLabelLen
kVstMaxShortLabelLen
kVstClockValid
kVstPinIsActive
kVstSmpte249fps
kVstSmpteFilm16mm
kVstSmpteFilm35mm
kVstVersion
~~~

The `kVstVersion` is a bit special, as JUCE doesn't use the `Vst2::` namespace on it.
We have to `#define` it, rather than use an `enum` for it...

## compiling MrsWatson
Another largish program we can try to compile is teregonaudio's [MrsWatson](http://teragonaudio.com/MrsWatson.html).
It seems to use some more symbols

| name                                    | notes                                           |
|-----------------------------------------|-------------------------------------------------|
| `audioMasterOpenFileSelector`           | unused                                          |
| `audioMasterCloseFileSelector`          | unused                                          |
| `audioMasterEditFile`                   | deprecated                                      |
| `audioMasterGetChunkFile`               | deprecated                                      |
| `audioMasterGetInputSpeakerArrangement` | deprecated                                      |
|-----------------------------------------|-------------------------------------------------|
| `kVstMaxProgNameLen`                    | used by `effGetProgramName`                     |
| `kVstAutomationUnsupported`             | returned by `audioMasterGetAutomationState`     |
| `kVstProcessLevelUnknown`               | returned by `audioMasterGetCurrentProcessLevel` |
| `kVstLangEnglish`                       | returned by `audioMasterGetLanguage`            |
| `kSpeakerUndefined`                     | assigned to `VstSpeakerProperties.type`         |
| `kEffectMagic`                          | `effect->magic`                                 |


Additionally *MrsWatson* uses some struct members that JUCE doesn't care about:

| struct                 | member      | type used |
|------------------------|-------------|-----------|
| `VstSpeakerProperties` | `azimuth`   | float     |
| `VstSpeakerProperties` | `elevation` | float     |
| `VstSpeakerProperties` | `radius`    | float     |
| `VstSpeakerProperties` | `reserved`  | float     |
| `VstSpeakerProperties` | `name`      | char[]    |
|------------------------|-------------|-----------|
| `VstMidiEvent`         | `flags`     | int       |
| `VstMidiEvent`         | `reserved1` | int       |
| `VstMidiEvent`         | `reserved1` | int       |
|------------------------|-------------|-----------|
| `AEffect`              | `user`      | void*     |


And there are also some additional types:

| type        | description                                                               |
|-------------|---------------------------------------------------------------------------|
| `VstInt32`  | ordinary int (32bit length?)                                              |
| `VstIntPtr` | returned by `effect->dispatcher`; used as 4th arg to `effect->dispatcher` |

`VstIntPtr` is what we have already declared as `t_fstPtrInt`.


### sidenote
I only discovered *MrsWatson* after reverse engineering a significant portion for the API.
You will notice that when reading on the discovery of `VstSpeakerProperties` and `VstMidiEvent`.

## conclusion
with the changes in place, we can now compile JUCE's AudioPluginProcessor (and *MrsWatson*)
it's still a long way to make it actually work...

# Part2: how the plugin interfaces with the host
For now, we have resolved all the names of the API to some more or less random values.
We also have discovered a few complex types (`struct`s), and while we know the names
of the struct-members and have an approximate guess of their types, we have no clue about
the order of the members, and thus the memory layout.
Since those structs are presumably used to pass data between a plugin host and a plugin,
getting them right is very important - at least if we don't want crashes.

Until know, we didn't need to know anything about the structure of the API.
We just blindly provided "reasonable" (dummy) names whenever we encountered an unknown name.

In order to reverse engineer the entire API, we should have a closer look about what we have so far.

- a lot of integer-typed values, presumably grouped in `enum`s
  - `VstPlugCategory`
  - a speaker arrangement type
  - an event type
- a few "Vst" types
  - `VstEvent`, `VstMidiEvent`, `VstSysexEvent`, `VstEvents`
  - `VstSpeakerProperties`, `VstSpeakerArrangement`
  - `VstTimeInfo`
  - `VstPinProperties`
- a generic type
  - `ERect` (just a rectangle with 4 corners)
- the somewhat special `AEffect` struct
  - it is protected with a magic number (that happens to translate to `VstP`)
  - it contains function pointer
  - the function pointer members take a pointer to an AEffect as their first argument

we can also make a few assumptions:
- some of the structs contain a `flags` field, which is presumable an integer type where bits need to be set/unset
- for simplicity of the API, most of the integer types will be `int32`.

Anyhow, the `AEffect` seems to be a central structure. Most likely it means *Audio Effect*,
which is a nice name for any kind of plugin.
So most likely this structure contains an *instance* of a plugin.
There are some specialized "class methods" (yay: OOP in C!),
like a DSP block processing `process` and friends, as well as setters and getters for plugin parameters.
Apart from that there is a generic `dispatcher` function that takes an opcode (an integer)
that describes the actual functionality.
That's a nice trick to keep an API small and stable.

Let's have a quick look at how plugins are instantiated (see the `open` method in
`JUCE/modules/juce_audio_processors/format_types/juce_VSTPluginFormat.cpp`):

- the plugin file ("dll") is opened (using `dlopen()` or the like)
- the function with the name `VSTPluginMain` (or as a fallback `main`) is obtained from the dll
- whenever we want to instantiate a new plugin (`constructEffect`) we call this function,
  and pass it a callback function as an argument.
  the plugin may use this callback the query information from the host
- the `VstPluginMain` function returns a pointer to an instance of `AEffect`
- we can use the `AEffect.dispatcher` function (which has the same signature as the callback function)
  to call most of the plugin's methods (including an `open` and `close`).
  A long list of functions callable via the `dispatcher` can be found in
  `juce_audio_plugin_client/VST/juce_VST_Wrapper.cpp`

In the `juce_VST_Wrapper.cpp` file, we can also find some more Vst2-symbols of the `eff*` category,
all of them being used as the opcode argument for the `AEffect.dispatcher`.

~~~
effEditDraw
effEditMouse
effEditSleep
effEditTop
effString2Parameter
effGetVstVersion
effGetCurrentMidiProgram
effSetTotalSampleToProcess
effGetNumMidiInputChannels
effGetNumMidiOutputChannels
~~~

We also note that the symbols matching `effFlags*` are never used as opcode specifier.
Instead they are used as bitmasks for `AEffect.flags`.

While we are at it, maybe the `audioMaster*` symbols are opcodes for the callback we pass
to the `VstPluginMain` function? Let's check:

~~~bash
rgrep "audioMaster" JUCE              \
| sed -e 's|audioMasterCallback|CB|g' \
| egrep "audioMaster[A-Z]"
~~~

Which seems to confirm our idea.
Let's also check whether we have missed some `audioMaster*` symbols:

~~~bash
rgrep "Vst2::audioMaster[A-Z]" JUCE/                   \
| sed -e 's|.*Vst2::\(audioMaster[A-Za-z0-9]*\).*|\1|' \
| sort -u
~~~

nope. all of them are already in our `fst.h` file.


## Conclusion
There is only a single symbol that needs to be shared between a plugin and a host:
the name of the main entry function into the plugin, which can be `VSTPluginMain` or `main`.

The host calls this function, and provides a callback function of type

~~~C
typedef VstIntPtr (t_fstEffectDispatcher)(AEffect*, int opcode, int, VstIntPtr, void* const, float);
~~~

The main function returns an `AEffect*` handle, which in turn has a member `dispatcher` of the same
type `t_fstEffectDispatcher`.
These two functions do the most work when communicating between host and plugin.
The opcodes for the plugin start with `eff*` (but not `effFlags*`),
the opcodes for the host start with `audioMaster*`.





# Part2.1: tricks to make our live easier

## fake values

to make our live a bit easier, we want to make sure to be able to differentiate between
real opcodes (the values we somehow detected from our reverse engineering effords) and
fake opcodes (those where we only know the name, but have no clue about their actual value).

The simplest way is to just assign values to the fake opcodes that we are pretty sure are wrong
and that most likely won't clash with *any* valid opcodes.

For starters, we put the values in some rather high range (e.g. starting at `100000`).
If the valid opcodes are grouped around 0 (like proper enums), they shouldn't interfere.
If they use magic numbers instead, they might interfere, but we'll have to deal with that once we are there.
Usually enumerations just keep incrementing their values automatically (unless overridden by the programmer),
which makes it somewhat hard to map a given fake value back to an opcode name.
A nice idea is to encode the line-number into the fake opcode value, using a little helper macro:

~~~C
#define FST_ENUM(x, y) x = y
#define FST_ENUM_UNKNOWN(x) x = 100000 + __LINE__

/* ... */
enum {
    FST_ENUM(someKnownOpcode, 42),
    FST_ENUM_UNKNOWN(someUnknownOpcode)
}
~~~

In the above example, if we ever encounter an opcode (e.g.) `100241`,
we just have to open our header-file, go to line `241` (`100241-100000`) and see what fake opcode we actually tried to send.

## dispatcher printout

We want to watch how the host calls a plugin, so we add debugging printout to the dispatcher function of our plugin:

~~~C
static VstIntPtr dispatcher(AEffect*eff, t_fstInt32 opcode, int index, VstIntPtr ivalue, void* const ptr, float fvalue) {
    printf("dispatcher(%p, %ld, %d, %ld, %p, %f);",
        effect, opcode, index, ivalue, ptr, fvalue);
    /* do the actual work */
    return 0;
}
~~~

This will give use output like:

~~~
dispatcher(0xEFFECT, 42, 0, 0, 0xPOINTER, 0.000000);
~~~

Similar on the host side.

## opcode->name mapping

Computers are good at numbers, but humans are usually better with symbols.
So instead of seeing `opcodes:42` (as defined in the example above), we would rather see `someKnownOpcode`.

We can do that with some little helper function (implemented with a helper macro):

~~~C
#define FST_UTILS__OPCODESTR(x)                   \
  case x:                                         \
  if(x>100000)                                    \
    snprintf(output, length, "%d[%s?]", x, #x);   \
  else                                            \
    snprintf(output, length, "%s[%d]", #x, x);    \
  output[length-1] = 0;                           \
  return output

static char*effCode2string(size_t opcode, char*output, size_t length) {
  switch(opcode) {
    default: break;
    FST_UTILS__OPCODESTR(effOpcodeFoo);
    FST_UTILS__OPCODESTR(effOpcodeBar);
    /* repeat for all effOpcodes */
  }
  snprintf(output, length, "%d", opcode);
  return output;
}
~~~

Now instead of printing the numeric value of an opcode, we can instead print a symbol:

~~~C
    char opcodestr[512];
    printf("dispatcher(%p, %s, %d, %ld, %p, %f);",
        effect, effCode2string(opcode, opcodestr, 512), index, ivalue, ptr, fvalue);
~~~

which will give us a much more readable:

~~~
dispatcher(0xEFFECT, someKnownOpcode[42], 0, 0, 0xPOINTER, 0.000000);
~~~

If we encounter an unknown opcode, we will notice immediately:

~~~
dispatcher(0xEFFECT, 666, 0, 0, 0xEVIL, 0.000000);
~~~



# Part3: AEffect
So let's try our code against some real plugins/hosts.
We start with some freely available plugin from a proper commercial plugin vendor,
that has been compiled for linux.
There are not many, but [u-he](https://u-he.com/) provides the `Protoverb` as binaries
for Windows, macOS and Linux (hooray!), free of charge (hooray again!).
*u-he* sell commercial plugins as well, so i figure they have a proper license from Steinberg
that allows them to do this. (I'm a bit afraid that many of the FLOSS VST2 plugins simply
violate Steinberg's terms of use. So I prefer to use plugins from commercial manufacturers for now.
Just for the record: by no means I'm trying to reverse engineer any of the plugin's trade secrets.)

Another bonus of `Protoverb` is that it is made *without* JUCE.
While JUCE is certainly a great toolkit, having plugins outside
that ecosystem will hopefully provide better coverage of edge cases...

Anyhow: we can try to load the `Protoverb` plugin in the JUCE AudioPluginHost that we
just compiled with our broken `fst.h` header.

Using <kbd>Ctrl</kbd>+<kbd>p</kbd> -> `Options` -> `Scan for new or updated VST plug-ins`,
we select the folder where we extracted the `Protoverb.64.so` file into and hit <kbd>Scan</kbd>.

Not very surprisingly it fails to load the plugin.

The error is:
>     Creating VST instance: Protoverb.64
>     *** Unhandled VST Callback: 1

This errors is printed in `juce_audio_processors/format_types/juce_VSTPluginFormat.cpp`,
when dispatching the callback opcodes.
Adding a little `printf` around the `JUCE_VST_WRAPPER_INVOKE_MAIN` it becomes clear that
the plugin is calling the callback from within the `VstPluginMain` function.
And it refuses to instantiate if we cannot handle this request.

Looking at the `audioMaster` opcodes, i only see `audioMasterVersion`
that looks like if it could be queried at the very beginning.
JUCE returns a hardcoded value of `2400` for this opcode,
and it seems like this is the VST-version (the plugin would need to make sure
that the host is expecting a compatible VST version before it instantiates itself).
Most likely the `kVstVersion` define uses the same value.

So we now have our first known enum. Yipee!

~~~C
#define FST_ENUM_EXP(x, y) x = y
// ...
   FST_ENUM_EXP(audioMasterVersion, 1),
~~~

Recompiling, and scanning for the plugin again, leads to...a crash.
But before that, we get a bit of information (obviously the plugin is quite verbose when
initializing).
The interesting parts are

>     *** Unhandled VST Callback: 33

and the crash itself, which happens *after* the plugin returned from `VstMainPlugin`.

Starting `gdb` to get a backtrace:

~~~sh
$ gdb --args JUCE/extras/AudioPluginHost/Builds/LinuxMakefile/build/AudioPluginHost
[...]
(gdb) run
[...]
Segmentation fault (core dumped)
(gdb) bt
#0  0x00007fffed6c9db2 in ?? () from /home/zmoelnig/src/iem/FST/tmp/vst/Protoverb/Protoverb.64.so
#1  0x00005555556b4d95 in juce::VSTPluginInstance::create (newModule=..., initialSampleRate=44100, initialBlockSize=512)
    at ../../../../modules/juce_audio_processors/format_types/juce_VSTPluginFormat.cpp:1146
[...]
~~~

the relevant line in `juce_VSTPluginFormat.cpp` calls

~~~
newEffect->dispatcher (newEffect, Vst2::effIdentify, 0, 0, 0, 0);
~~~

So we are accessing the `dispatcher` member of the `AEffect` struct.
Since we don't really know the memory layour of `AEffect` yet, the pointer most likely points to garbage.

Anyhow, in order to discover the layout of `AEffect` we need to examine it more closely.

For this we write a super-simplistic `FstHost` application,
that can load plugins for testing.
(I originally intended to just use the JUCE framework for this;
but we will experience lots of crashes, and will have to recompile *often*.
JUCE is rather big and it turns out that it takes just too long...)

~~~C
#include <stdio.h>
#include <dlfcn.h>
#include "fst.h"
typedef AEffect* (t_fstMain)(t_fstEffectDispatcher*);
VstIntPtr dispatcher (AEffect* effect, int opcode, int index, VstIntPtr ivalue, void*ptr, float fvalue) {
  printf("FstHost::dispatcher(%p, %d, %d, %d, %p, %f);\n", effect, opcode, index, ivalue, ptr, fvalue);
  return 0;
}
t_fstMain* load_plugin(const char* filename) {
  void*handle = dlopen(filename, RTLD_NOW | RTLD_GLOBAL);
  void*vstfun = 0;
  if(!handle)return 0;
  if(!vstfun)vstfun=dlsym(handle, "VSTPluginMain");
  if(!vstfun)vstfun=dlsym(handle, "main");
  if(!vstfun)dlclose(handle);
  printf("loaded %s as %p: %p\n", filename, handle, vstfun);
  return (t_fstMain*)vstfun;
}
int test_plugin(const char*filename) {
  t_fstMain*vstmain = load_plugin(filename);
  if(!vstmain)return printf("'%s' was not loaded\n");
  AEffect*effect = vstmain(&dispatcher);
  if(!effect)return printf("unable to instantiate plugin from '%s'\n", filename);
  return 0;
}
int main(int argc, const char*argv[]) {
  for(int i=1; i<argc; i++) test_plugin(argv[i]);
  return 0;
}
~~~

After building with `g++ -IFST FstHost.cpp -o FstHost -ldl -lrt` we can now try to open
plugins with `./FstHost path/to/plugin.so`.

Running it on a real plugin ("Protoverb.64.so") just returns
>     FstHost::dispatcher((nil), 1, 0, 0, (nil), 0.000000);
>     unable to instantiate plugin from 'tmp/vst/Protoverb/Protoverb.64.so'

So we apparently *must* handle the `audioMasterVersion` opcode in our local dispatcher
(just like JUCE does); let's extend our local dispatcher to do that:

~~~
VstIntPtr dispatcher (AEffect* effect, int opcode, int index, VstIntPtr ivalue, void*ptr, float fvalue) {
  printf("FstHost::dispatcher(%p, %d, %d, %d, %p, %f);\n", effect, opcode, index, ivalue, ptr, fvalue);
  switch(opcode) {
  case audioMasterVersion: return 2400;
  default:  break;
  }
  return 0;
}
~~~

And now we are getting somewhere!

Let's examine the `AEffect` data we get from the VSTPluginMain-function.
Setting a breakpoint in `gdb` right after the call to `vstmain`, and running
`print *effect` gives us:

~~~gdb
$1 = {magic = 1450406992, uniqueID = 0, version = -141927152, object = 0x7ffff78a5d70, dispatcher = 0x7ffff78a5d60,
  getParameter = 0x7ffff78a5d50, setParameter = 0x500000001, process = 0x200000002, processReplacing = 0x31,
  processDoubleReplacing = 0x0, numPrograms = 0, numParams = 0, numInputs = 16, numOutputs = 0, flags = 0,
  initialDelay = 1065353216, resvd1 = 93824992479632, resvd2 = 0}
~~~

This looks bad enough.
Not all looks bad though: `magic` has a value of `1450406992`, which really is `0x56737450` in hex,
which happens to be the magic number `VstP`.
The rest however is absymal: negative version numbers, unique IDs that are `0` (how unique can you get),
a function pointer (to `processReplacing`) that is `0x31` which is definitely invalid.

So let's take a closer look at the actual data.
For this, we just dump the memory where `AEffect*` points to into a file
(using `dumpdata(filename, effect, 160);` with the function below, just after calling `VSTPluginMain`)
and then use some hexeditor on it.

~~~
#include <string>
void dumpdata(const char*basename, const void*data, size_t length) {
  const char*ptr = (const char*)data;
  std::string filename = std::string(basename);
  filename+=".bin";
  FILE*f = fopen(filename.c_str(), "w");
  for(size_t i=0; i<length; i++) {
    fprintf(f, "%c", *ptr++);
  }
  fclose(f);
}
~~~

Inspecting the binary dumps with `bless`, we see that there are quite a lot of
8- (and some additional 4-) byte sequences that are always zero.
namely: bytes @04-07 (addresses in hex-notation), @40-57 and @68-6F.

~~~
00000000  50 74 73 56 00 00 00 00  10 ad 87 f7 ff 7f 00 00  |PtsV............|
00000010  70 ad 87 f7 ff 7f 00 00  60 ad 87 f7 ff 7f 00 00  |p.......`.......|
00000020  50 ad 87 f7 ff 7f 00 00  01 00 00 00 05 00 00 00  |P...............|
00000030  02 00 00 00 02 00 00 00  31 00 00 00 00 00 00 00  |........1.......|
00000040  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00000050  10 00 00 00 00 00 00 00  00 00 00 00 00 00 80 3f  |...............?|
00000060  c0 fb 58 55 55 55 00 00  00 00 00 00 00 00 00 00  |..XUUU..........|
00000070  56 50 68 75 01 00 00 00  80 ad 87 f7 ff 7f 00 00  |VPhu............|
00000080  90 ad 87 f7 ff 7f 00 00  00 00 00 00 00 00 00 00  |................|
00000090  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
~~~

If we run this repeatedly on the same plugin file, we can collect multiple dumps
(don't forget to rename them, else they will be overwritten).
Comparing those multiple dumps, we can see that many bytes stay the same, but a few
are changing for each run.

Comparing the changing values with the addresses of heap-allocated memory and functions
(e.g. the DLL-handle returned by `dlopen()` and the function pointer returned by `dlsym()`)
it becomes clear, that we have six function pointers
(e.g. `?? ad 87 f7 ff 7f 00 00` at @8, @10, @18, @20, @78, @80) and
one heap allocated object (e.g. `c0 fb 58 55 55 55 00 00` at @60).
On my amd64 system, pointers take 8 byte and aligned at 8 bytes (one halfline in the hexdump).

We don't know which functions are stored in which pointer,
but it cannot be coincidence, that our `AEffect` struct has exactly 6 function pointers
(`dispatcher`, `getParameter`, `setParameter`, `process`, `processReplacing`, `processDoubleReplacing`).
We are also a bit lucky, because we don't really know whether each function ptr must actually be filled
with a valid function (it might be allowed to set them to `0x0`).
The `Protoverb` plugin seems to implement all of them!

The heap allocated object (@60) might match the `void*object` member.

Comparing the hexdumps of multiple different plugins, we notice that @70
there are always 4 printable characters (here `VPhu`), although they are different
for each plugin.
Additionally, we notice that the `Protoverb` plugin (being pretty verbose on the stderr)
said something like:
>     setUniqueID (1969770582)

This number is `0x75685056` in hex. If we compare that to the bytes @70,
we cannot help but note that we have discovered the location of the `uniqueID` member.

If we start long enough at the remaining byte sequences, we see some other patterns
as well.
esp. at positions @28-37, there seem to be four (little endian) 4-byte integer values
(`1,5,2,2` for the *ProtoVerb*). These could map to `numPrograms`, `numParams`,
`numInputs` and `numOutputs`.
These assumption is hardened by looking at other plugins like *Digits*, *tonespace* and *hypercyclic*
that are synth-only plugins (no input) and indeed have `0` at @34-37.
I don't know how many plugins really implement *programs*,
but it doesn't sound absurd if all plugins we inspect have only a single program,
while most have numerous parameters.
So for now, let's assume that @28-37 are indeed  `numPrograms`, `numParams`, `numInputs`
and `numOutputs`, each taking 4 bytes.

The remaining non-zero bytesequences are
- @38-3f
- @5c-5f: always `00 00 80 3f`, which - weirdly enough - is the `1.f` in single-precision floating point (little endian)
- @74-77

the bits at position @38-3f are:

| plugin      | @38-3f (binary)                                                            | decimal |
|-------------|----------------------------------------------------------------------------|---------|
| ProtoEffect | `00110001 00000000 00000000 00000000  00000000 00000000 00000000 00000000` | 049 0   |
| InstaLooper | `00010001 00000000 00000000 00000000  00000000 00000000 00000000 00000000` | 017 0   |
| Digits      | `00110000 00000001 00000000 00000000  00000000 00000000 00000000 00000000` | 304 0   |
| BowEcho     | `00110001 00000010 00000000 00000000  00000000 00000000 00000000 00000000` | 561 0   |
| Danaides    | `00110001 00000010 00000000 00000000  00000000 00000000 00000000 00000000` | 561 0   |
| hypercyclic | `00110001 00000011 00000000 00000000  00000000 00000000 00000000 00000000` | 817 0   |
| tonespace   | `00110001 00000011 00000000 00000000  00000000 00000000 00000000 00000000` | 817 0   |

whereas the bits at position @74-77 are:

| plugin      | @74-77 (binary)                       | decimal |
|-------------|---------------------------------------|---------|
| ProtoEffect | `00000001 00000000 00000000 00000000` | 1       |
| InstaLooper | `00000001 00000000 00000000 00000000` | 1       |
| Digits      | `00000001 00000000 00000000 00000000` | 1       |
| BowEcho     | `01101110 00000000 00000000 00000000` | 110     |
| Danaides    | `01100110 00000000 00000000 00000000` | 102     |
| hypercyclic | `11000010 10010110 00000000 00000000` | 38594   |
| tonespace   | `11000011 10111010 00000000 00000000` | 47811   |



We still have the following fields to find:
- `flags`
- `initialDelay`
- `version`
- `resvd1`, `resvd2`

The `reserved` fields are most likely set to `0`
(all fields reserved for future use in structs, usually don't ever get used).
We know that JUCE (ab?)uses one of them to store a pointer, so they must be 8-bytes (on our 64bit system),
but we don't have any clue yet about their exact position in memory.

Searching for the `initialDelay` in the JUCE-code, it seems that this value is *in samples*,
which should make it an `int` value.
`flags` is certainly `int` (of whatever size, but the more the merrier),
and `version` most likely as well.

Comparing the values we see at positions @38-3f for various plugins seem to indicate a bitfield
(rather big changes in the decimal representations, but when looking at the binary representations
there are not so many changes at all),so for now we assume that this is the `flags` field.
Whether its 32bit or 64bit we don't really know.
However, the size of the flags field will most likely be constant across various architectures
(32bit resp. 64bit), so it probably won't be a "pointer sized int".

The `version` might be encoded in the data at @74-77. This field is (likely)
a free-form field, where the plugin can put anything (with a possible restriction that
it should be "better" (newer) plugins should sort after older ones).
The actual versioning scheme would be left to the plugin authors,
which might explain the wildly varying version numbers we have
(with *hyperbolic* and *tonespace* coming from the same author;
and *BowEcho* and *Danaides* coming from another author;
but all 4 plugins being based on JUCE (which nudges the user into setting a proper version).
Using a version number of `1` as *I don't care* is also not something unheard of...).

Collecting all the info we have so far, we end up with something like the following
(using `_pad` variables to account for unknown 0-memory).
There's also a `float1` dummy member (that always holds the value `1.f`),
and the `initialDelay` is set to the seemingly const-value at @50 (`16` in decimal,
which is a plausible value for a sample delay;
btw, loading the *Protoverb* plugin into *REAPER*,
will also print something about a 16samples delay to the stderr).

~~~C
typedef struct AEffect_ {
  t_fstInt32 magic; /* @0 0x56737450, aka 'VstP' */
  char _pad1[4]; // always 0
  AEffectDispatcherProc* dispatcher; // ???
  AEffectProcessProc* process; // ???
  AEffectGetParameterProc* getParameter; // ???
  AEffectSetParameterProc* setParameter; // ???

  t_fstInt32 numPrograms;
  t_fstInt32 numParams;
  t_fstInt32 numInputs;
  t_fstInt32 numOutputs;

  VstIntPtr flags; // size unclear
  VstIntPtr resvd1; //??
  VstIntPtr resvd2; //??
  t_fstInt32 initialDelay; //??

  char _pad2[8]; //?
  float float1; //?
  void* object; // FIXXXME
  char _pad3[8]; //??

  t_fstInt32 uniqueID; // @112
  t_fstInt32 version;

  AEffectProcessProc* processReplacing; //?
  AEffectProcessDoubleProc* processDoubleReplacing; //?

} AEffect;
~~~

If we run the same tests on 32bit plugins, we notice that we got the alignment wrong
in a number of places.

Here's a hexdump of the 1st 96 bytes of a 32bit `AEffect`:

~~~
00000000  50 74 73 56 50 91 8d f7  30 92 8d f7 f0 91 8d f7  |PtsVP...0.......|
00000010  c0 91 8d f7 01 00 00 00  05 00 00 00 02 00 00 00  |................|
00000020  02 00 00 00 31 00 00 00  00 00 00 00 00 00 00 00  |....1...........|
00000030  10 00 00 00 00 00 00 00  00 00 00 00 00 00 80 3f  |...............?|
00000040  c0 ee 56 56 00 00 00 00  56 50 68 75 01 00 00 00  |..VV....VPhu....|
00000050  80 92 8d f7 c0 92 8d f7  00 00 00 00 00 00 00 00  |................|
~~~

As we can see, there is no zero padding after the initial 4-byte `magic`.
Also the number of zeros around the `object` member seems to be off.

Using `pointer sized int` instead of `int32` helps a bit:

~~~
typedef struct AEffect_ {
  VstIntPtr magic;
  AEffectDispatcherProc* dispatcher;  //??
  AEffectProcessProc* process; //?
  AEffectGetParameterProc* getParameter; //??
  AEffectSetParameterProc* setParameter; //??

  t_fstInt32 numPrograms;
  t_fstInt32 numParams;
  t_fstInt32 numInputs;
  t_fstInt32 numOutputs;

  VstIntPtr flags;
  VstIntPtr resvd1; //??
  VstIntPtr resvd2; //??
  t_fstInt32 initialDelay; //??

  char _pad2[8]; //??
  float float1; //??
  void* object;
  VstIntPtr _pad3; //??
  t_fstInt32 uniqueID;
  t_fstInt32 version;

  AEffectProcessProc* processReplacing; //??
  AEffectProcessDoubleProc* processDoubleReplacing; //??
} AEffect;
~~~


### conf. REAPER
Printing the contents `AEffect` for the *Protoverb* plugin in gdb, gives something like:

~~~
{magic = 1450406992,
 dispatcher = 0xf78d9150,
 process = 0xf78d9230,
 getParameter = 0xf78d91f0,
 setParameter = 0xf78d91c0,
 numPrograms = 1,
 numParams = 5,
 numInputs = 2,
 numOutputs = 2,
 flags = 49,
 resvd1 = 0,
 resvd2 = 0,
 initialDelay = 16,
 _pad2 = "\000\000\000\000\000\000\000",
 myfloat = 1,
 object = 0x5656eec0,
 _pad3 = 0,
 uniqueID = 1969770582,
 version = 1,
 processReplacing = 0xf78d9280,
 processDoubleReplacing = 0xf78d92c0
}
~~~

Opening the same plugin in *REAPER* we can also learn a few things:

- 2 in, 2 out
- 1 built-in program named "initialize"
- 5 parameters
  - "main: Output" (100.)
  - "main: Active #FDN" (1.)
  - "FDN: Feedback" (50.)
  - "FDN: Dry" (100.)
  - "FDN: Wet" (30.)
- `stderr`
  - setUniqueID (1969770582)
  - HOST `REAPER` (so the plugin queries the host for the host-name)
  - Starting REVISION 4105
  - sending latency to host... 16
  - GUI: 1200 x 600
  - GUI:  640 x 575
  - CONNECTIONS_PER_PARAMETER: 8


So at least we have the number of inputs, outputs, programs and parameters right, as well as the uniquID.


## flags

We have the following flags to assign:
- `effFlagsHasEditor`
- `effFlagsIsSynth`
- `effFlagsCanDoubleReplacing`
- `effFlagsCanReplacing`
- `effFlagsNoSoundInStop`
- `effFlagsProgramChunks`

While the bitfields at @38 have the following values (displayed as little-endian):

| plugin      | flags               |
|-------------|---------------------|
| ProtoEffect | `00000000 00110001` |
| InstaLooper | `00000000 00010001` |
| Digits      | `00000001 00110000` |
| BowEcho     | `00000010 00110001` |
| Danaides    | `00000010 00110001` |
| hypercyclic | `00000011 00110001` |
| tonespace   | `00000011 00110001` |

Things we know from running the plugins through REAPER:
- all effects except *Digits* have a GUI
- *Protoverb*, *InstaLooper*, *BowEcho*, *Danaides* are all effects (with input and output)
- *Digits*, *hypercyclic*, *tonespace* are all instruments (aka synths; no input)
Things we know from looking at the source code of JUCE-5.4.1
- all JUCE plugins have `effFlagsHasEditor|effFlagsCanReplacing|effFlagsProgramChunks`
- we don't know which JUCE version was used to compile our JUCE plugins!


Comparing this with our binary flag values, we can conclude:

| flag              | value  |
|-------------------|--------|
| effFlagsHasEditor | `1<<0` |
| effFlagsIsSynth   | `1<<8` |


It's a it strange that we have `processReplacing` and `processDoubleReplacing` functions
(according to our hacked up `AEffect` definition) for each plugin,
although there are no more flags that are all set to `true` for each plugin.
This might be a problem with our `AEffect` struct or with the plugins
(or the information about replacing-functions might not be redundant.)

Another observation is that flag `1<<9` is set to `true` for all JUCE-plugins,
and to `false` for the rest.

I don't really know what either `effFlagsNoSoundInStop` nor `effFlagsProgramChunks`
actually mean.
The former is most likely related to time-based plugins (e.g. a reverb; if you pause playback
and reset to some other playback position, then you probably don't want the reverb of the
pre-pause input in your output).


## getting some opcodes
to find out the actual values of the opcodes, we just send various opcodes to the putative `dispatcher`
function of the `AEffect` and see what happens.
This is also a good test to see whether we have the address of the `dispatcher` correct.
I've added a `char[512]` as `ptr` to the dispatcher, because I've seen this in the JUCE's *juce_VSTPluginFormat.cpp*
(`fillInPluginDescription`).

~~~C
for(int i=0; i<256; i++) {
  char buffer[512] = { 0 };
  VstIntPtr res = effect->dispatcher (effect, i, 0, 0, buffer, 0);
  const char*str = (const char*)res;
  printf("%d[%d=0x%X]: %.*s\n", i, res, res, 32, str);
  if(*buffer)printf("\t'%.*s'\n", 512, buffer);
}
~~~

Running this on the `Protoverb` looper, crashes with opecode `1`.
For now we just ignore this, and don't run crashing opcodes, with something like:

~~~C
for(int i=0; i<256; i++) {
  bool skip = false;
  switch(i) {
    default: break;
    case 1:
      skip = true;
  }
  if(true)continue;
  VstIntPtr res = effect->dispatcher (effect, i, 0, 0, buffer, 0);
  //...
}
~~~

It turns out that the following opcodes `1`, `3`, `4`, `6`, `7`, `8`, `13`,
`14`, `22`, `23`, `24`, `25`, `26`, `29`, `33`, `34`, `35`, `45`, `47`, `48`,
`49`, `51`, `58`, `63`, `69`, `71`, `72` and `73` all crash
(most likely these expect a properly initialized structure at `ptr`).

Additional crashers
- JUCE-based: `3`, `42`, `44`
- Digits: `3`


The rest behaves nicely.

Just looking at our printout (ignoring the `stderr` generated by the plugin itself),
doesn't reveal very much.
The most important thing is probably, that opcode `5` gives us really something interesting in the `buffer`, namely the string
>     initialize

...which is the name of the (only) *program* as displayed in REAPER.

Since this sounds promising, we run the code on another plugin (*tonespace*), which has plenty of programs, the first
(according to REAPER) is *EDU-C Major Scale*.
Our code returns (with opcode `5`):
>     EDU-C Major Scale

Hooray, we found `effGetProgramName` or `effProgramNameIndexed`
(probably the former, as setting the `index` (and/or `ivalue` resp. `fvalue`)
doesn't make any difference.)

We also notice, that the `VstIntPtr` returned by `AEffect.dispatcher` is always `0`.


To proceed, we take a closer look at what else is printed when we run our host with some plugins.
As said before, the `Protoverb` is very verbose (on stderr),
giving us hints on what a certain opcode might be supposed to do.

E.g.

| plugin    | opcode | printout                                                                                 |
|-----------|--------|------------------------------------------------------------------------------------------|
| Protoverb | 10     | AM_AudioMan::reset()                                                                     |
| Protoverb | 12     | AM_VST_base::resume() / AM_AudioMan::reset()                                             |
| Protoverb | 15     | closed editor.                                                                           |
| Protoverb | 59     | plugin doesn't use key, returns false (VST) or jumps to default key handler (WindowProc) |
| Digits    | 11     | Block size 2.0000                                                                        |

it also calls back to the host:


| plugin      | when | opcode | notes                                                                             |
|-------------|------|--------|-----------------------------------------------------------------------------------|
| Protoverb   | main | 33     | after that, the VST-version is printed; in REAPER it also prints "HOST 'REAPER'"  |
| Protoverb   | main | 13     | right before, it prints "Protoverb VST telling unknown about 16 samples latency"  |
|             |      |        | (although no arguments are given to the callback)                                 |
| Protoverb   | 12   | 6      | just before the "resume()" comment is issued.                                     |
|-------------|------|--------|-----------------------------------------------------------------------------------|
| BowEcho     | 12   | 23     | `(..., 0,     0, NULL, 0.f)` (same for *Danaides*)                                |
| BowEcho     | 12   | 6      | `(..., 0,     1, NULL, 0.f)` (same for *Danaides*)                                |
|-------------|------|--------|-----------------------------------------------------------------------------------|
| Digits      | 12   | 6      | `(..., 0,     1, NULL, 0.f)`                                                      |
|-------------|------|--------|-----------------------------------------------------------------------------------|
| hypercyclic | 0    | 13     | `(..., 0,     0, NULL, 0.f)`                                                      |
| hypercyclic | 0    | 42     | `(..., 0,     0, NULL, 0.f)`                                                      |
| hypercyclic | 0    | 0      | `(..., 0,     i, NULL, f)` for i in range(numParams) and f=[0.f .. 1.f]           |
| hypercyclic | 0    | 13     | `(..., 0,     0, NULL, 0.f)`                                                      |
| hypercyclic | 0    | 42     | `(..., 0,     0, NULL, 0.f)`                                                      |
| hypercyclic | 0    | 0      | `(..., 0,     i, NULL, f)` for i in range(numParams) and f=[0.f .. 1.f]           |
| hypercyclic | 2    | 0      | `(..., 0,     i, NULL, f)` for i in range(numParams) and f=[0.f .. 1.f]           |
| hypercyclic | 12   | 23     | `(..., 0,     0, NULL, 0.f)`                                                      |
| hypercyclic | 12   | 7      | `(..., 0, 65024, NULL, 0.f)`                                                      |
| hypercyclic | 12   | 6      | `(..., 0,     1, NULL, 0.f)`                                                      |
|-------------|------|--------|-----------------------------------------------------------------------------------|
| tonespace   |      |        | almost the same as *hypercyclic*, but with an additional final callback           |
|             |      |        | to `FstHost::dispatcher(eff, 0, 5, 0, NULL, 0.)` whenever we iterated over params |



I guess that host-opcode `33` is meant to query the name of the host application.
In `juce_VSTPluginFormat.cpp` this is handled with
the `audioMasterGetProductString` and `audioMasterGetVendorString` opcodes,
which both return `1` and write a string into the `ptr`.
The string has a maximum length of `min(kVstMaxProductStrLen, kVstMaxVendorStrLen)`,
so these two symbols are actually constants, not enums.
We don't know the actual maximum string lengths, so I tried to compile with `-fsanitize=address -fsanitize=undefined`
and write as many bytes into the return string as possible.
This gave me a maximum string length of `197782` bytes. impressive, but i'm not sure i can trust these values...

The host-opcode `0` seems to be used to tell the host the current values for all the parameters.
In `juce_VSTPluginFormat.cpp::handleCallback()` this is handled in the `audioMasterAutomate` opcode.

## how JUCE handles opcodes
In order to understand how each opcode is used, we may look at *juce_VST_Wrapper.cpp*
to find out which parameters are used (and how) for a given opcode, and how values are returned:

### JUCE effect Opcodes

| effect opcode               |     | IN                 | OUT                     | return                | notes                       |
|-----------------------------|-----|--------------------|-------------------------|-----------------------|-----------------------------|
| effCanBeAutomated           | 26  | index              |                         | 0/1                   | can param#idx be automated? |
| effCanDo                    | 51  | ptr(char[])        |                         | 0/1/-1                |                             |
| effOpen                     | 0?  | -                  | -                       | 0                     |                             |
| effClose                    | 1?  | -                  | -                       | 0                     |                             |
| effEditClose                | 15  | -                  |                         | 0                     |                             |
| effEditGetRect              | 13  | -                  | ptr(&ERect)             | ptr                   |                             |
| effEditDraw                 |     | ptr(&ERect)        |                         |                       | JUCE-ignored                |
| effEditIdle                 |     |                    |                         |                       | JUCE-ignored                |
| effEditMouse                |     |                    |                         |                       | JUCE-ignored                |
| effEditOpen                 | 14  | ptr(Window)        |                         |                       |                             |
| effEditSleep                |     |                    |                         |                       | JUCE-ignored                |
| effEditTop                  |     |                    |                         |                       | JUCE-ignored                |
| effGetChunk                 | 23  | index              | ptr(void[])             | size                  |                             |
| effSetChunk                 | 24  | index, ivalue, ptr |                         | 0                     | ivalue=size                 |
| effGetCurrentMidiProgram    | 63? | -                  | -                       | -1                    |                             |
| effGetEffectName            | 45  | -                  | ptr(char[64])           | 1                     |                             |
| effGetInputProperties       | 33  | index              | ptr(VstPinProperties[]) | 1/0                   |                             |
| effGetOutputProperties      | 34  | index              | ptr(VstPinProperties[]) | 1/0                   |                             |
| effGetNumMidiInputChannels  |     | -                  |                         | 16/0                  | 16 if plugin handles MIDI   |
| effGetNumMidiOutputChannels |     | -                  |                         | 16/0                  | 16 if plugin handles MIDI   |
| effGetParamDisplay          | 7   | index              | ptr(char[8])            | 0                     |                             |
| effGetParamLabel            | 6   | index              | ptr(char[8])            | 0                     |                             |
| effGetParamName             | 8   | index              | ptr(char[8])            | 0                     |                             |
| effGetPlugCategory          | 35  | -                  |                         | category              |                             |
| effGetProductString         | 48  | -                  | ptr(char[64])           | 1                     |                             |
| effGetProgramNameIndexed    | 29  | index              | ptr(char[24])           | hasProgram#index      |                             |
| effGetProgramName           | 5   | -                  | ptr(char[24])           | 0                     |                             |
| effGetProgram               | 3   | -                  | -                       | current_program       |                             |
| effGetSpeakerArrangement    | 69  | -                  | ivalue([]), ptr([])     | (!(hasAUX or isMidi)) | in:(SpeakerArrangement[])   |
| effSetSpeakerArrangement    | 42  | ivalue(ptr), ptr   | -                       | 0/1                   |                             |
| effGetTailSize              |     | -                  | -                       | audiotailInSamples    |                             |
| effGetVendorString          | 47  | -                  | ptr(char[64])           | 1                     |                             |
| effGetVendorVersion         | 49  | -                  | -                       | version               |                             |
| effGetVstVersion            | 58  | -                  | -                       | kVstVersion           |                             |
| effIdentify                 | 22  | -                  | -                       | bigEndianInt("NvEf")  | 1316373862=0x4e764566       |
| effKeysRequired             |     | -                  | -                       | isKbdFocusRequired    |                             |
| effMainsChanged             | 12? | ivalue             | -                       | 0                     | ivalue?resume():suspend()   |
| effProcessEvents            | 25  | ptr(&VstEvents)    | -                       | isMidiProcessed       |                             |
| effSetBlockSize             | 11  | ivalue             | -                       | 0                     |                             |
| effSetBypass                |     | ivalue             | -                       | 0                     |                             |
| effSetProcessPrecision      | 77  | ivalue             | -                       | !isProcessing         |                             |
| effSetProgram               | 2   | ivalue             | -                       | 0                     |                             |
| effSetProgramName           | 4   | ptr(char[])        | -                       | 0                     |                             |
| effSetSampleRate            | 10  | fvalue             | -                       | 0                     |                             |
| effSetTotalSampleToProcess  | 73? | ivalue             | -                       | ivalue                |                             |
| effString2Parameter         | 27  | index, ptr(char[]) | -                       | !isLegacy(param[i])   |                             |
| effConnectInput             |     |                    |                         |                       | JUCE-ignored                |
| effConnectOutput            |     |                    |                         |                       | JUCE-ignored                |
| effIdle                     |     |                    |                         |                       | JUCE-ignored                |
| effVendorSpecific           | 50  | ???                | ???                     | ???                   |                             |
| effShellGetNextPlugin       | 70  |                    |                         |                       | JUCE-ignored                |
| effStartProcess             | 71  |                    |                         |                       | JUCE-ignored                |
| effStopProcess              | 72  |                    |                         |                       | JUCE-ignored                |

### JUCE host Opcodes

for host opcodes (`audioMaster*`) check out *juce_VSTPluginFormat.cpp*:

| host opcode                            |    | IN            | OUT         | return            | notes                                                           |
|----------------------------------------|----|---------------|-------------|-------------------|-----------------------------------------------------------------|
| audioMasterAutomate                    | 0  | index, fvalue | -           | 0                 |                                                                 |
| audioMasterProcessEvents               | 8  | ptr           | -           | 0                 | ptr=VstEvents[]                                                 |
| audioMasterGetTime                     | 7  | -             | -           | &vsttime          |                                                                 |
| audioMasterIdle                        |    | -             | -           | 0                 |                                                                 |
| audioMasterSizeWindow                  | 15 | index, value  |             | 1                 | setWindowSize(index,value)                                      |
| audioMasterUpdateDisplay               |    | -             | -           | 0                 | triggerAsyncUpdate()                                            |
| audioMasterIOChanged                   |    | -             | -           | 0                 | setLatencyDelay                                                 |
| audioMasterNeedIdle                    |    | -             | -           | 0                 | startTimer(50)                                                  |
| audioMasterGetSampleRate               | 16 | -             | -           | samplerate        |                                                                 |
| audioMasterGetBlockSize                | 17 | -             | -           | blocksize         |                                                                 |
| audioMasterWantMidi                    | 6  | -             | -           | 0                 | wantsMidi=true                                                  |
| audioMasterGetDirectory                |    | -             | -           | (char[])directory |                                                                 |
| audioMasterTempoAt                     | 10 | -             | -           | 10000*bpm         |                                                                 |
| audioMasterGetAutomationState          |    | -             | -           | 0/1/2/3/4         | 0 = not supported, 1 = off, 2 = read, 3 = write, 4 = read/write |
| audioMasterBeginEdit                   | 43 | index         | -           | 0                 | gesture                                                         |
| audioMasterEndEdit                     | 44 | index         | -           | 0                 | gesture                                                         |
| audioMasterPinConnected                |    | index,value   | -           | 0/1               | 0=true; value=direction                                         |
| audioMasterGetCurrentProcessLevel      | 23 | -             | -           | 4/0               | 4 if not realtime                                               |
|----------------------------------------|----|---------------|-------------|-------------------|-----------------------------------------------------------------|
| audioMasterCanDo                       | 37 | ptr(char[])   | -           | 1/0               | 1 if we can handle feature                                      |
| audioMasterVersion                     | 1  | -             | -           | 2400              |                                                                 |
| audioMasterCurrentId                   | 2? | -             | -           | shellUIDToCreate  | ?                                                               |
| audioMasterGetNumAutomatableParameters |    | -             | -           | 0                 |                                                                 |
| audioMasterGetVendorVersion            | 34 | -             | -           | 0x0101            |                                                                 |
| audioMasterGetVendorString             | 32 | -             | ptr(char[]) | ptr               | getHostName()                                                   |
| audioMasterGetProductString            | 33 | -             | ptr(char[]) | ptr               | getHostName()                                                   |
| audioMasterSetOutputSampleRate         |    | -             | -           | 0                 |                                                                 |
|----------------------------------------|----|---------------|-------------|-------------------|-----------------------------------------------------------------|
| audioMasterGetLanguage                 |    |               |             |                   | JUCE-ignored                                                    |
| audioMasterGetOutputSpeakerArrangement |    |               |             |                   | JUCE-ignored                                                    |
| audioMasterGetParameterQuantization    |    |               |             |                   | JUCE-ignored                                                    |
| audioMasterGetPreviousPlug             |    |               |             |                   | JUCE-ignored                                                    |
| audioMasterGetNextPlug                 |    |               |             |                   | JUCE-ignored                                                    |
| audioMasterSetTime                     |    |               |             |                   | JUCE-ignored                                                    |
| audioMasterWillReplaceOrAccumulate     |    |               |             |                   | JUCE-ignored                                                    |
| audioMasterGetInputLatency             |    |               |             |                   | JUCE-ignored                                                    |
| audioMasterGetOutputLatency            |    |               |             |                   | JUCE-ignored                                                    |
| audioMasterOpenWindow                  |    |               |             |                   | JUCE-ignored                                                    |
| audioMasterCloseWindow                 |    |               |             |                   | JUCE-ignored                                                    |
| audioMasterSetIcon                     |    |               |             |                   | JUCE-ignored                                                    |
| audioMasterOfflineGetCurrentMetaPass   |    |               |             |                   | JUCE-ignored                                                    |
| audioMasterOfflineGetCurrentPass       |    |               |             |                   | JUCE-ignored                                                    |
| audioMasterOfflineRead                 |    |               |             |                   | JUCE-ignored                                                    |
| audioMasterOfflineStart                |    |               |             |                   | JUCE-ignored                                                    |
| audioMasterOfflineWrite                |    |               |             |                   | JUCE-ignored                                                    |
| audioMasterVendorSpecific              |    |               |             |                   | JUCE-ignored                                                    |



# Part: a plugin

~~~C
/* compile with: 'g++ -shared -IFST -g -O0 FstPlugin.cpp -o FstPlugin.so' */
#include <stddef.h>
#include <cstdio>
#include "fst.h"
static AEffectDispatcherProc*dispatch = 0;
static VstIntPtr dispatcher(AEffect*eff, t_fstInt32 opcode, int index, VstIntPtr ivalue, void* const ptr, float fvalue) {
  printf("FstClient::dispatcher(%p, %d, %d, %d, %p, %f)\n", eff, opcode, index, ivalue, ptr, fvalue);
  return 0;
}

static void setParameter(AEffect*eff, int index, float value) {
  printf("FstClient::setParameter(%p)[%d] -> %f\n", eff, index, value);
}
static float getParameter(AEffect*eff, int index) {
  printf("FstClient::getParameter(%p)[%d]\n", eff, index);
  return 0.5;
}
static void process(AEffect*eff, float**indata, float**outdata, int sampleframes) {
  printf("FstClient::process(%p, %p, %p, %d\n", eff, indata, outdata, sampleframes);
}
static void processReplacing(AEffect*eff, float**indata, float**outdata, int sampleframes) {
  printf("FstClient::process'(%p, %p, %p, %d\n", eff, indata, outdata, sampleframes);
}
static void processDoubleReplacing(AEffect*eff, double**indata, double**outdata, int sampleframes) {
  printf("FstClient::process2(%p, %p, %p, %d\n", eff, indata, outdata, sampleframes);
}
extern "C"
AEffect*VSTPluginMain(AEffectDispatcherProc*dispatch4host) {
  dispatch = dispatch4host;
  printf("FstPlugin::main(%p)\n", dispatch4host);
  AEffect* eff = new AEffect;

  eff->magic = 0x56737450;
  eff->dispatcher = dispatcher;
  eff->process = process;
  eff->getParameter = getParameter;
  eff->setParameter = setParameter;

  eff->numPrograms = 1;
  eff->numParams = 3;
  eff->numInputs = 2;
  eff->numOutputs = 2;
  eff->float1 = 1.;
  eff->object = eff;
  eff->uniqueID = 123456;
  eff->version = 666;

  eff->processReplacing = processReplacing;
  eff->processDoubleReplacing = processDoubleReplacing;
  return eff;
}
~~~

This simple plugin allows us to query the host for whatever opcodes the host understands:

~~~C
for(size_t i = 0; i<64; i++) {
  char buf[512] = {0};
  VstIntPtr res = dispatch(0, i, 0, 0, buf, 0);
  if(*buf)
    printf("\t'%.*s'\n", 512, buf);
  if(res)
    printf("\treturned %d\n", res);
}
~~~

With REAPER, this returns:

| op   | result     | buf                               |
|------|------------|-----------------------------------|
| *0*  | 1          |                                   |
| *1*  | 2400       |                                   |
| 6    | 1          |                                   |
| *8*  | 1          |                                   |
| *10* | 1200000    |                                   |
| 11   | 65536      |                                   |
| 12   | 1          |                                   |
| 13   | 1          |                                   |
| 19   | 0x2800,... |                                   |
| 23   | 1          |                                   |
| *32* | 1          | Cockos                            |
| *33* | 1          | REAPER                            |
| *34* | 5965       |                                   |
| 42   | 1          |                                   |
| 43   | 1          |                                   |
| 44   | 1          |                                   |
| 48   | 1          | ~/Documents/REAPER Media/test.RPP |

This table confirms that host-opcode `33` is `audioMasterGetProductString`,
and we learn that host-opcode `32` is `audioMasterGetVendorString`.
The number `5969` happens to be the version of REAPER (`5.965`), so
host-opcode `34` is likely `audioMasterGetVendorVersion`.

The opcode `48` returns the currently opened REAPER session file.
The `audioMasterGetDirectory` might match, although REAPER returns a
file rather than a directory.
More importantly, JUCE will `return` the string address
in the `return` value, whereas REAPER writes the string into `ptr`.

Opcode `10` is a weirdo number, obviously for humans (rather than computers).
Reading up a bit on what JUCE does for the various audioMaster-opcodes,
I stumbled upong the `audioMasterTempoAt`, which - according to the comments
in the JUCE code - is meant to be the tempo in "BMP * 10000".
So the universal default of *120bpm* would be 1200000 which is exactly what
we have here.

TODO: 11

## process functions
We can also test our process functions, by adding some printout to them.

We notice that REAPER keeps calling our `process` function
(spamming the console, so we don't see much else).
Again, this is good news, as it means that we have the address of the `process` member correct.

If we apply the plugin on some track with music and printout the first sample
of the first channel (`printf("%f\n", indata[0][0]))`) we see that the samples
are nicely in the range between `-1...+1` which is what we expected.

It also confirms that REAPER is not calling some `processDouble` function.
Which is probably logical, as we haven't set either of the
`effFlagsCanReplacing` nor `effFlagsCanDoubleReplacing` flags (we don't know them yet).

If we set the various bits of `AEffect.flags` one by one (carefully leaving out #0 (editor) and #8 (synth))
and watch which of the `processReplacing` functions are being called, we soon learn new flags:

| flag                       | value   |
|----------------------------|---------|
| effFlagsCanReplacing       | `1<< 4` |
| effFlagsCanDoubleReplacing | `1<<12` |

Again, by printing out the first sample of the first channel (either as `float` or as `double`)
we learn that we already have the order correct (`processReplacing` comes before `processDoubleReplacing`
at the very end of `AEffect`.)


# AEffect initialisation from host

## startup

After the `VSTPluginMain` function has returned to the host,
REAPER calls our plugin a couple or times, with various opcodes (sorted by opcode number)):

>     FstClient::dispatcher(0x3098d20, 0, 0, 0, (nil), 0.000000)
>     FstClient::dispatcher(0x3098d20, 2, 0, 0, (nil), 0.000000)
>     FstClient::dispatcher(0x3098d20, 5, 0, 0, 0x7fffe1b4a2f0, 0.000000)
>     [...]
>     FstClient::dispatcher(0x3098d20, 10, 0, 0, (nil), 44100.000000)
>     FstClient::dispatcher(0x3098d20, 11, 0, 512, (nil), 0.000000)
>     FstClient::dispatcher(0x3098d20, 12, 0, 1, (nil), 0.000000)
>     FstClient::dispatcher(0x3098d20, 35, 0, 0, (nil), 0.000000)
>     FstClient::dispatcher(0x3098d20, 45, 0, 0, 0x7fffe1b4a530, 0.000000)
>     FstClient::dispatcher(0x3098d20, 47, 0, 0, 0x7fffe1b4a530, 0.000000)
>     FstClient::dispatcher(0x3098d20, 51, 0, 0, 0xab4617, 0.000000)
>     [...]
>     FstClient::dispatcher(0x3098d20, 58, 0, 0, (nil), 0.000000)
>     FstClient::dispatcher(0x3098d20, 71, 0, 0, (nil), 0.000000)

Opcode `10` looks like the host samplerate (`effSetSampleRate`),
and `11` looks like the blocksize (`effSetBlockSize`).

Opcode `5` is what we already suspect to be `effGetProgramName`,
which we can now confirm by implementing it.
If we do return a nice string for opcode 5,
REAPER will then call opcode `2` with a varying index and then calls opcode `5` again
(so supposedly it tries to change the program and query the new program name.
In theory it should use `effGetProgramNameIndexed` instead...

To find out what the pointers store, we can just try to print them out
(making sure to only print the first N bytes, in order to not flood the terminal).
For most of the opcodes this doesn't print anything (so the `ptr` points to a properly
zero-initialised memory region), only for opcode `51` we get something:

- `hasCockosExtensions`
- `hasCockosNoScrollUI`
- `receiveVstEvents`
- `receiveVstMidiEvent`
- `sendVstEvents`
- `sendVstMidiEvent`
- `wantsChannelCountNotifications`

This looks very much like a generic interface to either query a property by name
(could be `effCanDo`) or to allow vendor-specific programs (could be `effVendorSpecific`).
Looking up how JUCE handles the `effCanDo` in *juce_VST_Wrapper.cpp*, we see that
it really takes the `ptr` argument as string and compares it to values like
`bypass`, `sendVstEvents` and `hasCockosExtensions`. Bingo!

If we do *not* sort the output, we notice that after the plugin has been created with
opcode `0` is run (only) at the very beginning, when the plugin is initialised.
On the other hand, opcode `1` is run (only) at the very end.
So, these seem to be some initialisation/deinitialisation opcodes for the plugin.
`effOpen` and `effClose` look like likely candidates.





## inserting the plugin

If we add our new plugin to a REAPER track, we see printout like the following
(btw: for now i've muted the output for opcodes `53` and `3` as they are spamming
the console):

>     FstClient::setParameter(0x27967d0)[0] -> 0.000000
>     FstClient::setParameter(0x27967d0)[0] -> 0.000000
>     FstClient::dispatcher(0x27967d0, 8, 0, 0, 0x7ffeca15b580, 0.000000)
>     FstClient::dispatcher(0x27967d0, 7, 0, 0, 0x7ffeca15b480, 0.000000)
>     FstClient::dispatcher(0x27967d0, 6, 0, 0, 0x7ffeca15b480, 0.000000)
>     FstClient::setParameter(0x27967d0)[1] -> 279445823603548880896.000000
>     FstClient::setParameter(0x27967d0)[1] -> 0.000000
>     FstClient::dispatcher(0x27967d0, 8, 1, 0, 0x7ffeca15b580, 0.000000)
>     FstClient::dispatcher(0x27967d0, 7, 1, 0, 0x7ffeca15b480, 0.000000)
>     FstClient::dispatcher(0x27967d0, 6, 1, 0, 0x7ffeca15b480, 0.000000)
>     FstClient::setParameter(0x27967d0)[2] -> 279445823603548880896.000000
>     FstClient::setParameter(0x27967d0)[2] -> 0.000000
>     FstClient::dispatcher(0x27967d0, 8, 2, 0, 0x7ffeca15b580, 0.000000)
>     FstClient::dispatcher(0x27967d0, 7, 2, 0, 0x7ffeca15b480, 0.000000)
>     FstClient::dispatcher(0x27967d0, 6, 2, 0, 0x7ffeca15b480, 0.000000)

Since the opcodes `6`, `7` and `8` are the only ones that use the index
(with an index that goes until `numParams-1`), they are likely to be
parameter related. Conincidientally we have 3 parameter-related opcodes:
`effGetParamLabel`, `effGetParamDisplay` and `effGetParamName`.

If we respond to these opcodes with some strings (by copying them into `object`) and
display the generic FX-GUI in reaper, we notice that our strings appear besides the
three parameter sliders (`8` to the left, and `7` followed by `6` to the right).
For other plugins, these strings read like "Gain" (on the left) and "+0.00 dB" (on the right).
We can pretty safely assume that *Name* refers to the human readable name of the parameter (e.g. "Gain"),
so *Display* would be a nice string rendering of the value itself (e.g. "+0.00")
and *Label* would be the units (e.g. "dB").

Playing around with the sliders and watching the calls to `setParameter`,
we see that whenever we move the slider, both the setter- and the getter-function are called.
However, we get bogus values, e.g.:

>     FstClient::setParameter(0x2a88690)[0] -> -27706476332321908694659177423680045056.000000

This looks like the `setParameter` callback is not called correctly.
Remember, that we used arbitrary positions for the function pointers in the `AEffect` structure
(we were just lucky that the `dispatcher` function was at the correct place).
This means that the host might call the `setParameter` function without providing
any explicit `value` argument at all )e.g. because the host things this is really the getter).
So let's play with those, and just revert the order of the two functions:

~~~C
typedef struct AEffect_ {
  t_fstInt32 magic;
  AEffectDispatcherProc* dispatcher;
  AEffectProcessProc* process;
  AEffectSetParameterProc* setParameter;
  AEffectGetParameterProc* getParameter;
  // ...
}
~~~

And indeed, this seems to have done the trick!

When we move a slider, both parameter getter and setter are called as well as the opcodes `6`, `7` & `8`.
About 0.5 seconds after the last parameter change happened, the host calls opcode `5` (which we already believe
to be `effGetProgramName`).

# Part: more enums

## eff[...]Program[...]
If we set the `numPrograms` to some identifiably value (e.g. 5) and open the plugin with REAPER,
we can see that it does something like, the following when opening up the generic GUI:

~~~
FstClient::dispatcher(0x3339ff0, 2, 0, 0, (nil), 0.000000)
FstClient::dispatcher(0x3339ff0, 5, 0, 0, 0x7ffc1b29af60, 0.000000)
FstClient::dispatcher(0x3339ff0, 2, 0, 1, (nil), 0.000000)
FstClient::dispatcher(0x3339ff0, 5, 0, 0, 0x7ffc1b29af60, 0.000000)
FstClient::dispatcher(0x3339ff0, 2, 0, 2, (nil), 0.000000)
FstClient::dispatcher(0x3339ff0, 5, 0, 0, 0x7ffc1b29af60, 0.000000)
FstClient::dispatcher(0x3339ff0, 2, 0, 3, (nil), 0.000000)
FstClient::dispatcher(0x3339ff0, 5, 0, 0, 0x7ffc1b29af60, 0.000000)
FstClient::dispatcher(0x3339ff0, 2, 0, 4, (nil), 0.000000)
FstClient::dispatcher(0x3339ff0, 5, 0, 0, 0x7ffc1b29af60, 0.000000)
FstClient::dispatcher(0x3339ff0, 2, 0, 0, (nil), 0.000000)
~~~

If we write a different string into the `ptr` for each call to opcode `5`,
we can see that the generic GUI displays our strings as the program names.
So it really seems that REAPER makes each program current and then
queries the current program name in order to retrieve all program names.
Which confirms that `effGetProgramName` is `5` and introduces
`effSetProgram` as `2`.

We can now easily find the opcode for the corresponding `effSetProgramName`,
by passing a unique string (`"program#%02d" % opcode`) for the opcodes 0..10.
If we call `effGetProgramName` afterwards, we see that the program name is now
*program#04*, so `effSetProgramName` is `4`.

We have `effSetProgram`, `effSetProgramName` and `effGetProgramName` as `2`, `4` and `5` resp.,
so probably `effGetProgram` is `3`.
This can easily be confirmed by a little test program that first changes the program
to a known value (using a plugin that actually has that many programs!) via `effSetProgram`,
and then calling opcode `3` to get that value back (as returned from the dispatcher).

*MrsWatson* uses the `kVstMaxProgNameLen` to allocate memory for the string returned by
`effGetProgramName`. OTOH, JUCE just will allocate 264 bytes when used as a host,
but when acting as a plugin it will only copy the 24+1 bytes (including the terminating `\0`).
For `effSetProgramName`, JUCE will only send 24 bytes.
REAPER will crash if we attempt to write more than 265 bytes into the program name string.

Most likely `kVstMaxProgNameLen` is just 24 characters. and hosts traditionally accept more,
in case a plugin misbehaves.


## effEdit*

The *Protoverb* plugin prints information, for which we can guess opcode names:

| opcode | printout                            | guessed opcodeName |
|--------|-------------------------------------|--------------------|
| 13     | "AM_VST_Editor::getRect 1200 x 600" | effEditGetRect?    |
| 14     | "AM_VST_Editor::open"               | effEditOpen?       |
| 15     | "closed editor."                    | effEditClose?      |

Opcode `13` does not crash if we provide it a ptr to some memory,
and it will write some address into that provided memory.
It will also return that same address (just what JUCE does for the `effEditGetRect` opcode.)
If we read this address as `ERect` we get a garbage rectangle (e.g. `0x78643800+0+0`).
The memory looks like

>     00 00 00 00 58 02 B0 04  00 00 00 00 00 00 00 00

Given that a GUI rectangle will most likely not have a zero width or height,
it might be, that the `ERect` members are really only 2-bytes wide (`short`),
which would then translate to the numbers `0, 0, 600, 1200`,
which happens to align nicely with the printout of *Protoverb*

~~~C
typedef struct ERect_ {
  short left;
  short top;
  short right;
  short bottom;
} ERect;
~~~

## effIdentify

the following little helper gives us information, what a plugin returns for the various opcodes:

~~~C
  for(size_t opcode=16; opcode<64; opcode++) {
    char buffer[200] = { 0 };
    VstIntPtr result = effect->dispatcher(effect, opcode, 0, 0, buffer, 0.f);
    if(result || *buffer)
      printf("tested %d\n", opcode);
    if(result)
      printf("\t%llu 0x%llX\n", result, result);
    if(*buffer) {
      printf("\tbuffer '%.*s'\n", 512, buffer);
      hexprint(buffer, 16);
    }
  }
~~~

I get:

>     tested 22	 | 1316373862 0x4E764566
>     tested 23  |  791 0x317 |  buffer '�  ��U'
>     tested 24  |  1 0x1
>     tested 25  |  1 0x1
>     tested 26  |  1 0x1
>     tested 29  |  1 0x1     |  buffer 'initialize'
>     tested 33  |  1 0x1     |  buffer 'Protoverb-In0'
>     tested 34  |  1 0x1     |  buffer 'Protoverb-Out1'
>     tested 35  |  1 0x1
>     tested 45  |  1 0x1     |  buffer 'Protoverb'
>     tested 47  |  1 0x1     |  buffer 'u-he'
>     tested 48  |  1 0x1     |  buffer 'Protoverb 1.0.0'
>     tested 49  |  65536 0x10000
>     tested 51  |  18446744073709551615 0xFFFFFFFFFFFFFFFF
>     tested 58  |  2400 0x960
>     tested 63  |  18446744073709551615 0xFFFFFFFFFFFFFFFF

the value returned by opcode `22` is identical with the once that JUCE returns for the `effIdentify` opcode.
So we assign that.

## effGetProgramNameIndexed
The "initialize" string we already know as Protoverbs sole program name.
The only missing opcode for retrieving program names is `effGetProgramNameIndexed`.

We can confirm this assumption with a plugin that has many programs (e.g. *tonespace*)
and the following snippet:

~~~C
  for(int i=0; i<effect->numPrograms; i++) {
    char buffer[200] = { 0 };
    effect->dispatcher(effect, 29, i, 0, buffer, 0.f);
    if(*buffer) {
      printf("\tbuffer#%d '%.*s'\n", i, 512, buffer);
    }
  }
~~~

## effectName, productString, vendorString, vendorVersion

In the [C-snippet above](#effidentify), we also get nice strings for
opcodes `45`, `47`, and `48`, that look like they would be good matches for
`effGetEffectName` (45), `effGetProductString` (48) and `effGetVendorString` (47).

The value returned for `49` looks suspiciously like the plugin version.

E.g. if we compare the values to the ones we assumed to be `AEffect.version`:

| plugin      | AEffect.version | returned by `49` |
|-------------|-----------------|------------------|
| BowEcho     | 110             | 110              |
| Danaides    | 102             | 102              |
| Digits      | 1               |                  |
| InstaLooper | 1               | 1                |
| Protoverb   | 1               | 0x10000          |
| hypercyclic | 150             | 150              |
| tonespace   | 250             | 250              |

So we assign `effGetVendorVersion` the value of `49`

## effGetVstVersion

Opcode `58` returns `2400` for all plugins I've tested so far.
This is the same value that JUCE pluginhosts return (hardcoded) to the
`audioMasterVersion` opcode.
So it's probably save to assume that `58` is the opcode
that allows the host to query the plugin for it's VST version.
Which would make it the `effGetVstVersion`.

## Chunks
In the [C-snippet above](#effgetprogramnameindexed), we also get an interesting result for opcode `23`:
the return value is a reasonable number (791), and something is written to the `ptr`.

If we take a closer look on what is written, we see that the plugin wrote the address
of some heap-allocated memory into our buffer.
Comparing this to how JUCE handles the various opcodes, this could be `effGetChunk`.

Inspecting the data returned by *Protoverb*, we get something like:

~~~
#pgm=initialize
#AM=Protoverb
#Vers=10000
#Endian=little
#nm=5
#ms=none
#ms=ModWhl
#ms=PitchW
#ms=Breath
#ms=Xpress
#nv=0
#cm=main
CcOp=100.00
#FDN=1
#cm=PCore
[...]
// Section for ugly compressed binary Data
// DON'T TOUCH THIS
[...]
~~~

This indeed looks very much like some state dump of the plugin.
Other plugins returns only binary data (*hypercyclic*, *tonespace*).

*InstaLooper* is the only tested plugins that doesn't return anything.
It also happens to be the only tested plugin that has the bit#6 in
the `AEffect.flags` structure set to `0` - hinting that
`effFlagsProgramChunks = (1<<5)` (which would explain that this
flag has something to do with the ability to get/set chunks).

Of the `effFlags*`, the only reamining unknown flags is `effFlagsNoSoundInStop`.
Our [test plugins](#flags) have the flags `1<<0`, `1<<4`, `1<<5`, `1<<8` and `1<<9` set,
of which we haven't used `1<<9` yet.
Most likely this is the `effFlagsNoSoundInStop`.

## VstPinProperties
Opcodes `33` and `34` return strings that seem to be related to input and output.
There is no obvious effect opcode that provides such strings, nor can we find anything
in the [JUCE effect Opcodes tables](#juce-effect-opcodes).

The only opcodes that deal with input and output are `effGetInputProperties` resp. `effGetOutputProperties`.
But these should return a `VstPinProperties` struct.

So let us investigate the returned buffer:

~~~
00000000  50 72 6f 74 6f 76 65 72  62 2d 49 6e 30 00 00 00  |Protoverb-In0...|
00000010  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00000020  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00000030  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00000040  03 00 00 00 00 00 00 00  50 72 6f 74 6f 76 30 00  |........Protov0.|
00000050  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
~~~

The first 14 bytes are the 'Protoverb-In1' string.
The interesting thing is, that at position @44 we see a similar string 'Protov0'.

Investigating the buffer for other plugins, gives similar results (e.g.
*hypercyclic* has a string 'out0' at position @0 and the same string 'out0' @44).

Compare this with our tentative definition of the `VstPinProperties` struct:

~~~
typedef struct VstPinProperties_ {
  int arrangementType;
  char*label;
  char*shortLabel;
  int flags;
} VstPinProperties;
~~~

"Protoverb-In0" and "Protov0" could indeed be the `label` resp. `shortLabel`.

At position @40 we have a number `3` (in all tested plugins).
This could be a `flag` or an `arrangementType`.

So the opcode really filled in a `VstPinProperties` structure,
rather than a string, as we first thought.
However, because the structure contains a string at the very beginning
(that is: the address of the string is the same as the address of the containing struct),
our naive "print buffer" approach had revealed it.

In order for the struct to be useful, the strings must have a fixed size.
Since we have other information at @40, the first string must not exceed this byte.
So it can have a maximum size of 64 bytes.
The second string (supposedly `shortLabel`) ought to be shorter than the first one.
If it only occupied 8 bytes, the entire struct would nicely align to 16 byte boundaries.

The only remaining question is, what about the `flags` and `arrangementType` fields.
Since both would be 4 byte ints (as all `int`s we have seen so far were 4 bytes),
both would fit into the gap between `label` and `shortLabel`.
In this case, one would have to be `0`, and the other `3`.

JUCE-4.2.1 will always set the flags to *at least* `kVstPinIsActive | kVstPinUseSpeaker`,
suggesting that this is the non-0 field:

~~~
typedef struct VstPinProperties_ {
  char label[64];
  int flags; //?
  int arrangementType; //?
  char shortLabel[8];
} VstPinProperties;
~~~

There are also two related constants `kVstMaxLabelLen` and `kVstMaxShortLabelLen`
which will have the values `64` and `8` resp.


## MIDI
responding with `1` to `effCanDo receiveVstEvents` resp `effCanDo receiveVstMidiEvents`
gives us `opcode:25` events correlating to MIDI-events (sent from a MIDI-item
with a little sequence).

A typical frame received is:

~~~
00000000  02 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
00000010  50 3e 00 cc 90 7f 00 00  70 3e 00 cc 90 7f 00 00
00000020  00 00 00 00 00 00 00 00  01 00 00 00 18 00 00 00
~~~

The frame itself is at 0x7F90CC003E28, which makes it clear that we have
two addresses at positions @10 (0x7F90CC003E50) and @18 (0x7F90CC003E70).

The first address is only 40 bytes after the frame itself, which is indeed
position @28.

This looks very much like our `VstEvents` structure, with 2 events.
The number of events is also in stored in the first 4 bytes of the structure.
There seems to be a bit of padding between `numEvents` and the `events`,
depending on the architecture (32bit vs 64bit).
Since the `events` member is really a varsized array of `VstEvent*` pointers,
we can refine our struct definition as:

~~~
typedef struct VstEvents_ {
  int numEvents;
  VstIntPtr _pad;//?
  VstEvent*events[];
} VstEvents;
~~~

From the difference of the two `VstEvent` addresses, we gather, that a single `VstEvent`
(of type `VstMidiEvent`, because this is really what we are dealing with right now)
can have a maximum size of 32 bytes.

JUCE handles `VstEvents` in the `effProcessEvents` opcode, so we assign it to `opcode:25`

## VstMidiEvent

Setting up a little MIDI sequence in reaper, that plays the notes `C4 G4 C5 F4` (that is `0x3c 0x43 0x48 0x41` in hex),
we can test the actual `VstMidiEvent` structure.

If we play back the sequence in a loop and print the first 32 bytes of each `VstEvent`, we get something like:

~~~
[...]
 01  00  00  00  18  00  00  00  00  00  00  00  00  00  00  00  00  00  00  00  00  00  00  00   90  3C  7F  00  00  00  00  00
 01  00  00  00  18  00  00  00  22  00  00  00  00  00  00  00  00  00  00  00  00  00  00  00   80  3C  00  00  00  00  00  00
 01  00  00  00  18  00  00  00  22  00  00  00  00  00  00  00  00  00  00  00  00  00  00  00   90  43  7F  00  00  00  00  00
 01  00  00  00  18  00  00  00  44  00  00  00  00  00  00  00  00  00  00  00  00  00  00  00   80  43  00  00  00  00  00  00
 01  00  00  00  18  00  00  00  44  00  00  00  00  00  00  00  00  00  00  00  00  00  00  00   90  48  7F  00  00  00  00  00
 01  00  00  00  18  00  00  00  66  00  00  00  00  00  00  00  00  00  00  00  00  00  00  00   80  48  00  00  00  00  00  00
 01  00  00  00  18  00  00  00  66  00  00  00  00  00  00  00  00  00  00  00  00  00  00  00   90  41  7F  00  00  00  00  00
 01  00  00  00  18  00  00  00  00  01  00  00  00  00  00  00  00  00  00  00  00  00  00  00   80  41  00  00  00  00  00  00
 01  00  00  00  18  00  00  00  00  00  00  00  00  00  00  00  00  00  00  00  00  00  00  00   90  3C  7F  00  00  00  00  00
 01  00  00  00  18  00  00  00  C3  00  00  00  00  00  00  00  00  00  00  00  00  00  00  00   80  3C  00  00  00  00  00  00
 01  00  00  00  18  00  00  00  C3  00  00  00  00  00  00  00  00  00  00  00  00  00  00  00   90  43  7F  00  00  00  00  00
 [...]
~~~

The interesting part are the last 8 bytes, where we can see the MIDI NoteOn (`0x90`) and NoteOff (`0x80`) messages for our
notes. The NoteOn velocity is apparently `0x7f` (127).
One of the fields should be `byteSize`. If we take the MIDI-bytes into account, we have at least 27 bytes (28, if we allow 4 byte
MIDI messages, as JUCE suggests), and at most 32 bytes (see above).
The 5th byte of each line (`0x18` aka 24) is the only value that is close by.
JUCE uses `sizeof(VstMidiEvent)` for filling in the `byteSize` field.
If REAPER does the same, we lack (at least) 4 bytes.
One possible explanation could be that the `midiData` field is declared as a varsized array `char midiData[0]` in the SDK.

The `byteSize`, `deltaFrames` and `type` fields are available in `VstEvent`, `VstMidiEvent` and `VstMidiSysexEvent` structs.
Since we can cast the latter two to `VstEvent`, the common fields will be at the very beginning.

Since all events are of the same type, the `type` field should be constant for all our events, which happens to be the
true for the very first bytes. So let's assume `kVstMidiType` to be `0x01`.

Bytes at positions @8-11 vary and might be the `deltaFrames` field (JUCE uses this field to sort the events chronologically).
The remaining twelve 0-bytes in the middle need to be somehow assigned to `noteLength`, `noteOffset`, `detune` and `noteOffVelocity`.
I don't know anything about these fields. `noteLength` and `noteOffVelocity` seem to allow scheduling NoteOff events in the future.
`noteOffVelocity` should not need more than 7bits (hey MIDI).

Anyhow, here's what we have so far:

~~~
typedef struct VstEvent_ {
  int type;
  int byteSize;
  int deltaFrames;
} VstEvent;

typedef struct VstMidiEvent_ {
  int type;
  int byteSize;
  int deltaFrames;
  /* no idea about the position and sizes of the following four members */
  short noteLength; //?
  short noteOffset; //?
  int detune; //?
  int noteOffVelocity; //?
  unsigned char midiData[]; //really 4 bytes
} VstMidiEvent;
~~~

## VstMidiSysexEvent

Using REAPER's plugin, we can generate SysEx messages.
I prepared a SysEx message `F0 01 02 03 04 03 02 01 F7` and this is the
event that our plugin received:

~~~
06 00 00 00 30 00 00 00  00 00 00 00 00 00 00 00
09 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
78 DB 01 14 3C 7F 00 00  00 00 00 00 00 00 00 00
F0 01 02 03 04 03 02 01  F7 00 00 00 00 00 00 00
~~~

Aha: the first element changed to a low value as well, so `0x06` might be the value of `kVstSysExType`.
The size is `48` bytes, which covers a pointer 0x7F3C1401DB78 at position @20.
This address is 48 bytes after the current data frame (so right after the data frame, according to the `byteSize` field),
and it happens to hold the sequence `F0 01 02 03 04 03 02 01 F7`, our SysEx message.
The SysEx message is 9 bytes long (see position @10).
The members `flags` and `resvd1` resp. `resvd2` seem to be set to all zeros,
so we cannot determine their position.
I guess the `reserved` fields will be at the very end, which gives us:

~~~
typedef struct VstMidiSysexEvent_ {
  int type;
  int byteSize;
  int deltaFrames;
  int _pad; //?
  int dumpBytes;
  int flags; //?
  VstIntPtr resvd1; //?
  char*sysExDump;
  VstIntPtr resvd2; //?
} FST_UNKNOWN(VstMidiSysexEvent);
~~~

## enter MrsWatson
When trying to [compile MrsWatson](#compiling-mrswatson) there's an additional complication,
as it seems there are some more members in the `VstMidiEvent` structure.

| member      | type    |
|-------------|---------|
| `flags`     | integer |
| `reserved1` | integer |
| `reserved1` | integer |

This is weird because the `VstMidiEvent.byteSize` reported by REAPER is *24*,
and our structure is already 28 bytes long (if we include the 4-bytes for `midiData`).
We probably need another host for testing this structure (and see whether it's just REAPER that reports a bogus `byteSize`,
or whether some of the fields are only `unsigned char`).

The `flags` member is also part of the `VstMidiSysexEvent`, so it might well be *common* to both structs
(as in: at the same position).
So far, the two structures share the first 12 bytes (`type`, `byteSize`, `deltaFrames`).
We have assigned 4-byte `_pad` member to `VstMidiSysexEvent` right after the common members,
and the `VstMidiEvent` has some arbitrarily `noteLength` and `noteOffset`, which are always *0*.
So let's revise this and make the underlying `VstEvent` nicely ending on an 8-bytes boundary:

~~~C
typedef struct VstEvent_ {
  int type;
  int byteSize;
  int deltaFrames;
  int flags;
} VstEvent;
~~~

The `VstMidiSysexEvent` changes accordingly:
~~~C
typedef struct VstMidiSysexEvent_ {
  int type;
  int byteSize;
  int deltaFrames;
  int flags;
  int dumpytes;
  int _pad;
  VstIntPtr resvd1;
  char*sysexDump;
  VstIntPtr resvd2;
} VstMidiSysexEvent;
~~~

This works nicely on 64bit, but when running on 32bit, REAPER core-dumps.

On 64bit the first `byteSize` (48) bytes look like:
~~~
0000	  06 00 00 00 30 00 00 00  00 00 00 00 00 00 00 00
0010	  09 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
0020	  28 EB 04 AC 19 7F 00 00  00 00 00 00 00 00 00 00
~~~

Whereas on 32bit the first `byteSize` (32) bytes look like:
~~~
0000	  06 00 00 00 20 00 00 00  00 00 00 00 00 00 00 00
0010	  09 00 00 00 00 00 00 00  B4 E5 E4 ED 00 00 00 00
~~~

Fiddling around with the type-sizes, it seems we can use `VstIntPtr` as the type for `dumpBytes`,
and everything will align nicely, without the need for some padding bytes.
(Using a pointer-sized into for the size of `sysexDump` aligns with the typical use of `size_t` for memory-sizes,
although i hope that there will never be a need for 6 terabyte dumps of SysEx data...)

~~~C
typedef struct VstMidiSysexEvent_ {
  int type;
  int byteSize;
  int deltaFrames;
  int flags;
  VstIntPtr dumpytes;
  VstIntPtr resvd1;
  char*sysexDump;
  VstIntPtr resvd2;
} VstMidiSysexEvent;
~~~

Since we've added the 4-bytes `flags` to the common `VstEvent` members, we also need to fix the `VstMidiEvent`,
so that the midiData is still at position @18.
We already have (quite arbitrarily, to get the alignment right) assigned the `noteLength` and `noteOffset`
members a `short` type.
If we also do the same for `noteOffVelocity` and `detune`, we gain exactly 4 bytes and the data aligns again.

We also need to find some place for the `reserved1` and `reserved2` members.
For now we just arbitrarily use them to fill up the structure,
so it also ends at an 8-bytes boundary:

~~~C
typedef struct VstMidiEvent_ {
  int type;
  int byteSize;
  int deltaFrames;
  int flags;
  short noteLength; //?
  short noteOffset; //?
  short detune; //?
  short noteOffVelocity; //?
  unsigned char midiData[4]; /* @0x18 */
  short reserved1; //?
  short reserved2; //?
} VstMidiEvent;
~~~

Again: note that `noteLength`, `noteOffset`, `noteOffVelocity`, `detune`, `reserved1` and `reserved2` are arbitrarily
assigned in type and position.
The `reserved` types will probably always be *0*,
but we will have to find a plugin host that fills in the `note*` members to learn more about them.

Also note that the size of the `VstMidiEvent` struct is currently 32 bytes,
rather than the 24 bytes claimed by REAPER.

# some audioMaster opcodes
time to play get some more opcodes for the host.
We want the plugin host to be properly running, so the plugin initialisation time is probably too early.
All real plugins observed so far, call a number of host-opcodes when they receive opcode `12`:
/*BowEcho*/*Danaides*/*hypercyclic*/*tonespace* run opcode `23`, *hypercyclic*/*tonespace* also call `7`
and all plugins call `6`.

So opcode:12 seems to be a good place to test the plugin host, which we do
by simply running again, but this time in the opcode:12 callback.
~~~
for(size_t opcode=0; opcode<50; opcode++) {
   char buffer[1024] = {0};
   hostDispatch(effect, opcode, 0, 0, buf, 0.f);
}
~~~

This time this gives us the following opcodes that don't return `0x0`:

| op | result               |
|----|----------------------|
| 2  | 57005 (0xDEAD)       |
| 6  | 1 (0x1)              |
| 7  | 60352024 (0x398E618) |
| 8  | 1 (0x1)              |
| 11 | 3 (0x3)              |
| 12 | 1 (0x1)              |
| 13 | 1 (0x1)              |
| 15 | 1 (0x1)              |
| 23 | 1 (0x1)              |
| 42 | 1 (0x1)              |
| 43 | 1 (0x1)              |
| 44 | 1 (0x1)              |
| 48 | 1 (0x1)              |

Opcode:2 is interesting, as it has such a low number *and* such a funny hex-representation
(which is obviously a magic value).
The [JUCE host opcode table](#juce-host-opcodes) does not leave many possibilities.
The only entry that makes sense is the `audioMasterCurrentId`.

Opcode:11, returning `3`, might be `audioMasterGetAutomationState` (unless it is in the list of opcodes not handled by JUCE).

## VstTimeInfo

Opcode:7 returns a pointer. If we print the memory at the given position, we get the first 80 bytes like:

>     09 00 00 00 30 D3 01 41  00 00 00 00 80 88 E5 40
>     00 90 30 F3 DF FE D2 42  32 EF 75 99 3F 7D 1A 40
>     00 00 00 00 00 00 5E 40  0A A8 FF FF FF FF 0F 40
>     00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
>     04 00 00 00 04 00 00 00  02 00 00 00 EF BE AD DE

This *might* be `VstTimeInfo` struct as returned by `audioMasterGetTime` (let's assume that!).
Record that chunk multiple times (luckily the opcode:12 gets called whenever we start playback in REAPER), and inspect it.
Bytes @08-0f (`00 00 00 00 80 88 E5 40`) are *44100* in double precision, and the `VstTimeInfo` struct has a `sampleRate`
field, that is supposed to be `double`. That looks very promising!

Bytes @20-27 hold *120* in double (`tempo`). The first 8 bytes @00-07 read as double are monotonically increasing,
but the values (40448, 109056, 78848, 90624) are certainly not `nanoSeconds` (rather: samples).
Bytes @28-30 have values like *0*, *3.99999*, *15.9999*, *12.9999* (in double), which seems to be right.

The `VstTimeInfo` struct supposedly has 7 double values and 6 int32 values.

To decode the struct, we start by interpreting it as a simple array of only `double` values,
and print the values whenever the plugin receices `opcode:53`
(REAPER calls this opcode about every 50ms whenever the (generic) GUI of the plugin is open).
We are not entirely sure about the size of the structure, our first estimation of the member types would
require 80 bytes. For safety we add some more, and check the first 96 bytes
(but keep in mind that this means that we might also read in garbage at the end of the data).

The following is what I get when playing back a short loop in a *120bpm, 4/4* project.
The loop starts at beat `1.2.00` (first bar, second quarter note) aka half a second into the project,
the loop end point is at `4.2.00` aka 6.5 seconds.

~~~
 277572.0 44100.0 1.48147e+14 12.5883 120.0 12.0 1.0 13.0   8.48798e-314 -1.1886e+148 3.42363e-310 1.08646e-311
 279108.0 44100.0 1.48147e+14 12.6580 120.0 12.0 1.0 13.0   8.48798e-314 -1.1886e+148 3.42363e-310 1.08646e-311
 281156.0 44100.0 1.48147e+14 12.7508 120.0 12.0 1.0 13.0   8.48798e-314 -1.1886e+148 3.42363e-310 1.08646e-311
 282692.0 44100.0 1.48147e+14 12.8205 120.0 12.0 1.0 13.0   8.48798e-314 -1.1886e+148 3.42363e-310 1.08646e-311
 284740.0 44100.0 1.48147e+14 12.9134 120.0 12.0 1.0 13.0   8.48798e-314 -1.1886e+148 3.42363e-310 1.08646e-311
  22188.0 44100.0 1.48147e+14 1.00626 120.0  0.0 1.0 13.0   8.48798e-314 -1.1886e+148 3.42363e-310 1.08646e-311
  23724.0 44100.0 1.48147e+14 1.07592 120.0  0.0 1.0 13.0   8.48798e-314 -1.1886e+148 3.42363e-310 1.08646e-311
  25772.0 44100.0 1.48148e+14 1.16880 120.0  0.0 1.0 13.0   8.48798e-314 -1.1886e+148 3.42363e-310 1.08646e-311
  27308.0 44100.0 1.48148e+14 1.23846 120.0  0.0 1.0 13.0   8.48798e-314 -1.1886e+148 3.42363e-310 1.08646e-311
 [...]
 262212.0 44100.0 1.48203e+14 11.8917 120.0  8.0 1.0 13.0   8.48798e-314 -1.1886e+148 3.42363e-310 1.08646e-311
 263748.0 44100.0 1.48203e+14 11.9614 120.0  8.0 1.0 13.0   8.48798e-314 -1.1886e+148 3.42363e-310 1.08646e-311
 [...]
 283716.0 44100.0 1.48204e+14 12.8669 120.0 12.0 1.0 13.0   8.48798e-314 -1.1886e+148 3.42363e-310 1.08646e-311
 285252.0 44100.0 1.48204e+14 12.9366 120.0 12.0 1.0 13.0   8.48798e-314 -1.1886e+148 3.42363e-310 1.08646e-311
  22188.0 44100.0 1.48204e+14 1.00626 120.0  0.0 1.0 13.0   8.48798e-314 -1.1886e+148 3.42363e-310 1.08646e-311
  24236.0 44100.0 1.48204e+14 1.09914 120.0  0.0 1.0 13.0   8.48798e-314 -1.1886e+148 3.42363e-310 1.08646e-311
 [...]
  81068.0 44100.0 1.48205e+14 3.67655 120.0  0.0 1.0 13.0   8.48798e-314 -1.1886e+148 3.42363e-310 1.08646e-311
  83116.0 44100.0 1.48205e+14 3.76943 120.0  0.0 1.0 13.0   8.48798e-314 -1.1886e+148 3.42363e-310 1.08646e-311
  84652.0 44100.0 1.48205e+14 3.83909 120.0  0.0 1.0 13.0   8.48798e-314 -1.1886e+148 3.42363e-310 1.08646e-311
  86188.0 44100.0 1.48205e+14 3.90875 120.0  0.0 1.0 13.0   8.48798e-314 -1.1886e+148 3.42363e-310 1.08646e-311
  88236.0 44100.0 1.48205e+14 4.00163 120.0  4.0 1.0 13.0   8.48798e-314 -1.1886e+148 3.42363e-310 1.08646e-311
  90284.0 44100.0 1.48205e+14 4.09451 120.0  4.0 1.0 13.0   8.48798e-314 -1.1886e+148 3.42363e-310 1.08646e-311
  91820.0 44100.0 1.48205e+14 4.16417 120.0  4.0 1.0 13.0   8.48798e-314 -1.1886e+148 3.42363e-310 1.08646e-311
  93356.0 44100.0 1.48205e+14 4.23383 120.0  4.0 1.0 13.0   8.48798e-314 -1.1886e+148 3.42363e-310 1.08646e-311
  95404.0 44100.0 1.48205e+14 4.32671 120.0  4.0 1.0 13.0   8.48798e-314 -1.1886e+148 3.42363e-310 1.08646e-311
~~~

Some columns have very reasonable values (e.g. columns 1, 2, 4, 5, 6, 7 and 8),
while others are out of bounds (9, 10, 11 and 12).
The reasonable values indicate that we used the corret type to decode the bytes.
A value like "8.48798e-314" is hardly useful in the context of audio processing,
so most likely these bytes just don't represent double values.
Note that there's a clear divide between reasonable values to the left (the first 8 numbers; aka 64 bytes)
and out-of-bound values on the right (bytes 65+).

The third column (bytes @10-18) is somewhere in the middle, and requires closer inspection.

Using the same project, but displaying all the values as 32bit `(signed int)`

~~~
5    1091576768  0  1088784512  -44277760    1122039678  1529826454   1076362265  0  1079902208  -11258  1075838975  0  1072693248  0  1076494336   4  4  3  -559038737  0  16134  2048  512
6    1091584960  0  1088784512  1427722240   1122039679  1287516282   1076374439  0  1079902208  -5629   1076363263  0  1072693248  0  1076494336   4  4  4  -559038737  0  16134  1023  512
6    1091591104  0  1088784512  -1747245056  1122039679  -1041699995  1076383569  0  1079902208  -5629   1076363263  0  1072693248  0  1076494336   4  4  4  -559038737  0  16134  1023  512
[...]
12   1091650496  0  1088784512  446820352    1122039682  -650965094   1076471830  0  1079902208  -5629   1076363263  0  1072693248  0  1076494336   4  4  4  -559038737  0  16134  2048  512
12   1091658688  0  1088784512  1918820352   1122039682  -893275266   1076484004  0  1079902208  -5629   1076363263  0  1072693248  0  1076494336   4  4  4  -559038737  0  16134  1535  512
0    1087755776  0  1088784512  -872146944   1122039682  -494749067   1072707989  0  1079902208  0       0           0  1072693248  0  1076494336   4  4  1  -559038737  0  16134  1535  512
1    1087854080  0  1088784512  247853056    1122039683  -1948610105  1072781033  0  1079902208  0       0           0  1072693248  0  1076494336   4  4  1  -559038737  0  16134  1024  512
[...]
5    1089680768  0  1088784512  1337147392   1122039692  -1131689788  1074564040  0  1079902208  0       0           0  1072693248  0  1076494336   4  4  1  -559038737  0  16134  1024  512
5    1089713536  0  1088784512  -1485819904  1122039692  -2100930480  1074612736  0  1079902208  0       0           0  1072693248  0  1076494336   4  4  1  -559038737  0  16134  1536  512
5    1089738112  0  1088784512  -365819904   1122039692  1467106297   1074649258  0  1079902208  0       0           0  1072693248  0  1076494336   4  4  1  -559038737  0  16134  1536  512
6    1089770880  0  1088784512  1138180096   1122039693  497865605    1074697954  0  1079902208  0       0           0  1072693248  0  1076494336   4  4  1  -559038737  0  16134  1024  512
6    1089803648  0  1088784512  -1684787200  1122039693  -471375087   1074746649  0  1079902208  0       0           0  1072693248  0  1076494336   4  4  1  -559038737  0  16134  1536  512
6    1089828224  0  1088784512  -564787200   1122039693  -1198305606  1074783171  0  1079902208  0       0           0  1072693248  0  1076494336   4  4  1  -559038737  0  16134  1536  512
3    1089860992  0  1088784512  907212800    1122039694  -1083773151  1074811133  0  1079902208  -22518  1074790399  0  1072693248  0  1076494336   4  4  2  -559038737  0  16134  1024  512
1    1089893760  0  1088784512  -1883754496  1122039694  -1568393499  1074835481  0  1079902208  -22518  1074790399  0  1072693248  0  1076494336   4  4  2  -559038737  0  16134  1535  512
-1   1089918335  0  1088784512  -763754496   1122039694  -1931858760  1074853742  0  1079902208  -22518  1074790399  0  1072693248  0  1076494336   4  4  2  -559038737  0  16134  1535  512
-3   1089951103  0  1088784512  708245504    1122039695  1878488188   1074878090  0  1079902208  -22518  1074790399  0  1072693248  0  1076494336   4  4  2  -559038737  0  16134  1024  512
-6   1089983871  0  1088784512  -2114721792  1122039695  1393867840   1074902438  0  1079902208  -22518  1074790399  0  1072693248  0  1076494336   4  4  2  -559038737  0  16134  1535  512
[...]
-43  1090475391  0  1088784512  -1285558272  1122039700  -1580470084  1075267656  0  1079902208  -22518  1074790399  0  1072693248  0  1076494336   4  4  2  -559038737  0  16134  1024  512
-46  1090508159  0  1088784512  186441728    1122039701  -2065090432  1075292004  0  1079902208  -22518  1074790399  0  1072693248  0  1076494336   4  4  2  -559038737  0  16134  2047  512
-24  1090529983  0  1088784512  1690441728   1122039701  1745256516   1075316352  0  1079902208  -22518  1074790399  0  1072693248  0  1076494336   4  4  2  -559038737  0  16134  1024  512
-25  1090542271  0  1088784512  -1484525568  1122039701  1381791255   1075334613  0  1079902208  -22518  1074790399  0  1072693248  0  1076494336   4  4  2  -559038737  0  16134  1024  512
-26  1090558655  0  1088784512  -12525568    1122039701  897170907    1075358961  0  1079902208  -22518  1074790399  0  1072693248  0  1076494336   4  4  2  -559038737  0  16134  2047  512
[...]
-49  1090857663  0  1088784512  1353670656   1122039708  642784148    1075803310  0  1079902208  -22518  1074790399  0  1072693248  0  1076494336   4  4  2  -559038737  0  16134  1535  512
-50  1090869951  0  1088784512  -1821296640  1122039708  279318887    1075821571  0  1079902208  -22518  1074790399  0  1072693248  0  1076494336   4  4  2  -559038737  0  16134  2047  512
-51  1090886335  0  1088784512  -349296640   1122039708  2044832918   1075842447  0  1079902208  -11258  1075838975  0  1072693248  0  1076494336   4  4  3  -559038737  0  16134  1024  512
-49  1090902719  0  1088784512  1122703360   1122039709  1802522746   1075854621  0  1079902208  -11258  1075838975  0  1072693248  0  1076494336   4  4  3  -559038737  0  16134  1535  512
-48  1090919103  0  1088784512  -1668263936  1122039709  1560212574   1075866795  0  1079902208  -11258  1075838975  0  1072693248  0  1076494336   4  4  3  -559038737  0  16134  2048  512
-47  1090931391  0  1088784512  -548263936   1122039709  -769003703   1075875925  0  1079902208  -11258  1075838975  0  1072693248  0  1076494336   4  4  3  -559038737  0  16134  1023  512
~~~

Let's first inspect the first 64 bytes (columns 1 till 16):
columns 2, 9 and 13 are always *0*.
Columns 3, 4, 10, 14, 15 and 16 don't change, so there's no real information in them. Apart from that, their values don't seem to mean anything.
The other columns (1, 2, 5, 6, 7, 8, 11, 12) change, but i cannot make head nor tail of them.
This is somewhat OK, as the first 64 bytes decoded nicely for `double` numbers.
We were unsure about bytes @10-18 for double decoding, so let's inspect columns 5 and 6:
alas!, they don't make any sense as `int32` either, so require some more inspection.

The `double` decoding worked kind of nicely for the first 64 bytes and failed afterwards.
If we look at the int32 values for bytes 65+, we see `4 4 [1-4] -559038737 0 16134`.
At least the first 3 values are nice. If we change the metrum of the project to something weird like 3/7,
we get `3 7 [1-8] -559038737 0 16134`.
So the first two numbers denote the metrum.
Playing around with the project cursor a bit and watching the third number, it seems to be a bar counter.

The number `-559038737` has a (little endian) hex representation of 0xDEADBEEF and is a typical magic number.

We should also havea look at the bytes @10-18, which - so far - made most sense when decoded as double.
Because the numbers are so high (*1.48147e+14*) we divide them by 10^9.

This results in the display of a number (e.g. `152197`) that increments by 1 every second.
It doesn't matter whether the project is playing or not.
Given that we had to scale the number by 10^-9, the real value is in nanoseconds -
and our `VstTimeInfo` struct happens to have a `nanoSeconds` member!

So far we can conclude
~~~
typedef struct VstTimeInfo_ {
  double samplePos;
  double sampleRate;
  double nanoSeconds;
  double ppqPos;
  double tempo;
  double barStartPos;
  double cycleStartPos;
  double cycleEndPos;
  int timeSigNumerator;
  int timeSigDenominator;
  int currentBar;
  int magic;// 0xDEADBEEF
  //
} VstTimeInfo;
~~~

We do not know the position (and type) of the following struct members yet:
- `flags`
- `smpteFrameRate`
- `smpteOffset`

The `flags` member will be a bitfield.
JUCE assigns at least `kVstNanosValid` to it (and `kVstAutomationWriting` resp. `kVstAutomationReading`).
Looking up more constants with similar names, we find:
~~~
kVstNanosValid
kVstBarsValid
kVstClockValid
kVstCyclePosValid
kVstPpqPosValid
kVstSmpteValid
kVstTempoValid
kVstTimeSigValid
kVstAutomationWriting
kVstAutomationReading
~~~



### more on time flags
If we keep printing the raw data of the `VstTimeInfo` continuously
(e.g. as response to `opcode:53`), and start/stop the playback, we notice
something weird:
the bytes at position @54-57 (right after the magic `0xDEADBEEF`) keep changing
according to the playstate:

here's a few examples:

| @50-58      | action        |
|-------------|---------------|
| 00 2f 00 00 | pause         |
| 02 2f 00 00 | playing       |
| 06 3f 00 00 | looping       |
| 01 2f 00 00 | stopping      |
| 03 2f 00 00 | starting play |
| 07 3f 00 00 | starting loop |

Could it be, that these bytes actually encode some flags?
If we read the 4 bytes as int and print that in binary, we get:

| binary          | description |
|-----------------|-------------|
| 101111 00000000 | pause       |
| 101111 00000010 | playing     |
| 111111 00000110 | looping     |

there's some pattern to be observed:
- the 2nd (least significant) bit is set when the state is playing
- the 3rd and the 13th bit are set when looping
- bits 4-8 and 1 are always 0
- bits 9-12 and 14 are always 1


Maybe these are `flags` that are somehow related to the transport state of the system?
Let's do some more tests:

| binary            | description   |
|-------------------|---------------|
| 00101111 00001010 | recording     |
| 00101111 00000001 | stopping      |
| 00101111 00000011 | starting play |
| 00111111 00000111 | starting loop |


The stopping/playing states can only be observed for a single time frame,
whenever we start (resp. stop) playback
E.g. if we go from *paused* to *playing* (unlooped),
we first see `10111100000000` (while the system is paused),
when we press the space-bar (to start playing) there's a single time frame
showing `10111100000011`, and then we see `11111100000111` while the system plays.

So far we got:
- when starting/stopping playback the 1st (least significant) bit is set.
- the 2nd bit is set when the (target) state is playing (looped or not)
- the 3rd and 13th bits are set when looping
- the 4th bit is set when recording

If we skim through the list of contants for values that might be related to the transport state,
we find 4 constants starting with `kVstTransport*`, that map neatly to the observed bits:

| flag                       | value |
|----------------------------|-------|
| `kVstTransportChanged`     | 1<<0  |
| `kVstTransportPlaying`     | 1<<1  |
| `kVstTransportCycleActive` | 1<<2  |
| `kVstTransportRecording`   | 1<<3  |

That leaves us with two problems though: we still have the 13th bit that is also
related to looping, and we have the 8 `kVst*Valid` values and 2 `kVstAutomation*`, that are supposed to be
in the `VstTimeInfo.flags` field.

There's the `kVstCyclePosValid` flag, that might occupy bit #13.
JUCE will always assign either both `kVstCyclePosValid` and `kVstTransportCycleActive` or none.
We have put the `kVstTransportCycleActive` in the group of the 4 least-significant bits,
because there is it surrounded by other `kVstTransport*` bits.

Most of the double-values in the `VstTimeInfo` struct made sense so far, although we haven't really
found the fields for *SMPTE* yet; also the `nanoSeconds` field seems to be quantized to seconds.
So I am not really sure whether `kVstNanosValid` and `kVstSmpteValid` are really set.

To find out some more values, we can use a different VST-Host - `MrsWatson` - that uses the
`ivalue` field of the callback (using the same flags as the `VstTimeInfo.flags` field)
to determine which fields of the `VstTimeInfo` struct it fills.

Iterating over etting one bit after the other while asking for the time
(in the `process` callback),
we get the following errors/warnings:

| ivalue | binary | warning                                |
|--------|--------|----------------------------------------|
| 0x0100 | 1<< 8  | "plugin asked for time in nanoseconds" |
| 0x4000 | 1<<14  | "Current time in SMPTE format"         |
| 0x8000 | 1<<15  | "Sample frames until next clock"       |


A few other `ivalue`s seem to enable the setting specific members:

| ivalue | binary | set data                            |
|--------|--------|-------------------------------------|
| 0x0200 | 1<< 9  | ppqPos                              |
| 0x0400 | 1<<10  | tempo                               |
| 0x0800 | 1<<11  | barStartPos                         |
| 0x2000 | 1<<13  | timeSigNumerator/timeSigDenominator |

Which gives us the following values:

| flag                       | value |
|----------------------------|-------|
| `kVstTransportChanged`     | 1<< 0 |
| `kVstTransportPlaying`     | 1<< 1 |
| `kVstTransportCycleActive` | 1<< 2 |
| `kVstTransportRecording`   | 1<< 3 |
|----------------------------|-------|
| `kVstNanosValid`           | 1<< 8 |
| `kVstPpqPosValid`          | 1<< 9 |
| `kVstTempoValid`           | 1<<10 |
| `kVstBarsValid`            | 1<<11 |
| `kVstCyclePosValid`        | 1<<12 |
| `kVstTimeSigValid`         | 1<<13 |
|----------------------------|-------|
| `kVstAutomationReading`    | ??    |
| `kVstAutomationWriting`    | ??    |

This also means, that the `VstTimeInfo.flags` field really is at position @54-57.

The `kVstAutomation*` flags are unknown, but probably occupy bits 4, 5, 6 or 7
(the names sound as if they were related to system state, like the `kVstTransport*` flags
that occupy the lowbyte;
as opposed to the flags related to timestamp validity, in the highbyte.)


## MIDI out
If a plugin wants to send out MIDI data, it needs to pass the MIDI events back to the host.
The `audioMasterProcessEvents` opcode looks like a likely candidate for this task.

We had to tell the host that we want to receive MIDI data by responding positively
to the `effCanDo` requests with `receiveVstEvents` and `receiveVstMidiEvent`.
Likely, if we want to *send* MIDI data, we ought to respond positively to the
question whether we can do `sendVstEvents` and `sendVstMidiEvent`.

To find out the value of `audioMasterProcessEvents`, we try to find an opcode
that can read a valid `VstEvents` struct, but crashes with an invalid struct.

In a first attempt, whenever REAPER calls our plugin with `effProcessEvents`,
we create a new `VstEvents` struct, filled with a single `VstMidiEvent`.
We then try a number of opcodes (starting with *3*, because we already know that
opcodes 0..2 have different purposes; ending arbitrary at 30 for now)

~~~
  VstEvents*events = (VstEvents*)calloc(1, sizeof(VstEvents)+sizeof(VstEvent*));
  VstMidiEvent*event=(VstMidiEvent*)calloc(1, sizeof(VstMidiEvent));
  events->numEvents = 1;
  events->events[0]=(VstEvent*)event;
  event->type = kVstMidiType;
  event->byteSize = sizeof(VstMidiEvent);
  event->midiData[0] = 0x90;
  event->midiData[0] = 0x40;
  event->midiData[0] = 0x7f;
  event->midiData[0] = 0x0;

  for(size_t opcode=3; opcode<30; opcode++)
    dispatch(effect, opcode, 0, 0, events, 0.f);

  // free the event/events structures!
~~~

The first run doesn't show much, except that REAPER doesn't crash with any of the tried opcodes.
So now we taint the passed structure, by using an invalid address for the first event:

~~~
  events->events[0]=(VstEvent*)0x1;
~~~

If the host tries to access the `VstEvent` at the invalid position, it should segfault.
The address is non-null, as the host might do a simple check about the validity of the pointer.

If we iterate again over the various opcodes, we see that it crashes at `opcode:8`
with a segmentation fault.

So probably the value of `audioMasterProcessEvents` is *8*.

We can confirm this, by connecting REAPER's MIDI output a MIDI monitor (e.g. `gmidimonitor`),
to see that it reports (after getting the MIDI routing right):

> Note on, E, octave 4, velocity 127

whenever we send a MIDI message {0x90, 0x41, 0x7f, 0} (via a *valid* `VstEvents` structure)
to `opcode:8`: Q.E.D!

## effMainsChanged, audioMasterWantMidi and audioMasterGetCurrentProcessLevel
REAPER calls `effcode:12` twice (with varying `ivalue`) whenever playback gets started:

~~~
FstClient::dispatcher(0x39166c0, 12, 0, 0, (nil), 0.000000);
FstClient::dispatcher(0x39166c0, 12, 0, 1, (nil), 0.000000);
~~~

Looking up [JUCE's effect opcodes](#juce-effect-opcodes), the following opcodes could be called:
- `effMainsChanged`
- `effSetBypass`
- `effSetProcessPrecision`

The `effSetBypass` can probably be ruled out, as the host wouldn't *enable* bypassing just before starting playback.


If we use play back this sequence with effcode:12 to our test plugins,
most of them respond immediately by calling the host callback:


| plugin                | response to 12 |
|-----------------------|----------------|
| Digits                | 6              |
| BowEcho/Danaides      | 23+6           |
| hypercyclic/tonespace | 23+7+6         |

We already know that the `hostcode:7` (as called by hypercyclic/tonespace) is `audioMasterGetTime`.

When looking at how JUCE actually handles the two remaining effect opcodes, we see that `effSetProcessPrecision`
only sets some internal state, leaving little chance to call back to the host.

That leaves us with `effMainsChanged`: and indeed, JUCE potentially does callbacks to the host
when `resume()`ing operation, which is done when handling this opcode with `ivalue=1`
- it calls `audioMasterGetCurrentProcessLevel` without arguments (indirectly, via the `isProcessLevelOffline()` method)
- if the plugin somehow handles MIDI, it will also issue an `audioMasterWantMidi` call (with `ivalue=1`)

Checking how *BowEcho* actually calls the opcodes 23+6, we can log:

~~~
FstHost::dispatcher(23, 0, 0, (nil), 0.000000);
FstHost::dispatcher(7, 0, 1, (nil), 0.000000);
~~~

And *hypercyclic*:

~~~
FstHost::dispatcher(23, 0, 0, (nil), 0.000000);
FstHost::dispatcher(audioMasterGetTime, 0, 65024, (nil), 0.000000);
FstHost::dispatcher(7, 0, 1, (nil), 0.000000);
~~~

So the calling conventions kind of match the expectations we have for `audioMasterGetCurrentProcessLevel` and `audioMasterWantMidi`,
giving us three new opcodes:

| opcode                            | value |
|-----------------------------------|-------|
| effMainsChanged                   | 12    |
| audioMasterWantMidi               | 6     |
| audioMasterGetCurrentProcessLevel | 23    |


## audioMasterCanDo
We already know that we can use `effCanDo` to query a plugin whether it supports a
feature given in a string.
There's also an `audioMasterCanDo` opcode that is supposed to work in the other direction.
According to the JUCE sources, typical feature strings are
`supplyIdle`, `sendVstEvents` and `sendVstMidiEvent`.

To find the value of the `audioMasterCanDo` opcode, we iterate a range of opcodes
while providing a `ptr` to a valid (and likely supported) feature string (like `sendVstEvents`),
and record the return values.
After that, we iterate over the same opcodes with some randomly made up (and therefore
likely unsupported) feature string (like `fudelDudelDei`).
Finally we compare the return values of the two iterations and find that they always returned
the same result *except* for `opcode:37`, our likely candidate for `audioMasterCanDo`.

## Speaker Arrangements

Running our test plugin in REAPER, we can also calls to `effcode:42` in the startup phase.

~~~
FstClient::dispatcher(0x1ec2080, 42, 0,  32252624, 0x1ec2740, 0.000000);
~~~

in another run this is:
~~~
FstClient::dispatcher(0x9e36510, 42, 0, 172519840, 0xa487610, 0.000000);
~~~

The `ivalue` is a bit strange, unless it is printed in hex (`0x1ec22d0` resp . `0xa4871a0`),
where it becomes apparent that this is really another address (just compare the hex representation
with the `ptr` value; there difference is 1136, which is practically nothing in address space)!

According to [JUCE](#juce-effect-opcodes), there are only very few opcodes
where both `ivalue` and `ptr` point both to addresses:
- `effGetSpeakerArrangement`
- `effSetSpeakerArrangement`

Both opcodes get pointers to `VstSpeakerArrangement`.

Here is a dump of the first 96 bytes of the data found at those addresses
(the data is the same on both addresses,
at least if our plugin is configured with 2 IN and 2 OUT channels):
~~~
01 00 00 00 02 00 00 00  00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00  01 00 00 00 00 00 00 00
~~~

The int32 at position @4-7 is the `numChannels`,
the int32 at position @0-3 is most likely the `type`.
I have no explanation for the value at position @58,
it's probably just uninitialized data.

By setting the `numChannels` of the plugin, REAPER responds
with following types

| numChannels  | type | type(hex)  |
|--------------|------|------------|
| 1            | 0    | 0x0        |
| 2            | 1    | 0x1        |
| 3            | 11   | 0xb        |
| 4            | 11   | 0xb        |
| 5            | 15   | 0xf        |
| 6            | 15   | 0xf        |
| 7            | 23   | 0x17       |
| 8            | 23   | 0x17       |
| 12           | 28   | 0x1C       |
| all the rest | -2   | 0xFFFFFFFE |

Since the values are filled in by REAPER, it's likely that `effcode:42` is `effSetSpeakerArrangement`.
This is somewhat confirmed by *Protoverb*, that prints out

>     resulting in SpeakerArrangement $numIns - $numOuts

with $numIns and $numOuts replaced by 0, 1 or 2, depending on the chosen speaker arrangement.
It seems that *Protoverb* doesn't support more than 2 channels.


`effGetSeakerArrangement` is most likely close by (`41` or `43`),
A simple test would first set the speaker-arrangement, and then try to query it back.
According to JUCE, the plugin should return `1` in case of success.
The calling convention is slightly different from `effSetSpeakerArrangement`, as `ptr` and `ivalue`
hold addresses of pointer-sized memory regions, where the plugin is supposed to write
the addresses of the `VstSpeakerArrangement` structs to (cf. JUCE code).

~~~C
for(size_t opcode=40; opcode<45; opcode++) {
  VstSpeakerArrangement *arrptr[2] = {0,0};
  if(42 == opcode)continue;
  dispatch_v(effect, opcode, 0, (VstIntPtr)(arrptr+0), (arrptr+1), 0.f);
  print_hex(arrptr[0], 32);
  print_hex(arrptr[1], 32);
}
~~~

Unfortunately, this is not very successful.
Only `opcode:44` returns 1 for *some* plugins, but none write data into our `VstSpeakerArrangement` struct.

| plugin    | opcode | result |
|-----------|--------|--------|
|Danaides   | 44     | 1      |
|BowEcho    | 44     | 1      |
|hypercyclic| 44     | 1      |
|tonespace  | 44     | 1      |
|Protoverb  | *      | 0      |
|Digits     | *      | 0      |
|InstaLooper| *      | 0      |


If we try a bigger range of opcodes (e.g. 0..80), we need to skip the opcodes 45, 47 and 48 (`effGet*String`)
to prevent crashes.

Interestingly, *Protoverb* will now react to `opcode:69`, returning the same data we just set via opcode:42.
So we probably have just found `effGetSeakerArrangement` as well.

### Speaker Arrangement Types

So what do we know about the `VstSpeakerArrangement.type` field?
In [VstSpeakerArrangement](#vstspeakerarrangement), we found out that this
member is really of type `t_fstSpeakerArrangementType`, that has (so far) symbolic names starting with `kSpeakerArr*`.
JUCE uses the following names matching this pattern (in a regex notation):

| VST name                             | JUCE name    |
|--------------------------------------|--------------|
| `kSpeakerArrEmpty`                   | disabled     |
| `kSpeakerArrMono`                    | mono         |
| `kSpeakerArrStereo`                  | stereo       |
| `kSpeakerArrStereoCLfe`              |              |
| `kSpeakerArrStereoCenter`            |              |
| `kSpeakerArrStereoSide`              |              |
| `kSpeakerArrStereoSurround`          |              |
| `kSpeakerArrUserDefined`             |              |
| `kSpeakerArr[34678][01](Cine,Music)` |              |
| `kSpeakerArr30Cine`                  | LCR          |
| `kSpeakerArr30Music`                 | LRS          |
| `kSpeakerArr40Cine`                  | LCRS         |
| `kSpeakerArr60Cine`                  | 6.0          |
| `kSpeakerArr61Cine`                  | 6.1          |
| `kSpeakerArr60Music`                 | 6.0Music     |
| `kSpeakerArr61Music`                 | 6.1Music     |
| `kSpeakerArr70Music`                 | 7.0          |
| `kSpeakerArr70Cine`                  | 7.0SDDS      |
| `kSpeakerArr71Music`                 | 7.1          |
| `kSpeakerArr71Cine`                  | 7.1SDDS      |
| `kSpeakerArr40Music`                 | quadraphonic |
| `kSpeakerArr50`                      | 5.0          |
| `kSpeakerArr51`                      | 5.1          |
| `kSpeakerArr102`                     |              |


Comparing these names to [what REAPER fills in for various channel counts](#speaker-arrangements),
we can at least make some simple guesses.
We repeat the table from above:

| numChannels  | type |
|--------------|------|
| 1            | 0    |
| 2            | 1    |
| 3            | 11   |
| 4            | 11   |
| 5            | 15   |
| 6            | 15   |
| 7            | 23   |
| 8            | 23   |
| 12           | 28   |
| all the rest | -2   |

Mono is a single channel, Stereo needs two channels, for which REAPER fills in `0` and `1` (`kSpeakerArrMono` resp `kSpeakerArrStereo`).
That's a bit unconventional: personally I would have used `0` for `kSpeakerArrEmpty`, and `1` and `2` for Mono and Stereo.

Unfortunately, REAPER doesn't report anything for 0 channels, so we don't know that value of `kSpeakerArrEmpty`.
I also don't really understand why we always get pairwise assignments for the next 6 number of channels.
E.g. assigning `11` to both 3 and 4 channels would make sense
if there was a speaker-arrangement for 4 channels (e.g. quadrophony aka `kSpeakerArr40Music`),
but none for 3 channels (so REAPER just assumes some "degraded" arrangement).
But we do have `kSpeakerArr30Music`, which should serve 3 channels just fine.
It's probably safe to assume that REAPER does the "degraded" arrangement thing nevertheless.
It's tricky to get the correct assignment though, since we have so many variants with the same
number of channels.
E.g. 6 channels could be `kSpeakerArr51`, `kSpeakerArr60Music` and `kSpeakerArr60Cine`.

There's also the catch-all type *-2*. Given the `kSpeakerArr*` names we know so far, `kSpeakerArrUserDefined` might be this catch-all.
The value for `kSpeakerArrEmpty` might be "special" as well (and therefore not be assigned some ordered value like, say, *7*) -
it could well be *-1*, but we don't have any evidence of that yet.

Here's some possible assignments (the names for 3, 5 & 7 channels are in parentheses, as the type has the same value as arrangement with one more channel):

| numChannels  | type (value) | type (name)                                                                            |
|--------------|--------------|----------------------------------------------------------------------------------------|
| 1            | 0            | `kSpeakerArrMono`                                                                      |
| 2            | 1            | `kSpeakerArrStereo`                                                                    |
| 3            | 11           | (`kSpeakerArr30Music`, `kSpeakerArr30Cine`)                                            |
| 4            | 11           | `kSpeakerArr31Music`, `kSpeakerArr31Cine`, `kSpeakerArr40Music`, `kSpeakerArr40Cine`   |
| 5            | 15           | (`kSpeakerArr41Music`, `kSpeakerArr41Cine`, `kSpeakerArr50Music`)                      |
| 6            | 15           | `kSpeakerArr51`, `kSpeakerArr60Music`, `kSpeakerArr60Cine`                             |
| 7            | 23           | (`kSpeakerArr61Music`, `kSpeakerArr61Cine`, `kSpeakerArr70Music`, `kSpeakerArr70Cine`) |
| 8            | 23           | `kSpeakerArr71Music`, `kSpeakerArr71Cine`, `kSpeakerArr80Music`, `kSpeakerArr80Cine`   |
| 12           | 28           | `kSpeakerArr102`                                                                       |
| all the rest | -2           | `kSpeakerArrUserDefined`                                                               |

So we can at least be pretty confident of the values of `kSpeakerArrMono`, `kSpeakerArrStereo`,
`kSpeakerArr102` & `kSpeakerArrUserDefined`.


### Amendment: wantsChannelCountNotifications

REAPER doesn't *always* call the `effSetSpeakerArrangement`.
It seems the plugin must return `1` for the `effCanDo` opcode with a value of `wantsChannelCountNotifications`
in order to receive this opcode (which kind of makes sense).


# Part: AudioPluginHost
With many opcodes working, we can start testing on a larger scale.

A good start is by compiling some slighly larger application ourself, e.g. the *AudioPluginHost* that comes with JUCE.

Once started we can load the *Protoverb* plugin, to see that a number of yet unknown opcodes are called in both directions:

~~~ C
host2plugin(effKeysRequired, 0, 0, (nil), 0.000000);
host2plugin(effCanBeAutomated, 0, 0, (nil), 0.000000);
host2plugin(effGetPlugCategory, 0, 0, (nil), 0.000000);
host2plugin(effStartProcess, 0, 0, (nil), 0.000000);
host2plugin(effStopProcess, 0, 0, (nil), 0.000000);
host2plugin(effConnectInput, 0, 1, (nil), 0.000000);
host2plugin(effConnectOutput, 0, 1, (nil), 0.000000);
[...]
plugin2host(13, 0, 0, (nil), 0.000000);
plugin2host(15, 640, 575, (nil), 0.000000);
plugin2host(42, 0, 0, (nil), 0.000000);
plugin2host(43, 3, 0, (nil), 0.000000)
plugin2host(44, 3, 0, (nil), 0.000000)
~~~

## audioMasterSizeWindow, audioMasterBeginEdit, audioMasterEndEdit

A low hanging fruit is the `hostCode:15` which is called with `index:640` and `ivalue:575`.
Esp. the `index` looks suspiciously like some image/window dimension, which leads us to
the `audioMasterSizeWindow` opcode.

Another one is `hostCode:43` (and it's twin `hostCode:44`), which is called whenever a controller
is changed in the plugin's GUI: `hostCode:43` is issued when a fader/... is "touched", and
`hostCode:44` is called when it's released. The `index` parameter changes with the controller
(so it's likely the *parameter index*).
In the [JUCE host opcode table](#juce-host-opcodes), there's the `audioMasterBeginEdit` and `audioMasterEndEdit`
pair that fits nicely.

| opcode                |    |
|-----------------------|----|
| audioMasterSizeWindow | 15 |
| audioMasterBeginEdit  | 43 |
| audioMasterEndEdit    | 44 |

## effStartProcess/effStopProcess
*AudioPluginHost* calls `effStartProcess` right after the plugin has been initialised
(and before it starts calling the `process` callbacks).
When shutting down *AudioPluginHost* the `effStopProcess` opcode is called
(after the last call to the `process` callback).
We can probably safely assume, that the opcodes will be close to each other (adjacent numbers).

Comparing this to what *REAPER* does when calling our fake plugin,
we notice that there are the opcodes `71` and `72` that are used in a similar way.

Most likely these two numbers are indeed the `eff*Process` opcodes:

| opcode          |    |
|-----------------|----|
| effStartProcess | 71 |
| effStopProcess  | 72 |


# Part: more enums

## effVendorSpecific

that one is weird.
If we compile a plugin based on JUCE and load it into REAPER,
we notice tat REAPER sends `opcode:50` (with `index:45` and `ivalue:80`)
once the FX window (with the effect) is opened for a track.

    plugin::dispatcher(50, 45, 80, 0x7ffe849f12f0, 0.000000)

In the dump of `ptr` we see two addresses at @58 and @60,
but it seems there are no doubles, int32 or strings in the data.
The first 8 bytes are 0, so *maybe* the plugin is supposed to write some
data (pointer) into this.

What's more, REAPER does *not* send this opcode,
when we create a plain (none-JUCE based) plugin (even if it has the `effFlagsHasEditor` set).
So the JUCE plugins must do something to provoke REAPER to send this opcode:

If we hack our JUCE-based plugin to print out each opcode sent from REAPER
before it starts sending `opcode:50` (because obviously REAPER has to know that
it can send this opcode before it starts doing so), we get the following
(on the right side we see what the plugin returns for each command):

~~~ C
// initialisation
host2plugin(effOpen, 0, 0, NULL, 0.000000)                               => 0
host2plugin(effSetSampleRate, 0, 0, NULL, 44100.000000)                  => 0
host2plugin(effSetBlockSize, 0, 512, NULL, 0.000000)                     => 0
host2plugin(effGetEffectName, 0, 0, <out>, 0.000000)                     => 1
host2plugin(effGetVendorString, 0, 0, <out>, 0.000000)                   => 1
host2plugin(effCanDo, 0, 0, "hasCockosNoScrollUI", 0.000000)             => 0
host2plugin(effCanDo, 0, 0, "wantsChannelCountNotifications", 0.000000)  => 0
host2plugin(effCanDo, 0, 0, "hasCockosExtensions", 0.000000)             => 0xBEEF0000
host2plugin(effGetVstVersion, 0, 0, NULL, 0.000000)                      => 2400
host2plugin(effMainsChanged, 0, 1, NULL, 0.000000)                       => 0
host2plugin(effStartProcess, 0, 0, NULL, 0.000000)                       => 0
host2plugin(effCanDo, 0, 0, "receiveVstEvents", 0.000000)                => 0xFFFFFFFF
host2plugin(effCanDo, 0, 0, "receiveVstMidiEvent", 0.000000)             => 0xFFFFFFFF
host2plugin(35, 0, 0, NULL, 0.000000)                                    => 0
host2plugin(effCanDo, 0, 0, "sendVstEvents", 0.000000)                   => 0xFFFFFFFF
host2plugin(effCanDo, 0, 0, "sendMidiVstEvents", 0.000000)               => 0xFFFFFFFF
host2plugin(35, 0, 0, NULL, 0.000000)                                    => 0
host2plugin(effGetProgram, 0, 0, NULL, 0.000000)                         => 0
host2plugin(effGetChunk, 0, 0, <out>, 0.000000)                          => 0
host2plugin(effGetProgramName, 0, 0, <out>, 0.000000)                    => 0
host2plugin(effCanDo, 0, 0, "cockosLoadingConfigAsParameters", 0.000000) => 0
host2plugin(effGetProgram, 0, 0, NULL, 0.000000)                         => 0
host2plugin(effGetChunk, 0, 0, <out>, 0.000000)                          => 0
host2plugin(effGetProgramName, 0, 0, <out>, 0.000000)                    => 0
// initialisation done; if we now open the FX window we get:
host2plugin(50, 45, 80, <ptr>, 0.000000)
/* [...] */
~~~

So we create a fake plugin (*not* JUCE-based!) that responds to most opcodes with `0`
and doesn't really do anything.
In order to make REAPER do the same as with JUCE-plugins (that is: send an `opcode:50` command),
we must mimic the behaviour of the JUCE plugin (making REAPER think it is really the same).

We do this incrementally:
~~~C
VstIntPtr dispatcher(AEffect*eff, t_fstInt32 opcode, int index, VstIntPtr ivalue, void* const ptr, float fvalue) {
    switch(opcode) {
    case effGetVstVersion:
        return 2400;
    case effGetVendorString:
        snprintf((char*)ptr, 16, "SuperVendor");
        return 1;
    case effGetEffectName:
        snprintf((char*)ptr, 16, "SuperEffect");
        return 1;
    default:
        break;
    }
    return 0;
}
~~~

There's no real effect here (which is expected; REAPER would be really weird if it changed behaviour based on effect *names*).

So let's add some more opcodes:

~~~C
    case effCanDo:
        if(!strcmp((char*)ptr, "receiveVstEvents"))
            return 0xFFFFFFFF;
        if(!strcmp((char*)ptr, "receiveVstMidiEvents"))
            return 0xFFFFFFFF;
        if(!strcmp((char*)ptr, "sendVstEvents"))
            return 0xFFFFFFFF;
        if(!strcmp((char*)ptr, "sendVstMidiEvents"))
            return 0xFFFFFFFF;
        if(!strcmp((char*)ptr, "hasCockosExtensions"))
            return 0xBEEF0000;
        return 0;
~~~

Oha! Suddenly REAPER sends plenty of `opcode:50` commands:

~~~
host2plugin(50, 0xDEADBEF0,  0, 0x7ffe7aa23c30, 0.000000);
host2plugin(50, 45,         80, 0x7ffe7aa23a50, 0.000000);
host2plugin(50, 0xDEADBEF0,  0, 0x7ffe7aa23d10, 0.000000);
host2plugin(50, 45,         80, 0x7ffe7aa23b30, 0.000000);
host2plugin(50, 7,           0, 0x7ffe7aa33f60, 0.500000);
host2plugin(50, 0xDEADBEF0,  0, 0x7f3c85f32150, 0.000000);
~~~

Disabling the `effCanDo` features selectively, it becomes apparent that REAPER
will start sending this opcode only if we reply to `hasCockosExtensions` with `0xBEEF0000`.

Now "Cockos" is the name of the REAPER *vendor*, and `opcode:50` is only sent if
we claim to support the *Cockos* extensions.
*Maybe* this is a *vendor specific* opcode, e.g. `effVendorSpecific`?.
This idea is supported by the neighbouring opcodes: e.g. `opcode:49` is `effGetVendorVersion`.

Also a quick internet search for `effVendorSpecific+hasCockosExtensions` directs us to
the [Cockos Extensions to VST SDK](https://www.reaper.fm/sdk/vst/vst_ext.php).
One of the extensions documented there is

~~~
dispatcher(effect,effVendorSpecific,effGetEffectName,0x50,&ptr,0.0f);
~~~

We already know that `effGetEffectName` is *45*, and `0x50` is *80*,
and *that* is exactly how REAPER is using `opcode:50`


## effSetProcessPrecision

When running a plugin in a self-compiled JUCE `AudioPluginHost`,
almost all effCodes our host would like to call in a plugin, have a proper value.

The exceptions are:

~~~
effCanBeAutomated
effConnectInput
effConnectOutput
effGetPlugCategory
effSetProcessPrecision
~~~

Let's tackle those.

According to *juce_VSTPluginFormat.cpp*, the `effSetProcessPrecision` is called,
if the plugin supports double-precision. It is then called with `kVstProcessPrecision64`
resp `kVstProcessPrecision32` (depending on whether the host wants to do single or double
precision.)

Maybe REAPER does something similar, so let's check.
One of the few remaining opcodes that REAPER calls in our simple plugin
and that have non-0 arguments (namely `ivalue:1`), is `effcode:77`:

~~~
dispatcher(effect, 77, 0, 1, (nil), 0.000000);
~~~

If we compile our plugin without double-precision support (`eff->flags &= ~effFlagsCanDoubleReplacing`),
`opcode:77` does not get called!
So it seems that indeed `effSetProcessPrecision` is `77`.

Now the `ivalue` is supposed to be `kVstProcessPrecision64` or `kVstProcessPrecision32`.
REAPER uses double precision (we know that because the `processDoubleReplacing` callback is called with audio data),
and uses `ivalue:1`, so that is most likely the value of  `kVstProcessPrecision64`.

The value of `kVstProcessPrecision32` can only be guessed. Intuitively, I would suggest `0`
(so the opcode could be used as a boolean-like on/off switch for double precision rendering)

| opcode                 | value |
|------------------------|-------|
| effSetProcessPrecision | 77    |
|------------------------|-------|
| kVstProcessPrecision32 | 0 (?) |
| kVstProcessPrecision64 | 1     |

## effGetPlugCategory

The `effGetPlugCategory` is closely related to the following constants,
which a call to this opcode is supposed to return:

~~~
kPlugCategEffect
kPlugCategSynth
kPlugCategAnalysis
kPlugCategMastering
kPlugCategSpacializer
kPlugCategRoomFx
kPlugSurroundFx
kPlugCategRestoration
kPlugCategGenerator

kPlugCategShell
~~~

REAPER offers a categorization of plugins that includes categories like
`Delay`, `Surround`, `Synth`, `Tone Generator`...
While the categories are not the same, there is a significant overlap (and of course the title "Categories")
which makes me think that the two are related.
However, so far none of our plugins (neither our self-created plugins, nor the commercially available ones we use for testing)
is categorised at all.

Anyhow: once we have the `effGetPlugCategory` it should be pretty easy to find out the `kPlugCateg` values by
iterating over values `0..n` and see how REAPER categorizes the plugin.
But how do we get the opcode?

Somebody in the [Cockos forum](https://forum.cockos.com/archive/index.php/t-165245.html) (accessed 2020-01-20)
mentioned, that REAPER would call this opcode when *scanning* for plugins, pretty soon after
issuing `effOpen`:

~~~
effOpen
// ...looks at various elements of AEffect
effGetPlugCategory
effGetVendorString
[...]
~~~

That was a nice hint, because issuing a `Clear cache/re-scan` in REAPER's Preferences gives us:

~~~
dispatch(effOpen, 0, 0, (nil), 0.000000);
dispatch(35, 0, 0, (nil), 0.000000);
dispatch(effGetVendorString, 0, 0, 0xF00, 0.000000);
dispatch(effGetEffectName,   0, 0, 0xBA8, 0.000000);
dispatch(effClose, 0, 0, (nil), 0.000000);
~~~

... suggesting that `effGetPlugCategory` really is `35`.

So I tried returning a non-0 value on this opcode (starting with `1`), but the plugin is still not categorised :-(.
But let's try other values:
- returning `2` suddenly categorises the plugin as *Synth*. This is promising
(if it weren't for the `effFlagsIsSynth` flag that might allow the same)!
- continuing to return different values, we see that `3` would make REAPER show the plugin in the (newly created) *Analysis* category.
Now it seems like we are indeed on the right track.

Playing around we get the following REAPER categories for various return values:


| return | category       |
|--------|----------------|
| 2      | Synth          |
| 3      | Analysis       |
| 4      | Mastering      |
| 5      | Spatial        |
| 6      | Room           |
| 7      | Surround       |
| 8      | Restoration    |
| 11     | Tone Generator |

The return values `1` and `9` (and `0`) leave the plugin in the previous category (whichever that was),
so it seems these return values are ignored by REAPER and the host just uses some cached values.
If the plugin returns `10`, it is *not listed* at all in the list of available plugins.

The REAPER categories match nicely with the `kPlugCateg*` constants we need to find:

| name                    | value |
|-------------------------|-------|
| `kPlugCategSynth`       | 2     |
| `kPlugCategAnalysis`    | 3     |
| `kPlugCategMastering`   | 4     |
| `kPlugCategSpacializer` | 5     |
| `kPlugCategRoomFx`      | 6     |
| `kPlugSurroundFx`       | 7     |
| `kPlugCategRestoration` | 8     |
| `kPlugCategGenerator`   | 11    |


We haven't found any values for `kPlugCategEffect` and `kPlugCategShell` yet.

Looking at *juce_VSTPluginFormat.cpp* there are two notable things:
- `kPlugCategEffect` is the first in a `switch/case` group, that is pretty much ordered in the very same way as the
`kPlugCateg*` values we already know (`kPlugCategShell` is missing from that `switch/case` group)
- if the category is `kPlugCategShell`, JUCE will call the `effShellGetNextPlugin` opcode repeatedly
  to get *shellEffectName*s for the "shell plugin's subtypes".

The first observation *might* hint that the value for `kPlugCategEffect` is `1`.

The second observation can be combined with another obersvation: when a plugin returns `10` for `opcode:35`,
REAPER will later issue `opcode:70` instead of `effGetEffectName`.
So `kPlugCategShell` would be `10`, and `effShellGetNextPlugin` would be `70`.

We can confirm this assumption by returning the proper values for the opcodes `35` and `70`:

~~~C
    switch(opcode) {
    case 35:
        return 10;
    case 70: {
        static int count = 0;
        count++;
        if(count < 5) {
            snprintf(ptr, 128, "shell%d", count);
            return count;
        }
    }
        return 0;
~~~

... which will give use 4 plugins `shell1`..`shell4` in REAPER.

So to conclude, we have the following new values:

| name                    | value |
|-------------------------|-------|
| `effGetPlugCategory`    | 35    |
| `effShellGetNextPlugin` | 70    |
|-------------------------|-------|
| `kPlugCategEffect`      | 1?    |
| `kPlugCategSynth`       | 2     |
| `kPlugCategAnalysis`    | 3     |
| `kPlugCategMastering`   | 4     |
| `kPlugCategSpacializer` | 5     |
| `kPlugCategRoomFx`      | 6     |
| `kPlugSurroundFx`       | 7     |
| `kPlugCategRestoration` | 8     |
| ??                      | 9     |
| `kPlugCategShell`       | 10    |
| `kPlugCategGenerator`   | 11    |


Scanning through the JUCE sources for strings like `kPlugCategEffect`, we find a list of
strings in the *Projucer* sources (`jucer_Project.cpp`):

- `kPlugCategUnknown` (!)
- `kPlugCategEffect`
- `kPlugCategSynth`
- `kPlugCategAnalysis`
- `kPlugCategMastering`
- `kPlugCategSpacializer`
- `kPlugCategRoomFx`
- `kPlugSurroundFx`
- `kPlugCategRestoration`
- `kPlugCategOfflineProcess` (!)
- `kPlugCategShell`
- `kPlugCategGenerator`

So there's two more categories ("Unknown" and "OfflineProcess").

If we compare the list (in the same order as found in the Projucer sources) with the values we already found above,
we see that they align nicely.
`kPlugCategOfflineProcess` just falls between `kPlugCategRestoration` and `kPlugCategShell`,
where we haven't found a name for the value *9* yet.
Sorting `kPlugCategUnknown` before `kPlugCategEffect` (aka *1*) would give us *0*, which also makes some sense:

| name                       | value |
|----------------------------|-------|
| `kPlugCategUnknown`        | 0     |
| `kPlugCategOfflineProcess` | 9     |


## effGetCurrentMidiProgram / effSetTotalSampleToProcess
It's starting to get harder harvesting new opcodes.

We can do another attempt at sending yet-unknown opcodes to our
test plugins and note those were the plugins return non-0 values.

Something like:

~~~
static int effKnown(VstIntPtr opcode) {
  if(opcode>=100000)
    return 0;
  switch(opcode) {
    case effCanBeAutomated:
    case effCanDo:
    case effClose:
    case effConnectInput:
    case effConnectOutput:
    /* continue for all known opcodes */
    // ...
    case effVendorSpecific:
      return 1;
    default break;
  }
  return 0;

void test(AEffect*effect) {
  int index = 0;
  VstIntPtr ivalue = 0;
  dispatch(effect, effOpen, 0, 0, 0, 0.0);
  for(size_t opcode=2; opcode<128; opcode++)
    if(!effKnown(opcode))
       dispatch(effect, opcode, index, ivalue, 0, 0.f);
  dispatch(effect, effClose, 0, 0, 0, 0.0);
}
~~~

This gives us the following opcodes:

| value | return | plugins            |
|-------|--------|--------------------|
| 26    | 1      | ALL plugins        |
| 44    | 1      | JUCE-based plugins |
| 63    | -1     | ALL plugins        |
| 78    | 1      | Digits             |

If we set the `index` to something non-0 and compare the returned values, we don't notice any differences.
If we set the `ivalue` to something non-0 and compare the returned values, we get another opcode,
that seemingly returns the `ivalue` itself:

| value | return   | plugins     |
|-------|----------|-------------|
| 73    | <ivalue> | ALL plugins |

The [JUCE effect opcode table](#juce-effect-opcodes) lists only
two opcodes that return `-1` (and `effCanDo` is already known, leaving `effGetCurrentMidiProgram`).
It also has a single opcode that returns the `ivalue` as is, which is `effSetTotalSampleToProcess`, so we know have:

| opcode                       | value |
|------------------------------|-------|
| `effGetCurrentMidiProgram`   | 63    |
| `effSetTotalSampleToProcess` | 73    |


# effString2Parameter
The `effString2Parameter` is most likely used for setting a parameter via a string representation.
A simple routine to find the opcode value is to send a string with some numeric value to all unknown opcodes,
and read back the parameter (e.g. as a string representation via `effGetParamDisplay`).
If the read back parameter has changed (not necessarily to the value we sent it, but in relation to previous printouts),
we probably have found the `effString2Parameter` opcode:

~~~C
  dispatch_v(effect, effOpen, 0, 0, 000, 0.000000);
  for(VstIntPtr opcode=2; opcode<128; opcode++) {
    int index = 0;
    if(effKnown(opcode))
      continue;
    char buffer[1024];
    char outbuffer[1024];
    outbuffer[0] = buffer[0] = 0;
    snprintf(buffer, 1024, "0.666");
    dispatch_v(effect, opcode, index, 0, buffer, 0.);
    effect->dispatcher(effect, effGetParamDisplay, index, 0, outbuffer, 0.);
    if(*outbuffer)printf("param:%02d: %s\n", index, outbuffer);
  }
  dispatch_v(effect, effClose, 0, 0, 000, 0.000000);
~~~

Unfortunately for none of our test plugins
(*BowEcho*, *Danaides*, *Digits*, *InstaLooper*, *Protoverb*, *hypercyclic*, *tonespace*)
the read back value changes for any of the tested opcodes.

Luckily, a friend of mine pointed me to another set of free-as-in-beer plugins, the [GVST](https://www.gvst.co.uk/index.htm) set.

## kVstMaxVendorStrLen
Loading the above plugin in a fake host, we get immediate segfaults when trying to write vendor string,
using our estimate of `kVstMaxVendorStrLen` (197782).
Gradually lowering the maximum length of the vendor string, the first value where it doesn't crash is `130`.
This is a much more reasonable length thatn *197782*, although `128` would be even more plausible.
Lets used that last value for `kVstMaxVendorStrLen` (and `kVstMaxProductStrLen` as well).

## effString2Parameter
If we now try run the above snippet to test for `effString2Parameter`, the returned parameter value changes with
`opcode:27`:

~~~
dispatch(21, 0, 0, "0.666", 0.000000) => 0; param[0] = "10"
dispatch(26, 0, 0, "0.666", 0.000000) => 1; param[0] = "10"
dispatch(27, 0, 0, "0.666", 0.000000) => 1; param[0] = "1"
dispatch(28, 0, 0, "0.666", 0.000000) => 0; param[0] = "1"
~~~

We can also test with some bigger value in the string:
~~~
dispatch(21, 0, 0, "39", 0.000000) => 0; param[0] = "10"
dispatch(26, 0, 0, "39", 0.000000) => 1; param[0] = "10"
dispatch(27, 0, 0, "39", 0.000000) => 1; param[0] = "39"
dispatch(28, 0, 0, "39", 0.000000) => 0; param[0] = "39"
~~~

So we have learned that `effString2Parameter` is `27`.
(`opcode:26` returns `1`, like any other plugin we have seen so far.)

## effCanBeAutomated

We now have another set of plugins to test (the `GVST` set).
In order to make testing a bit easier, I wrote a little "proxy plugin" (see `src/FstProxy/`)
that takes an ordinary plugin file via the `FST_PROXYPLUGIN` environment variable.
When instantiating the proxy plugin, it will return an instance of the plugin
found in the `FST_PROXYPLUGIN` file, but will re-write the `AEffect.dispatcher` function
and the host-dispatcher (as passed to `VstPlugMain`), so we can add some additional
printout, to see how a real-world plugin communicates with a real-world host.

Something like the following:

~~~
static
VstIntPtr host2plugin (AEffect* effect, int opcode, int index, VstIntPtr ivalue, void*ptr, float fvalue) {
  AEffectDispatcherProc h2p = s_host2plugin[effect];
  char effbuf[256] = {0};
  printf("Fst::host2plugin(%s, %d, %ld, %p, %f)",
         effCode2string(opcode, effbuf, 255),
         index, ivalue, ptr, fvalue);
  VstIntPtr result = h2p(effect, opcode, index, ivalue, ptr, fvalue);
  printf(" => %ld\n", result);
  fflush(stdout);
  return result;
}
~~~

Unfortunately this doesn't reveal *very* much.
I discovered the following unknown effect opcodes:

~~~
Fst::host2plugin(19, 0, 0, (nil), 0.000000) => 0
Fst::host2plugin(26, 0, 0, (nil), 0.000000) => 1
Fst::host2plugin(26, 1, 0, (nil), 0.000000) => 1
Fst::host2plugin(53, 0, 0, (nil), 0.000000) => 0
Fst::host2plugin(56, 2, 0, 0xPTR, 0.000000) => 0
Fst::host2plugin(62, 0, 0, 0xPTR, 0.000000) => 0
Fst::host2plugin(66, 0, 0, 0xPTR, 0.000000) => 0
~~~

And the following unknown audioMaster opcodes:

~~~
Fst::plugin2host(13, 0, 0, (nil), 0.000000) -> 1
Fst::plugin2host(42, 0, 0, (nil), 0.000000) -> 1
~~~

On closer inspection, we see `effCode:26` is called with an index up to `AEffect.numParams`.
REAPER only calls this opcode, before opening a dialog window where we can select an parameter to
be automated.

Looking up the [JUCE table](#juce-effect-opcodes), there's only one effect opcode (we know of)
that takes an index (and which we don't know yet): `effCanBeAutomated`.
Since the opcode is called when REAPER does something with automation, it is even more likely that this is the correct name.

However, in our minimal fake plugin, this opcode doesn't really get called.
Probably we don't reply correctly to some prior opcode.

Anyhow, we can hack our proxy plugin, to respond to `effCode:26` with `1` only for indices less than (e.g.) `5`.
Doing that with a plugin that has more parameters (e.g. *GVST/GSinth* has 30 parameters),
REAPER will reduce the selection choice for parameters to be automated to the first five.

So indeed, `effCanBeAutomated` seems to have the value `26`.

So why does REAPER query this opcode for the *GSinth* plugin, but not for our own fake plugin?
Comparing the return values of the *GSinth* plugin with our own, there are not many differences.
However *GSinth* returns `2400` for the `effGetVstVersion` opcode, whereas our own plugin
returns the default `0`.
If we make it return `2400` as well, REAPER starts asking with `effCanBeAutomated`.
So it seems that this opcode was only introduced later, and requires the plugin to support a minimum version of the API.


# Speaker Arrangement revisited
We still haven't found out the details of the speaker arrangement structs.

So far, we know the `effGetSpeakerArrangement` and `effSetSpeakerArrangement` opcodes, which return (resp. take)
pointers to the `VstSpeakerArrangement`, which in turn includes an array of type `VstSpeakerProperties`.

We currently only know of a single member (`type`) of the `VstSpeakerProperties` struct, which is certainly wrong
(a struct only makes sense if there are more members. creating an array of this struct requires it to have a
fixed size, so the struct cannot be extended "later".)

We already know the positions of `VstSpeakerArrangement.type` resp. `.numChannels`, but don't really know whether
there are more members in this struct (so we don't know the exact position of the `.speakers` member).
However, we do now that the `.speakers` member contains `.numChannels` instances of `VstSpeakerProperties`.

~~~C
typedef struct VstSpeakerProperties_ {
  /* misses members; order undefined */
  int type;
} VstSpeakerProperties;

typedef struct VstSpeakerArrangement_ {
  int type;
  int numChannels;
  /* missing members? */
  VstSpeakerProperties speakers[];
} VstSpeakerArrangement;
~~~

When we [first discovered](#speaker-arrangement) some details of the Speaker Arrangement,
we noticed non-null values a position @58, which we concluded might be uninitialized data.

We can easily test this assumption: since `VstSpeakerProperties` is at least 4 bytes large
(assuming that it's `type` member is a 32bit `int` like all other types we have seen so far),
and REAPER can handle up to 64 channels, we can force the full size of `speakers[]` to 256 bytes
(64 * 4), which is way beyong the 88 bytes of position @58.

Printing the first 512 bytes a 64channel plugin receives with the `effSetSpeakerArrangement` opcode, gives:

~~~
0000	  FE FF FF FF 40 00 00 00  00 00 00 00 00 00 00 00
0010	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
0020	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
0030	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
0040	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
0050	  00 00 00 00 00 00 00 00  01 00 00 00 00 00 00 00
0060	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
0070	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
0080	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
0090	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
00a0	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
00b0	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
00c0	  00 00 00 00 00 00 00 00  02 00 00 00 00 00 00 00
00d0	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
00e0	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
00f0	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
0100	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
0110	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
0120	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
0130	  00 00 00 00 00 00 00 00  01 00 00 00 00 00 00 00
0140	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
0150	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
0160	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
0170	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
0180	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
0190	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
01a0	  00 00 00 00 00 00 00 00  02 00 00 00 00 00 00 00
01b0	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
01c0	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
01d0	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
01e0	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
01f0	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
~~~

This is interesting as we still have a value (`01`) at position @58 -
which according to the math we did above cannot be "uninitialized memory".

If we set the number of channels our plugin can process to *2*, we get the following instead:

~~~
0000	  01 00 00 00 02 00 00 00  00 00 00 00 00 00 00 00
0010	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
0020	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
0030	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
0040	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
0050	  00 00 00 00 00 00 00 00  01 00 00 00 00 00 00 00
0060	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
0070	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
0080	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
0090	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
00a0	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
00b0	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
00c0	  00 00 00 00 00 00 00 00  02 00 00 00 00 00 00 00
00d0	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
00e0	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
00f0	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
0100	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
0110	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
0120	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
0130	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
0140	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
0150	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
0160	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
0170	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
0180	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
0190	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
01a0	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
01b0	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
01c0	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
01d0	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
01e0	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
01f0	  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
~~~

Apart from the differing first 8 bytes (that's the `.type` and `.numChannels`
members, which we already established to be differing), we can *also* see some
pattern:

The 2-channel plugin gets values `01` and `02` at the positions @58 resp @c8 and after that only zeros,
whereas the 64-channel version has alternating `01` and `02` values until the printout ends.
Could it be that we are actually seeing `VstSpeakerProperties` that are not occupying only 4 bytes but rather
112 bytes in length (112 being the difference between @c8 and @58)?

So printing 8192 bytes (which should cover 64 channels if each really takes 112 bytes), we observe:

| channels | 112-bytes pattern                                         | garbage     |
|----------|-----------------------------------------------------------|-------------|
| 2        | `01`@58, `02`@c8                                          | after @468  |
| 64       | alternating `01`/`02` from @58 to @1be8, every 0x70 bytes | after @1f88 |
| 3        | `01`@58, `02`@c8, `03`@138                                | after @4da  |

After a certain point the data is densly filled with non-null bytes, which probably really is "uninitialized memory" (aka "garbage").

The important part to notice is that the position difference between the first lonely byte @58 and the last one (@c8, @138, @1be8)
is always `112 * (numChannels-1)`.
So we can conclude, that the `VstSpeakerProperties` has a size of 112 bytes (of which we know that 4 bytes contain the `type` member).
If we assume that the `VstSpeakerArrangment.speakers` array starts immediately after the first 8 bytes,
the `VstSpeakerProperties.type` cannot be at the beginning or the end of the `VstSpeakerProperties`,
but is located at some 80 bytes offset.
There's no real evidence for *any* position of the `type` field.
Since the surrounding bytes don't carry information (at least when filled in by REAPER), we just pick one randomly.
There's also no real evidence whether the stray non-null bytes actually *are* the `type` member,
but since this is the only member of which we know and the only bytes that carry some information, the guess is probably correct.


~~~C
typedef struct VstSpeakerProperties_ {
  /* misses members; order undefined */
  char _padding1[80];
  int type;
  char _padding2[28];
} VstSpeakerProperties;

typedef struct VstSpeakerArrangement_ {
  int type;
  int numChannels;
  /* missing members? */
  VstSpeakerProperties speakers[];
} VstSpeakerArrangement;
~~~

Creating 65 plugins with varying channel count (from 0 to 64) and printing the `VstSpeakerProperties.type` for each channel,
we get:

| `VstSpeakerArrangement.numChannels` | `VstSpeakerArrangement.type` | `VstSpeakerProperties.type` |
|-------------------------------------|------------------------------|-----------------------------|
| 00                                  | -1                           | -                           |
| 01                                  | 0                            | 0                           |
| 02                                  | 1                            | 1,2                         |
| 03                                  | 11                           | 1,2,3                       |
| 04                                  | 11                           | 1,2,1,2                     |
| 05                                  | 15                           | 1,2,1,2,3                   |
| 06                                  | 15                           | 1,2,1,2,1,2                 |
| 07                                  | 23                           | 1,2,1,2,1,2,3               |
| 08                                  | 23                           | 1,2,1,2,1,2,1,2             |
| 09                                  | -2                           | 1,2,1,2,1,2,1,2,3           |
| 10                                  | -2                           | 1,2,1,2,1,2,1,2,1,2         |
| 11                                  | -2                           | 1,2,1,2,1,2,1,2,1,2,3       |
| 12                                  | 28                           | 1,2,1,2,1,2,1,2,1,2,1,2     |
| odd                                 | -2                           | 1,2,...,1,2,3               |
| even                                | -2                           | 1,2,...,1,2,1,2             |

The 108 unknown bytes in each `VstSpeakerProperties` struct are always `0x00`.
So the `VstSpeakerProperties.type` is always pairs of `1` and `2`,
and if the number of speakers is odd, the leftover (last) speaker has a type `3`.

JUCE (juce_audio_plugin_client/VST/juce_VST_Wrapper.cpp) assigns values like
`kSpeakerL`, `kSpeakerR` or `kSpeakerLfe` to the `VstSpeakerProperties.type` member.
Obviously, REAPER doesn't make full use of these values (at least not in my configuration).
The values `1` and `2` are probably `kSpeakerL` resp. `kSpeakerR`.
The value `3` could be `kSpeakerC`, `kSpeakerLfe` or `kSpeakerS`, but it's hard to say.
The value `0` (only seen with the mono setup, tentatively `kSpeakerArrMono`) would be some mono channel,
either `kSpeakerC`, `kSpeakerUndefined` or - following the naming pattern of the Left/Right channels and
[confirmed to exist by some random googling](http://convolver.sourceforge.net/vst.html) -
`kSpeakerM`.

As a sidenote, we also see now that a plugin without channels has a `VstSpeakerArrangement.type` of *-1*,
which is most likely `kSpeakerArrEmpty` (something we already guessed above, but had no backing yet).

| name               | value |
|--------------------|-------|
| `kSpeakerArrEmpty` | -1    |
| `kSpeakerL`        | 1     |
| `kSpeakerR`        | 2     |

## enter MrsWatson
After trying to [compile MrsWatson](#compiling-mrswatson), it becomes a bit clearer, why there's quite an offset
at the beginning of `VstSpeakerProperties`, as there are some additional members.

*MrsWatson* assigns to the properties like so:

~~~C
speakerArrangement->speakers[i].azimuth = 0.0f;
speakerArrangement->speakers[i].elevation = 0.0f;
speakerArrangement->speakers[i].radius = 0.0f;
speakerArrangement->speakers[i].reserved = 0.0f;
speakerArrangement->speakers[i].name[0] = '\0';
speakerArrangement->speakers[i].type = kSpeakerUndefined;
~~~

Assuming that the order of members follows the struct definition,
and that the first four values are indeed single-precision (since the only
reason why *MrsWatson* would use the `f` specifier is to avoid compiler warnings),
we get something like:

~~~C
typedef struct VstSpeakerProperties_ {
  float azimuth;
  float elevation;
  float radius;
  float reserved;
  char name[64];
  int type;
  char _padding2[28];
} VstSpeakerProperties;
~~~


# audioMasterGetCurrentProcessLevel
So far we know that there is a host opcode `audioMasterGetCurrentProcessLevel`, which returns `kVstProcessLevelUnknown` in MrsWatson.
The presence of the host opcode and the generic name of the value returned by MrsWatson, suggest that there are other, more specific values to return.

After another round of googling for terms `vst` and `processlevel`, I eventually stumbled upon a [post in the "Deprecated REAPER issue tracker"](https://forums.cockos.com/project.php?issueid=3382) (accessed 2020-01-20):

> Offline Render causes glitching with some plugs (Reaper 3.76 sending kVstProcessLevelRealtime to AudioEffectX during Offline Render)
>
> First encountered using EW Spaces convolution reverb. The verb always glitches in offline render, and runs much faster than would be expected (i.e. 60x)
> In further investigation it seems that when Reaper is in offline render mode, for any plug-in that calls AudioEffectX::getCurrentProcessLevel Reaper 3.76 returns kVstProcessLevelRealtime instead of kVstProcessLevelOffline.

and further on:

> Just discovered this isn't a bug. There is a setting in Reaper to enable the sending of this report variable.
> In the Reaper Preferences under Plug-ins > VST there is a check-box for "inform plug-ins of offline rendering state" ... that fixes the problem.

This post tells us that there are at least two more process levels, named `kVstProcessLevelRealtime` and `kVstProcessLevelOffline`.
And that REAPER would report the former during offline rendering, but can be made into reporting the latter (by checking some option in the preferences).

Nice. Now we only need to ask the host for the current processLevel (calling `audioMasterGetCurrentProcessLevel` e.g. in the `processReplacing` callback) while telling REAPER to "Render to disk", and we get

| "Inform plug-ins of offline rendering..." | value | name                       |
|-------------------------------------------|-------|----------------------------|
| unchecked (OFF)                           | 2     | `kVstProcessLevelRealtime` |
| checked (ON)                              | 4     | `kVstProcessLevelOffline`  |

REAPER also reports a a process-level of `1`, but only at the beginning (e.g. while the host calls the plugin with `effMainsChanged`).

| name                       | value | note                                        |
|----------------------------|-------|---------------------------------------------|
| ??                         | 0     | returned by JUCE is in realtime mode        |
| ??                         | 1     | returned by REAPER during `effMainsChanged` |
| `kVstProcessLevelRealtime` | 2     | returned by REAPER during normal rendering  |
| ??                         | 3     | (inbetween)                                 |
| `kVstProcessLevelOffline`  | 4     | returned by REAPER when offline-rendering; returned by JUCE if NOT in realtime mode |
| `kVstProcessLevelUnknown`  | ??    | used by MrsWatson                           |

The value of `kVstProcessLevelUnknown` is most likely something special, like *-1*, or (if we follow the schema used for `kPlugCategUnknown`) *0*.
I'm not sure, why JUCE would return `kPlugCategUnknown` for the realtime-case though.


# JUCE-6.0.7

Time has passed.
Recently (at the beginning of 2021) I checked compiling `JstHost` and `JstPlugin` against JUCE-6.0.7.
And it miserably failed, with unknown symbols.

~~~bash
make -k ... 2>&1 \
| egrep -i "error: .*vst2" \
| sed -e 's|.*: error: ||' \
| sort -u
~~~

Examining the error messages, we see that the `kPlugCategMaxCount` is missing.

## kPlugCategMaxCount

The `kPlugCategMaxCount` seems to be related to `VstPlugCategory` enumeration (as it matches the
`kPlugCateg*` naming convention).
It is a common C-idiom when working with enums to add a final "dummy" entry, that can be used to
determine the number of (valid) items, as in:

~~~C
enum foobar {
    foo,
    bar,
    //...
    last /* when adding new entries, add them before this one */
};
~~~

The `kPlugCategMaxCount` most likely falls in this category as well,
so we can assume it is the last element of the `VstPlugCategory` enumeration.
We don't really have a known value for this symbol, as we cannot know the
real size of the enumeration.
Currently our `VstPlugCategory` enumeration holds 12 items (with values from 0 to 11),
adding `kPlugCategMaxCount` will give it the value of 12 (but keep in mind that adding
more items to the enumeration will change this value).
Not knowing the exact value here is not a big deal,
as the usual purpose of such an enum-terminator
is to check whether a given enum value is in the range of known items at the time of compilation,
so it will be correct in any case.


## kVstMaxParamStrLen

We can also do a bit of blind searching in the JUCE-sources,
for symbols that *look like* those we are interested in
(but that don't trigger compilation errors, e.g. because they are only used in
comments).

So far, many variables follow one of these patterns (with the variant part `*` starting with a capital letter):

| pattern     | note                  |
|-------------|-----------------------|
| `eff*`      | effect opcode         |
| `kVst*`     | constant              |
| `kPlug*`    | effect category       |
| `kSpeaker*` | speaker configuration |
| `k*`        | constant              |

We are only interested in those that have something to do with VST (and not VST3).
So we simply check for such names in files that somewhere in their path contain the string `vst`.

~~~sh
for prefix in eff k; do
  rgrep -h "\b${prefix}" */ 2>/dev/null | sed -e "s|\b\(${prefix}\)|\n\1|g" | egrep "^${prefix}[A-Z]" | sed -e 's|\(.\)\b.*|\1|'
done | sort -u | while read s; do
  rgrep -wl "${s}" */ 2>/dev/null | grep -vi vst3 | grep -i vst >/dev/null && echo ${s}
done | tee symbols.txt

(echo '#include "fst.h"
int main() {'; cat symbols.txt | sed -e 's|$|;|'; echo 'return 0;}') > fst.c

gcc fst.c -o fst 2>&1 \
| grep "error:" | sed -e 's|.*: error: .||' -e 's|. .*||' \
> missing.txt
~~~

When we try a test-compilation with some dummy use of these symbols, we can conflate the total list
of 171 symbols extracted from JUCE-6.0.7 to 11 names that we don't know yet:


| name                          | note                   |
|-------------------------------|------------------------|
| `kCFAllocatorDefault`         | Apple's CoreFoundation |
| `kCFStringEncodingUTF8`       | Apple's CoreFoundation |
| `kCFURLPOSIXPathStyle`        | Apple's CoreFoundation |
| `kControlBoundsChangedEvent`  | ?? (macOS)             |
| `kEventClassControl`          | ?? (macOS)             |
| `kEventControlBoundsChanged`  | ?? (macOS)             |
| `kFSCatInfoNone`              | macOS                  |
| `kHIViewWindowContentID`      | macOS                  |
| `kVstMaxParamStrLen`          |                        |
| `kWindowCompositingAttribute` | macOS                  |
| `kWindowContentRgn`           | macOS                  |


So Apple also uses the `k` prefix for constants.
Anyhow, only the `kVstMaxParamStrLen` constant appear to be truely VST2-related.
Checking the JUCE sources for a bit of context, we learn:

> length should technically be `kVstMaxParamStrLen`, which is 8, but hosts will normally allow a bit more

So we found one more symbol, along with its value:

| name                 | value |
|----------------------|-------|
| `kVstMaxParamStrLen` | 8     |





# Summary

So far we have discovered quite a few opcodes (and constants):


Trying to compile JUCE plugins or plugin-hosts, we still miss a considerable number
(leaving those out that JUCE handles with a no-op):

| names used by JUCE                       |
|------------------------------------------|
| `audioMasterGetAutomationState`          |
| `audioMasterGetDirectory`                |
| `audioMasterGetNumAutomatableParameters` |
| `audioMasterIOChanged`                   |
| `audioMasterIdle`                        |
| `audioMasterNeedIdle`                    |
| `audioMasterPinConnected`                |
| `audioMasterSetOutputSampleRate`         |
| `audioMasterUpdateDisplay`               |
|------------------------------------------|
| `effConnectInput`/ `effConnectOutput`    |
| `effEdit(Draw,Idle,Mouse,Sleep,Top)`     |
| `effGetNumMidi(In,Out)putChannels`       |
| `effGetTailSize`                         |
| `effIdle`                                |
| `effKeysRequired`                        |
| `effSetBypass`                           |
|------------------------------------------|
| `kVstAutomationReading`                  |
| `kVstAutomationWriting`                  |
| `kVstPinIsActive`                        |
| `kVstPinIsStereo`                        |
| `kVstPinUseSpeaker`                      |
| `kSpeakerArr*`                           |
| `kSpeaker*`                              |
| `kVstSmpte*`                             |


On the other hand, running using a self-compiled plugin in a commercial DAW (like REAPER)
or using commercial plugins in a self-compiled plugin host, we only encounter very few opcodes
that we don't know yet:

| name       | value |
|------------|-------|
| audioHost* | 3     |
| audioHost* | 13    |
| audioHost* | 42    |
|------------|-------|
| eff*       | 19    |
| eff*       | 53    |
| eff*       | 56    |
| eff*       | 62    |
| eff*       | 66    |


# misc
LATER move this to proper sections


## hostCode:3
## hostCode:13
## hostCode:42

## effCode:19
## effCode:53


## effCode:56

gets called with automation, whenever the window gets focus?

    FstClient::dispatcher(0x2a4c8b0, 56, 0, 0, 0x7fff4a83fb40, 0.000000)...

The address seems to be zeroed-out (at least the first 0x99 bytes).
The index is the parameter index currently being automated...


## effCode:62
This is called when the MIDI-dialog gets opened (right before effCode:66; but only once)

5*16 (80) bytes == 0x00

## effCode:66
adding a MIDI-item in REAPER and pressing some keys
will send plenty of messages with effect opcode `66` to a plugin.
There's also the occasional opcode `62`:

~~~
FstClient::dispatcher(0x19b9250, 66, 0, 0, 0xeae040, 0.000000);
FstClient::dispatcher(0x19b9250, 62, 0, 0, 0x7ffe232a7660, 0.000000);
FstClient::dispatcher(0x19b9250, 66, 0, 0, 0xeae040, 0.000000);
FstClient::dispatcher(0x19b9250, 66, 0, 0, 0xeae040, 0.000000);
~~~
### effCode:66
the pointer is an address to a memory region,
where the first 4 bytes are 0,
and the 2nd 4 bytes are an int32 between 34 and 72.
the numbers seem to be the visible notes on the virtual MIDI keyboard.


## even more symbols

|                | symbol                         | project    |
|----------------|--------------------------------|------------|
| opcode         | effBeginSetProgram             | vstplugin~ |
| opcode         | effEndSetProgram               | vstplugin~ |
|----------------|--------------------------------|------------|
| constant       | kSpeakerM                      | vstplugin~ |
|----------------|--------------------------------|------------|
| member         | VstTimeInfo.samplesToNextClock | vstplugin~ |
|----------------|--------------------------------|------------|
| type           | VstAEffectFlags                | vstplugin~ |
| type           | VstInt32                       | vstplugin~ |
|----------------|--------------------------------|------------|
| function/macro | CCONST                         | vstplugin~ |
