#include "fst.h"
#include "fst_utils.h"
#include <stdio.h>

#include <string>
#include <string.h>

#include <map>

static std::map<AEffect*, AEffectDispatcherProc>s_host2plugin;
static std::map<AEffect*, AEffectDispatcherProc>s_plugin2host;
static std::map<AEffect*, std::string>s_pluginname;
static AEffectDispatcherProc s_plug2host;

static
t_fstPtrInt host2plugin (AEffect* effect, int opcode, int index, t_fstPtrInt ivalue, void*ptr, float fvalue) {
  //printf("host2plugin:%d\n", opcode); fflush(stdout);
  switch(opcode) {
  case effGetVendorString:
    printf("getVendorString\n");
    snprintf((char*)ptr, 16, "ProxyVendor");
    return 1;
  case effGetEffectName:
    printf("getEffectName\n");
    snprintf((char*)ptr, 16, "ProxyEffect");
    return 1;
  case 26:
    printf("OPCODE26: %d\n", index);
    return (index<5);
  case 56:
    printf("OPCODE56\n");
    print_hex(ptr, 256);
    return dispatch_effect("???", s_host2plugin[effect], effect, opcode, index, ivalue, 0, fvalue);
    break;
  case 62:
    printf("OPCODE62?\n");
    print_hex(ptr, 256);
      // >=90: stack smashing
      // <=85: ok
    snprintf((char*)ptr, 85, "JMZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ");
  default:
    break;
  }
  AEffectDispatcherProc h2p = s_host2plugin[effect];
  if(!h2p) {
    printf("Fst::host2plugin:: NO CALLBACK!\n");
    return 0xDEAD;
  }
  const char*pluginname = 0;
  if(effect)
    pluginname = s_pluginname[effect].c_str();

  bool doPrint = true;
#ifdef FST_EFFKNOWN
  doPrint = !effKnown(opcode);
#endif
  switch(opcode) {
  default: break;
  case effGetChunk: case effSetChunk:
  case effVendorSpecific:
    doPrint = false;
    break;
  }
  t_fstPtrInt result = 0;
  if(doPrint) {
    dispatch_effect(pluginname, h2p, effect, opcode, index, ivalue, ptr, fvalue);
  } else {
    result = h2p(effect, opcode, index, ivalue, ptr, fvalue);
  }
  switch(opcode) {
  default: break;
  case 56:
    print_hex(ptr, 256);
    break;
  case 62:
    printf("OPCODE62!\n");
    print_hex(ptr, 256);
  }
  return result;
}

static
t_fstPtrInt plugin2host (AEffect* effect, int opcode, int index, t_fstPtrInt ivalue, void*ptr, float fvalue) {
  //printf("plugin2host:%d\n", opcode); fflush(stdout);
  AEffectDispatcherProc p2h = s_plugin2host[effect];

  if(!p2h)p2h = s_plug2host;
  if(effect && !s_host2plugin[effect]) {
    s_host2plugin[effect] = effect->dispatcher;
    effect->dispatcher = host2plugin;
  }
  const char*pluginname = 0;
  if(effect)
    pluginname = s_pluginname[effect].c_str();

  bool doPrint = true;
#ifdef FST_HOSTKNOWN
  doPrint = !hostKnown(opcode);
#endif
  switch(opcode) {
  default: break;
  case audioMasterGetTime:
    doPrint = false;
    break;
  }
  t_fstPtrInt result = -1;
  if(doPrint) {
    if(0xDEADBEEF ==opcode) {
      if (0xDEADF00D == index) {
        printf("\t0xDEADFEED/0xDEADF00D '%s'\n", ptr);
      } else
        printf("\t0x%X/0x%X\n", opcode, index);
    }

    result = dispatch_host(pluginname, p2h, effect, opcode, index, ivalue, ptr, fvalue);
  } else {
    result = p2h(effect, opcode, index, ivalue, ptr, fvalue);
  }
  return result;
}


extern "C"
AEffect*VSTPluginMain(AEffectDispatcherProc dispatch4host) {
  char pluginname[512] = {0};
  char*pluginfile = getenv("FST_PROXYPLUGIN");
  if(!pluginfile)return 0;
  s_plug2host = dispatch4host;

  t_fstMain*plugMain = fstLoadPlugin(pluginfile);
  if(!plugMain)return 0;

  AEffect*plug = plugMain(plugin2host);
  if(!plug)
    return plug;

  printf("plugin.dispatcher '%p' -> '%p'\n", plug->dispatcher, host2plugin);
  if(plug->dispatcher != host2plugin) {
    s_host2plugin[plug] = plug->dispatcher;
    plug->dispatcher = host2plugin;
  }

  s_host2plugin[plug](plug, effGetEffectName, 0, 0, pluginname, 0);
  if(*pluginname)
    s_pluginname[plug] = pluginname;
  else
    s_pluginname[plug] = pluginfile;

  s_plugin2host[plug] = dispatch4host;
  print_aeffect(plug);
  fflush(stdout);
  return plug;
}
