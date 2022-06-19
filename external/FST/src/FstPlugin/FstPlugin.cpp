#include "fst.h"
#include "fst_utils.h"

#include <cstdio>
#include <cstring>
#include <iostream>

typedef void (t_fun0)(void);

static AEffectDispatcherProc dispatch = 0;
static int curProgram = 0;

static float parameters[3];
static ERect editorBounds = {0, 0, 320, 240};

static char chunk[] = "This is the chunk for the FstPlugin.";

static VstEvents*s_ves;
static VstMidiSysexEvent s_sysex;
static VstMidiEvent s_midi;
static unsigned char s_sysexDump[] = {0xF0, 0x01, 0x02, 0x03, 0x04, 0x03, 0x02, 0x01, 0xF7};
static unsigned char s_midiDump[] = {0x80, 0x40, 0x0, 0};

void crash() {
  t_fun0*f=0;
  /* crash */
  fflush(stdout);
  fflush(stderr);
  f();
}

void print_struct7(AEffect* effect) {
#if 0
  auto *str = (double*)dispatch(effect, 7, 0, 65024, 0, 0.);
  for(size_t i=0; i<96/sizeof(*str); i++)
    std::cout << " " << str[i];
  std::cout << std::endl;
#else
  auto *vti = (VstTimeInfo*)dispatch(effect, 7, 0, 65024, 0, 0.);
  print_timeinfo(vti);
#endif
}

t_fstPtrInt dispatch_v (AEffect* effect, int opcode, int index, t_fstPtrInt ivalue, void*ptr, float fvalue) {
  bool doprint=true;
  switch(opcode) {
  default:
    doprint=true;
    break;
  case audioMasterGetCurrentProcessLevel:
    //doprint=false;
    break;
  }
  if(effect) {
    if(doprint) {
      char opcodestr[256];
      printf("FstClient::snd2host(%p, %s, %d, %lu, %p, %f) => ",  effect, hostCode2string(opcode, opcodestr, 255), index, ivalue, ptr, fvalue);
    }
    t_fstPtrInt result = dispatch(effect, opcode, index, ivalue, ptr, fvalue);
    if(doprint)
      printf("FstClient::snd2host: %lu (0x%lX)\n", result, result);
    return result;
  }
  return 0xBEEF;
}

void test_opcodes(AEffect*effect, size_t toopcode = 100, size_t fromopcode=0) {
  printf("testing host's dispatcher\n");
  for(size_t opcode=fromopcode; opcode<toopcode; opcode++) {
    char buf[1024] = {0};
    snprintf(buf, 1023, "%s", "fudelDudelDa");
    dispatch_v(effect, opcode, 0, 0, buf, 0.f);
#if 0
    if(*buf) {
      printf("\t'%.*s'\n", 1024, buf);
    }
#endif
  }
  printf("tested hosts's dispatcher with opcodes %d..%d\n", fromopcode, toopcode);
}

static void test_gettime_(AEffect*eff, t_fstInt32 flags) {
  printf("getTime -> 0x%04X\n", flags);
  VstTimeInfo*t=(VstTimeInfo*)dispatch(eff, audioMasterGetTime, 0, flags, 0, 0.f);
  print_timeinfo(t);

}

static void test_gettime(AEffect*eff) {
  t_fstInt32 flags = 0xFFFFFFFF;
  //flags = 0x2E02;
  //flags |= (1<< 8); // kVstNanosValid
  //flags |= (1<<14); // kVstSmpteValid (Current time in SMPTE format)
  //flags |= (1<<15); // kVstClockValid (Sample frames until next clock)
#ifdef TIME_FLAG
  flags = (1<<TIME_FLAG);
#endif

  test_gettime_(eff, flags);
}


static int test_opcode35(AEffect*eff) {
#ifdef FST_CATEGORY
  return FST_CATEGORY;
#endif
  return kPlugSurroundFx;
}

static int test_opcode70(AEffect*eff, char*ptr) {
  static int count = 0;
  count++;
  if(count < 5) {
    snprintf(ptr, 128, "shell%d", count);
    return count;
  }
  return 0;
}

static int test_opcode66(AEffect*eff,
    t_fstInt32 opcode, int index,
    t_fstPtrInt ivalue, void* const ptr, float fvalue) {
  /* this opcodes set the names of they keys of the virtual MIDI-keyboard in REAPER... */
  //printf("opcode:%d\n", opcode);
  //print_numbers((int*)ptr, 2);
  char*str=(char*)ptr + 8;
  int*iptr=(int*)ptr;
  //printf("KEY: %d\t%d\n", iptr[0], iptr[1]);
  snprintf(str, 16, "key[%d]%d=", iptr[0], iptr[1]);
  for(size_t i=0; i<32; i++) {
    //str[i+8]=i+64;
  }
  return 0;
}
static int test_opcode62(AEffect*eff,
    t_fstInt32 opcode, int index,
    t_fstPtrInt ivalue, void* const ptr, float fvalue) {
  print_hex(ptr, 128);
  snprintf((char*)ptr, 128, "OPCODE62");
  return 100;
}
static void test_opcode56(AEffect*eff,
    t_fstInt32 opcode, int index,
    t_fstPtrInt ivalue, void* const ptr, float fvalue) {
  print_hex(ptr, 160);
}

static int test_opcode26(AEffect*eff,
                         t_fstInt32 opcode, int index,
                         t_fstPtrInt ivalue, void* const ptr, float fvalue) {
  int result = (index<2);
  printf("OPCODE26[%d]: %d\n", index, result);
  fflush(stdout);
  return result;
}

/* effProcessEvents: handle MIDI */
static void test_opcode25(AEffect*eff,
    t_fstInt32 opcode, int index,
    t_fstPtrInt ivalue, void* const ptr, float fvalue) {

  unsigned char midi[4] = {0x90, 0x40, 0x7f, 0};
  VstEvents*vse=(VstEvents*)ptr;
  vse = create_vstevents(midi);

  dispatch_v(eff, audioMasterProcessEvents, index, ivalue, vse, fvalue);

  return;
  char filename[128];
  print_hex(ptr, 256);
  static int count = 0;
  count++;
  sprintf(filename, "midi/dump.%d", count);
  dump_data(filename, ptr, 256);
}

static void test_opcode42(AEffect*eff,
    t_fstInt32 opcode, int index,
    t_fstPtrInt ivalue, void* const ptr, float fvalue) {
  return;
  /* effGetSpeakerArrangement or effGetSpeakerArrangement */
  /* ptr and ivalue point to VstSpeakerArrangement* */
  VstSpeakerArrangement*iarr = (VstSpeakerArrangement*)ivalue;
  VstSpeakerArrangement*oarr = (VstSpeakerArrangement*)ptr;
  print_hex(iarr, 16);
  print_hex(oarr, 16);
  printf("JMZ| %d | 0x%X | %d |\n", iarr->type, iarr->type, iarr->numChannels);
  printf("JMZ| %d | 0x%X | %d |\n", oarr->type, oarr->type, oarr->numChannels);
  fflush(stdout);
#ifdef NUM_INPUTS
  crash();
#endif
}

static void test_processLevel(AEffect*eff) {
  dispatch_v(eff, audioMasterGetCurrentProcessLevel, 0, 0, 0, 0.);
}

static bool dispatcher_printEff(AEffect*eff,
    t_fstInt32 opcode, int index,
    t_fstPtrInt ivalue, void* const ptr, float fvalue) {
  char opcodestr[512];
#if 0
  printf("FstClient::dispatch(%p, %s, %d, %ld, %p, %f);\n",
         eff, effCode2string(opcode, opcodestr, 512), index, ivalue, ptr, fvalue);
#else
  printf("FstClient::dispatch(%s, %d, %ld, %p, %f);\n",
         effCode2string(opcode, opcodestr, 512), index, ivalue, ptr, fvalue);
#endif
  return true;
}
static bool dispatcher_skip(t_fstInt32 opcode) {
  switch(opcode) {
  case 53:
    /* REAPER calls this permanently */
    //printf("53...\n");
    //print_struct7(eff);
    return true;
  }
  return false;
}
static bool dispatcher_noprint(t_fstInt32 opcode) {
  switch(opcode) {
  case effGetParamDisplay:
  case effGetParamLabel:
  case effGetParamName:
  case effProcessEvents:
  case effVendorSpecific:
  case 66:
    return true;
  }
  return false;
}
static void print_ptr4opcode(t_fstInt32 opcode, void*const ptr) {
  if(!ptr)return;
  char*str = (char*)ptr;
  switch(opcode) {
  default: break;
  case effEditOpen:
    /* 'ptr' is a window-id, we cannot print it */
    return;
  case effGetParamName: case effGetParamDisplay: case effGetParamLabel:
    return;
  }
  if(str && *str)
    printf("\tFtsClient::dispatcher(ptr='%.*s')\n", 64, str);
  //if(str)print_hex(str, 96);
}
static t_fstPtrInt dispatcher(AEffect*eff, t_fstInt32 opcode, int index, t_fstPtrInt ivalue, void* const ptr, float fvalue) {
  if(25==opcode)      test_processLevel(eff);
  //if(53==opcode)    test_gettime(eff);
  if(26==opcode)      return test_opcode26(eff, opcode, index, ivalue, ptr, fvalue);
  if(dispatcher_skip(opcode))return 0;
  if(!dispatcher_noprint(opcode)) {
    dispatcher_printEff(eff, opcode, index, ivalue, ptr, fvalue);
    print_ptr4opcode(opcode, ptr);
  }

  switch(opcode) {
  default: break;
    //case effGetVstVersion: return 2400;
  case 26:
    return test_opcode26(eff, opcode, index, ivalue, ptr, fvalue);
  case 35:
    return test_opcode35(eff);
  case 70:
    return test_opcode70(eff, (char*)ptr);
  case effGetVendorString:
    snprintf((char*)ptr, 16, "SuperVendor");
    return 1;
  case effGetEffectName:
    snprintf((char*)ptr, 16, "SuperEffect");
    return 1;
#if 1
  case effSetSpeakerArrangement:
    test_opcode42(eff, opcode, index, ivalue, ptr, fvalue);
    return 0;
#endif
#if 0
  case 56:
    test_opcode56(eff, opcode, index, ivalue, ptr, fvalue);
    return 1;
#endif
  case effProcessEvents:
    //test_opcode25(eff, opcode, index, ivalue, ptr, fvalue);
    return 1;
  case 62:
    return test_opcode62(eff, opcode, index, ivalue, ptr, fvalue);
  case 66:
    return test_opcode66(eff, opcode, index, ivalue, ptr, fvalue);
  case effEditGetRect:
    *((ERect**)ptr) = &editorBounds;
    return (t_fstPtrInt)&editorBounds;
  case effGetChunk:
    {
      char**strptr=(char**)ptr;
      *strptr=chunk;
    }
    //printf("getChunk: %d bytes @ %p\n", sizeof(chunk), chunk);
    return sizeof(chunk);
  case effSetProgram:
    //printf("setting program to %d\n", ivalue);
    curProgram = ivalue;
    return 1;
  case effGetProgramName:
    snprintf((char*)ptr, 32, "FstProgram%d", curProgram);
    //printf("JMZ:setting program-name to %s\n", (char*)ptr);
    return 1;
  case effGetParamLabel:
    snprintf((char*)ptr, 32, "°");
    return 0;
  case effGetParamName:
    if(index>=sizeof(parameters))
      index=sizeof(parameters);
    snprintf((char*)ptr, 32, "rotation%c", index+88);
    return 0;
  case effGetParamDisplay:
    if(index>=sizeof(parameters))
      index=sizeof(parameters);
    snprintf((char*)ptr, 32, "%+03d", int((parameters[index]-0.5)*360+0.5));
    return 0;
  case effCanDo:
    do {
      printf("canDo '%s'?\n", (char*)ptr);
      if(!strcmp((char*)ptr, "receiveVstEvents"))
        return 1;
      if(!strcmp((char*)ptr, "receiveVstMidiEvents"))
        return 1;
      if(!strcmp((char*)ptr, "sendVstEvents"))
        return 1;
      if(!strcmp((char*)ptr, "sendVstMidiEvents"))
        return 1;
      if(!strcmp((char*)ptr, "wantsChannelCountNotifications"))
        return 1;
#if 0
      if(!strcmp((char*)ptr, "hasCockosExtensions")) {
        return 0xBEEF0000;
      }
#endif
    } while(0);
    return 0;
  case effMainsChanged:
      dispatch_v(eff, audioMasterGetCurrentProcessLevel, 0, 0, 0, 0.);
#if 0
      do {
        static bool first=true;
        if(first) {
          test_opcodes(eff, 50);
        } else {
          auto *str = (char*)dispatch_v(eff, audioMasterGetTime, 0, 65024, 0, 0.);
          char filename[128];
          static int icount = 0;
          snprintf(filename, 127, "./testdump.%d", icount);
          printf("OUTFILE[%d]: %s\n", icount, filename);
          icount++;
          dump_data(filename, str, 512);
        }
        first=false;
      } while(0);
#endif
      dispatch_v(eff, audioMasterWantMidi, 0, 1, 0, 0.);
      break;
  }
  //printf("FstClient::dispatch(%p, %d, %d, %d, %p, %f)\n", eff, opcode, index, ivalue, ptr, fvalue);
  //printf("JMZ\n");

  return 0;
}
static t_fstPtrInt dispatchme(AEffect*eff, t_fstInt32 opcode, int index, t_fstPtrInt ivalue, void* const ptr, float fvalue) {
  return dispatch_effect ("FstPlugin", dispatcher, eff, opcode, index, ivalue, ptr, fvalue);
}
static void find_audioMasterSizeWindow() {
  for(size_t opcode = 0; opcode<100; opcode++) {
    char hostcode[512] = {0};
    int width = 1720;
    int height = 640;
    if(37==opcode)
      continue;
    printf("trying: %d\n", opcode);
    t_fstPtrInt res = dispatch(0, opcode, width, height, 0, 0);
    printf("%s[%dx%d] returned %ld\n", hostCode2string(opcode, hostcode, 512), width, height, res);
  }
}

static void setParameter(AEffect*eff, int index, float value) {
  //printf("FstClient::setParameter(%p)[%d] -> %f\n", eff, index, value);
  if(index>=sizeof(parameters))
    index=sizeof(parameters);
  parameters[index] = value;

}
static float getParameter(AEffect*eff, int index) {
  if(index>=sizeof(parameters))
    index=sizeof(parameters);
  //printf("FstClient::getParameter(%p)[%d] <- %f\n", eff, index, parameters[index]);
  return parameters[index];
}
static void process(AEffect*eff, float**indata, float**outdata, int sampleframes) {
#if 0
  printf("FstClient::process0(%p, %p, %p, %d) -> %f\n", eff, indata, outdata, sampleframes, indata[0][0]);
  test_gettime(eff);
#endif
}
static void processReplacing(AEffect*eff, float**indata, float**outdata, int sampleframes) {
#if 1
  printf("FstClient::process1(%p, %p, %p, %d) -> %f\n", eff, indata, outdata, sampleframes, indata[0][0]);
  test_processLevel(eff);
  //test_gettime(eff);
#endif
}
static void processDoubleReplacing(AEffect*eff, double**indata, double**outdata, int sampleframes) {
#if 0
  printf("FstClient::process2(%p, %p, %p, %d) -> %g\n", eff, indata, outdata, sampleframes, indata[0][0]);
  test_gettime(eff);
#endif
}

extern "C"
AEffect*VSTPluginMain(AEffectDispatcherProc dispatch4host) {
  dispatch = dispatch4host;
  printf("FstPlugin::main(%p)\n", dispatch4host);
  for(size_t i=0; i<sizeof(parameters); i++)
    parameters[i] = 0.5;

  AEffect* eff = new AEffect;
  memset(eff, 0, sizeof(AEffect));
  eff->magic = 0x56737450;
  eff->dispatcher = dispatchme;
  eff->process = process;
  eff->getParameter = getParameter;
  eff->setParameter = setParameter;

  eff->numPrograms = 1;
  eff->numParams = 3;
#ifdef NUM_INPUTS
  eff->numInputs  = NUM_INPUTS;
  eff->numOutputs = NUM_INPUTS;
#else
  eff->numInputs  = 6;
  eff->numOutputs = 6;
#endif
  eff->float1 = 1.;
  eff->object = eff;
  eff->uniqueID = 0xf00d;
  eff->version = 666;

  //eff->flags |= effFlagsProgramChunks;
  eff->flags |= effFlagsCanReplacing;
  eff->flags |= effFlagsCanDoubleReplacing;

  eff->flags |= effFlagsHasEditor;

  eff->processReplacing = processReplacing;
  eff->processDoubleReplacing = processDoubleReplacing;
  print_aeffect(eff);

  const char* canDos[] = { "supplyIdle",
                           "sendVstEvents",
                           "sendVstMidiEvent",
                           "sendVstTimeInfo",
                           "receiveVstEvents",
                           "receiveVstMidiEvent",
                           "supportShell",
                           "sizeWindow",
                           "shellCategory" };
  for(size_t i = 0; i<(sizeof(canDos)/sizeof(*canDos)); i++) {
    char buf[512] = {0};
    char hostcode[512] = {0};
    snprintf(buf, 511, canDos[i]);
    buf[511]=0;
    t_fstPtrInt res = dispatch(0, audioMasterCanDo, 0, 0, buf, 0);
    if(*buf)
      printf("%s['%.*s'] returned %ld\n", hostCode2string(audioMasterCanDo, hostcode, 512), 512, buf, res);
  }

  //find_audioMasterSizeWindow();

  char buf[512] = {0};
  dispatch(eff, audioMasterGetProductString, 0, 0, buf, 0.f);
  printf("masterProduct: %s\n", buf);

  s_ves = (VstEvents*)calloc(1, sizeof(VstEvents)+2*sizeof(VstEvent*));
  s_ves->numEvents = 2;
  s_ves->events[0] = (VstEvent*)&s_midi;
  s_ves->events[1] = (VstEvent*)&s_sysex;

  memset(&s_midi, 0, sizeof(s_midi));
  memset(&s_sysex, 0, sizeof(s_sysex));

  s_midi.type = kVstMidiType;
  s_midi.byteSize = sizeof(s_midi);
  s_midi.deltaFrames = 1;
  for(size_t i=0; i<4; i++)
    s_midi.midiData[i] = s_midiDump[i];

  s_sysex.type = kVstSysExType;
  s_sysex.byteSize = sizeof(s_sysex);
  s_sysex.deltaFrames = 0;
  s_sysex.dumpBytes = sizeof(s_sysexDump);
  s_sysex.sysexDump = (char*)s_sysexDump;

  print_events(s_ves);

  printf("=====================================\n\n\n");

  return eff;
}
