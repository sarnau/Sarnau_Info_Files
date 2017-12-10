// CCF = Cabernet Configuration File
// Version 4.8
// - with support for hidden panels)
// - with support for CCF version 2 (Beep and Timer actions)

// WANRING: only works on BIG-ENDIAN machines!!!
// (because the ccf file format is also big endian)

// done 1999 Markus Fritze, <http://www.markus-fritze.de/>

//#define NDEBUG    1 // disable assert()
#include <MacTypes.h> // SInt8, SInt16, etc.
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#define FNAME   "default.ccf"

#define CHECK_LEAKEDMEM     1

#pragma align_array_members off
#pragma options align=packed
typedef struct
{
  SInt32  versionPos;     // position offset to version string
  SInt32  filler1;
  char    id[8];          // = "@\245Z@_CCF";
  SInt32  crcPos;         // offset to the checksum (same like endPos)

  SInt16  year;
  SInt8   month;
  SInt8   day;
  SInt8   filler2;
  SInt8   hour;
  SInt8   minute;
  SInt8   seconds;

  SInt32  filler3;
  char    id2[4];         // = "CCF\0";

  SInt32  version;        // (1 = first version, 2 = with timers)
  SInt32  endPos;         // offset to the checksum (same like ptr2checksum)
  SInt32  version2;       // (version1: 60, version2: 1)
  SInt32  sysHomePos;     // offset to HOME panels
  SInt32  devPos;         // offset to first device
  SInt32  macroPos;       // offset to first macro
  SInt32  confProp;       // e.g. 0x02 = configuration is write protected
  SInt32  macroXPosV1;    // ??? (== 4 in version 2?!?)
  SInt32  macroXPosV2;    // ???
  SInt16  filler4;
} sHeaderStruct;

typedef struct
{
  SInt32  nextPos;        // != NULL => position of next sEntryStruct
  SInt32  namePos;
  SInt32  filler1;
  SInt32  filler2;
  SInt32  actionPos;
  SInt32  leftKeyPos;
  SInt32  rightKeyPos;
  SInt32  VolMinusKeyPos;
  SInt32  VolPlusKeyPos;
  SInt32  ChanMinusKeyPos;
  SInt32  ChanPlusKeyPos;
  SInt32  MuteKeyPos;
  SInt32  filler3;
  SInt32  leftKeyNamePos;
  SInt32  rightKeyNamePos;
  SInt32  panelsPos;
  SInt8   attr;
} sEntryStruct;

typedef struct
{
  SInt32  nextPos;      // != NULL => position of next sPanelStruct
  SInt32  namePos;      // name of the panel (Bit 31 is set when panel is hidden)
  SInt8   count;        // number of entries, always (?) == count2
  SInt8   count2;
  struct
  {
    SInt16  x;
    SInt16  y;
    SInt32  subPanelPos;
    SInt8   type;
  } data[];
} sPanelStruct;

typedef struct
{
  union
  {
    struct
    {
      SInt16    w;
      SInt16    h;
      SInt32    namePos;
      SInt32    iconPos;
      SInt32    filler;
      SInt16    attr;
      SInt16    filler2;
    } def;    // type = 0
    struct
    {
      SInt16    w;
      SInt16    h;
      SInt32    actionPos;
      SInt32    namePos;
      SInt32    filler;
      SInt16    attr;
      SInt32    icon1Pos;
      SInt32    icon2Pos;
      SInt32    icon3Pos;
      SInt32    icon4Pos;
      SInt8     inactiveUnselColor;
      SInt8     inactiveSelColor;
      SInt8     activeUnselColor;
      SInt8     activeSelColor;
    } button; // type = 1
  };
} sPanelSubStruct;

typedef struct
{
  SInt16    size;       // size of the structure in bytes
  SInt16    w;
  SInt16    h;
  SInt16    mode;
  char      data[];
} sIconStruct;

typedef struct {
  SInt8 type;         // 0x01 = CODE, 0x04 = DELAY, 0x05 = ALIAS, etc.
  SInt32  p1;
  SInt32  p2;
} sActionStruct;

typedef struct
{
  SInt8   count;        // number of entries, always (?) == count2
  SInt8   count2;
  sActionStruct data[];
} sActionListStruct;

typedef struct
{
  SInt16  size;         // size of the structure in bytes
  SInt32  namePos;
  SInt16  details;      // 0x0000 = pulse-width coding
                        // 0x0100 = pulse-position coding
                        // 0x5000 = RC5 (phase modulation)
                        // 0x5001 = RC5x (phase modulation)
                        // 0x6000 = RC6 (phase modulation)
  SInt16  carrier;
  SInt16  onceCounter;
  SInt16  repeatCounter;
  union {
    struct
    {
      SInt16  device;
      SInt16  command;
    } RC5;
    struct
    {
      SInt16  device;
      SInt16  command;
      SInt16  data;
    } RC5x;
    struct
    {
      SInt16  device;
      SInt16  command;
    } RC6;
    struct
    {
      SInt16  data[];
    } LRN;
  };
} sIRCodeStruct;

typedef struct
{
  SInt32  zero;
  SInt16  sDays;
  SInt16  sTime;
  SInt16  eDays;
  SInt16  eTime;
  sActionStruct sAction;
  sActionStruct eAction;
} sTimerStruct;

/***
 *
 ***/
const char  *gData;
#if CHECK_LEAKEDMEM
bool        *gUse;
long        gFileSize;
#endif

/***
 *
 ***/
static inline const char            *GetPosPtr(SInt32 pos)
{
  return reinterpret_cast<const char*>(pos + gData);
}

static inline const sHeaderStruct   *GetHeader(SInt32 pos)
{
  const sHeaderStruct *p = reinterpret_cast<const sHeaderStruct*>(GetPosPtr(pos));
  assert(p);
#if CHECK_LEAKEDMEM
  memset(gUse + pos, 0xFF, sizeof(sHeaderStruct));
#endif
  return p;
}

static inline UInt16                GetCRC(SInt32 pos)
{
#if CHECK_LEAKEDMEM
  memset(gUse + pos, 0xFF, sizeof(UInt16));
#endif
  return *reinterpret_cast<const UInt16*>(gData + pos);
}

static inline const sPanelStruct    *GetPanel(SInt32 pos)
{
  const sPanelStruct  *p = reinterpret_cast<const sPanelStruct*>(GetPosPtr(pos));
  assert(p);
#if CHECK_LEAKEDMEM
  memset(gUse + pos, 0xFF, sizeof(sPanelStruct) + 9 * p->count);
#endif
  return p;
}

static inline const sEntryStruct    *GetEntry(SInt32 pos)
{
  const sEntryStruct  *p = reinterpret_cast<const sEntryStruct*>(GetPosPtr(pos));
  assert(p);
#if CHECK_LEAKEDMEM
  memset(gUse + pos, 0xFF, sizeof(sEntryStruct));
#endif
  return p;
}

static inline const sActionListStruct *GetActionList(SInt32 pos)
{
  const sActionListStruct *p = reinterpret_cast<const sActionListStruct*>(GetPosPtr(pos));
  assert(p);
#if CHECK_LEAKEDMEM
  memset(gUse + pos, 0xFF, sizeof(sActionListStruct) + p->count * sizeof(sActionStruct));
#endif
  return p;
}

static inline const sIRCodeStruct   *GetIRCode(SInt32 pos)
{
  const sIRCodeStruct *p = reinterpret_cast<const sIRCodeStruct*>(GetPosPtr(pos));
  assert(p);
#if CHECK_LEAKEDMEM
  memset(gUse + pos, 0xFF, p->size);
#endif
  return p;
}

static inline const sPanelSubStruct *GetSubPanel(SInt32 pos)
{
  const sPanelSubStruct *p = reinterpret_cast<const sPanelSubStruct*>(GetPosPtr(pos));
  assert(p);
#if CHECK_LEAKEDMEM
  memset(gUse + pos, 0xFF, sizeof(sPanelSubStruct));
#endif
  return p;
}

static inline const sIconStruct     *GetIcon(SInt32 pos)
{
  const sIconStruct     *p = reinterpret_cast<const sIconStruct*>(GetPosPtr(pos));
  assert(p);
#if CHECK_LEAKEDMEM
  memset(gUse + pos, 0xFF, p->size);
#endif
  return p;
}

static inline const sTimerStruct    *GetTimerEntry(SInt32 pos)
{
  const sTimerStruct *p = reinterpret_cast<const sTimerStruct*>(GetPosPtr(pos));
  assert(p);
#if CHECK_LEAKEDMEM
  memset(gUse + pos, 0xFF, sizeof(sTimerStruct));
#endif
  return p;
}

// return a ptr to a _pascal_ string (first byte = length, second byte = 1. char, third byte = 2. char, etc.)
static inline const char            *GetCCFString(SInt32 pos)
{
  if(pos == 0) return reinterpret_cast<const char*>("\p<NIL>"); // illegal position => <NIL> string

  const char            *p = reinterpret_cast<const char*>(GetPosPtr(pos));
  assert(p);
#if CHECK_LEAKEDMEM
  memset(gUse + pos, 0xFF, p[0] + 1);
#endif
  return p;
}

/***
 *
 ***/
static void PrintAction(const sActionStruct *si)
{
  const sIRCodeStruct   *irs;
  const sEntryStruct    *es;
  const sPanelStruct    *ps;
  const sPanelSubStruct *pss;
  const sTimerStruct    *ts;

  switch(si->type)
  {
  case 0x01:  // Code
    irs = GetIRCode(si->p2);
    printf("[C] %#s ", GetCCFString(irs->namePos));
    switch(irs->details)
    {
    case 0x0100:  // pulse-position coding
        printf("(pulse-position coding)\n");
        goto cont;
    case 0x0000:  // learned
        printf("(pulse-width coding)\n");
    cont:
#if 0
        int   index;
        printf("Carrier: %x\n", irs->carrier);
        printf("Once: ");
        for(index=0; index < irs->onceCounter*2; index += 2)
          printf("%.4x/%.4x ", irs->LRN.data[index], irs->LRN.data[index+1]);
        printf("\n");
        printf("Repeat: ");
        for(int i=0; i < irs->repeatCounter*2; i += 2)
          printf("%.4x/%.4x ", irs->LRN.data[i + index], irs->LRN.data[i + index + 1]);
        printf("\n");
#endif
        break;
    case 0x5000:
        printf("%d %d\n", irs->RC5.device, irs->RC5.command);
        break;
    case 0x5001:
        printf("%d %d %d\n", irs->RC5x.device, irs->RC5x.command, irs->RC5x.data);
        break;
    case 0x6000:
        printf("%d %d\n", irs->RC6.device, irs->RC6.command);
        break;
    default:
        printf("unknown: %x\n", irs->details);
        printf("Carrier: %x\n", irs->carrier);
        printf("Once   : %x\n", irs->onceCounter);
        printf("Repeat : %x\n", irs->repeatCounter);
        break;
    }
    break;

  case 0x02:  // Button
    es = GetEntry(si->p1);
    if(es->namePos) printf("[B] %#s - ", GetCCFString(es->namePos));
    pss = GetSubPanel(si->p2);
    if(pss->button.namePos) printf("%#s", GetCCFString(pss->button.namePos));
    printf("\n");
    break;

  case 0x03:  // Jump (last possible action in a macro)
    printf("Jump: ");
    switch(si->p2)
    {
    default:
      es = GetEntry(si->p1);
      if(es->namePos) printf("%#s - ", GetCCFString(es->namePos));
      ps = GetPanel(si->p2);
      if(ps->namePos) printf("%#s", GetCCFString(ps->namePos));
      printf("\n");
      break;
    case 0xdddddddd:
      printf("Scroll down");
      break;
    case 0xEEEEEEEE:
      printf("Scroll up");
      break;
    case 0xFFFFFFFF:
      printf("Mouse mode");
      break;
    }
    printf("\n");
    break;

  case 0x04:  // Delay
    // p2 contains the delay in ms
    printf("[D] %.1f sec\n", float(si->p2) / 1000);
    break;

  case 0x05:  // Alias
    printf("[K] ");
    es = GetEntry(si->p1);
    if(es->namePos) printf("%#s - ", GetCCFString(es->namePos));
    switch(si->p2)
    {
    case 0: printf("left"); break;
    case 1: printf("right"); break;
    case 2: printf("Vol-"); break;
    case 3: printf("Vol+"); break;
    case 4: printf("Chan-"); break;
    case 5: printf("Chan+"); break;
    case 6: printf("Mute"); break;
    default:printf("unknown(%ld)", si->p2); break;
    }
    printf("\n");
    break;

  case 0x06:  // Jump Device
    es = GetEntry(si->p1);
    if(es->namePos) printf("[A] %#s\n", GetCCFString(es->namePos));
    break;

  case 0x07:  // Timer
    printf("[T] TimerAction\n");
    ts = GetTimerEntry(si->p2);
    printf("Start: ");
    PrintAction(&ts->sAction);
    static const char *dayArr[] = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Son", "Wkly" };
    printf("       ");
    for(long i=0x0100,j=0; i<=0x8000; i<<=1,++j)
      if(ts->sDays & i) printf("%s ", dayArr[j]);
    printf("%d:%d\n", ts->sTime / 60, ts->sTime % 60);
    printf("Stop : ");
    PrintAction(&ts->eAction);
    printf("       ");
    for(long i=0x0100,j=0; i<=0x8000; i<<=1,++j)
      if(ts->eDays & i) printf("%s ", dayArr[j]);
    printf("%d:%d\n", ts->eTime / 60, ts->eTime % 60);
    break;

  case 0x08:  // Beep
    SInt32  val = si->p2;
    printf("[S] %ld Hz %ld %% %ld0 ms\n", (val >> 8) & 0xFFFF, val & 0xFF, (val >> 24) & 0xFF);
    break;

  default:
    printf("unknown Type : %d : %ld/%lx\n", si->type, si->p1, si->p2);
    break;
  }
}

static void PrintActionList(ConstStr255Param name, SInt32 pos)
{
  printf("%#s: ", name);

  const sActionListStruct *sa = GetActionList(pos);
  for(int index=0; index<sa->count; ++index)
    PrintAction(&sa->data[index]);
}

/***
 *
 ***/
static void PrintIcon(SInt32 pos)
{
  const sIconStruct *si = GetIcon(pos);
#if 0
  printf("ICON(P:%lx,S:%d,W:%d,H:%d,M:%x)\n", pos, si->size, si->w, si->h, si->mode);
  // print some nice ascii art...
  const char  *dataPtr = si->data;
  int         bitOffset = (si->mode == 0x1800) ? 1 : 2; // b/w (mode == 0x1800) or 4 colors (mode == 0x0100)
  int         bitMask = (1 << bitOffset) - 1;
  const char  *bitMap = (bitOffset == 1) ? "# " : "#+. ";
  for(int h=0; h<si->h; ++h)
  {
    int   bitPos = 8 - bitOffset;
    for(int w=0; w<si->w; ++w)
    {
      putchar(bitMap[(*dataPtr >> bitPos) & bitMask]);
      bitPos -= bitOffset;
      if(bitPos < 0)
      {
        bitPos = 8 - bitOffset;
        dataPtr++;
      }
    }
    if(bitPos > 0)
      ++dataPtr;
    printf("\n");
  }
#endif
}

/***
 *  2 bit color index => string
 ***/
static const char   *GetColor(char color)
{
  switch(color & 3)
  {
  default:
  case 0: return "black";
  case 1: return "dkGray";
  case 2: return "ltGray";
  case 3: return "white";
  }
}

/***
 *  8 bit font index => string
 ***/
static const char   *GetFont(char font)
{
  switch(font)
  {
  case 0: return "<none>";
  case 1: return "Pronto 8";
  case 2: return "Pronto 10";
  case 3: return "Pronto 12";
  case 4: return "Pronto 14";
  case 5: return "Pronto 16";
  case 6: return "Pronto 18";
  default:return "Pronto xx";
  }
}

/***
 *
 ***/
static void PrintSubPanel(const sPanelStruct *ps, int index)
{
  assert(ps);
  const sPanelSubStruct *ps2 = GetSubPanel(ps->data[index].subPanelPos);
  switch(ps->data[index].type)
  {
  case 0:
    if(ps2->def.namePos) printf("Name : %#s\n", GetCCFString(ps2->def.namePos));
    printf("Frame:\n");
    printf("Pos  : (%d,%d)-(%d,%d)\n", ps->data[index].x, ps->data[index].y, ps2->def.w, ps2->def.h);
    printf("(Font:%s, Text;%s, Back:%s)\n", GetFont(ps2->def.attr >> 8), GetColor(ps2->def.attr), GetColor(ps2->def.attr >> 2));
    if(ps2->def.iconPos) PrintIcon(ps2->def.iconPos);
    break;
  case 1:
    if(ps2->button.namePos) printf("Name : %#s\n", GetCCFString(ps2->button.namePos));
    printf("Button:\n");
    printf("Pos  : (%d,%d)-(%d,%d)\n", ps->data[index].x, ps->data[index].y, ps2->button.w, ps2->button.h);
    printf("(Font:%s, Text;%s, Back:%s)\n", GetFont(ps2->button.attr >> 8), GetColor(ps2->button.attr), GetColor(ps2->button.attr >> 2));
    printf("(inUnCol:%s", GetColor(ps2->button.inactiveUnselColor));
    printf(", inSelCol:%s", GetColor(ps2->button.inactiveSelColor));
    printf(", actUnCol:%s", GetColor(ps2->button.activeUnselColor));
    printf(", actSelCol:%s)\n", GetColor(ps2->button.activeSelColor));
    if(ps2->button.icon1Pos) PrintIcon(ps2->button.icon1Pos);
    if(ps2->button.icon2Pos) PrintIcon(ps2->button.icon2Pos);
    if(ps2->button.icon3Pos) PrintIcon(ps2->button.icon3Pos);
    if(ps2->button.icon4Pos) PrintIcon(ps2->button.icon4Pos);
    if(ps2->button.actionPos) PrintActionList("\pAction", ps2->button.actionPos);
    break;
  }
}

/***
 *  Print panel with all subpanels
 ***/
static void PrintPanel(SInt32 pos)
{
  for(const sPanelStruct *ps = GetPanel(pos); ps != NULL; ps = GetPanel(ps->nextPos))
  {
    printf("Panelname : ");
    bool  isHidden = (ps->namePos & 0x80000000L) == 0x80000000L;
    if(isHidden) putchar('[');
    printf("%#s", GetCCFString(ps->namePos & 0x7FFFFFFFL));
    if(isHidden) putchar(']');
    printf("\n");

    for(int i=0; i<ps->count; ++i)
      PrintSubPanel(ps, i);

    if(ps->nextPos == NULL)
      break;
  }
}

/***
 *  Print an entry (device, macrolist, home, etc.) with panels
 ***/
static void PrintEntry(const sEntryStruct *dl, bool printName = true)
{
  assert(dl);
  if(printName) printf("Entryname : %#s", GetCCFString(dl->namePos));
  if(dl->attr)
  {
    printf(" (");
    bool  isPrinted = false;
    if(dl->attr & 0x40) { printf("Is Template"); isPrinted = true; }
    if(dl->attr & 0x01) { if(isPrinted) printf(","); printf("Is Read Only"); isPrinted = true; }
    if(dl->attr & 0x20) { if(isPrinted) printf(","); printf("Has Separator"); isPrinted = true; }
    printf(")");
  }
  printf("\n");
  if(dl->actionPos) PrintActionList("\pAction", dl->actionPos);
  if(dl->leftKeyPos) PrintActionList(dl->leftKeyNamePos ? StringPtr(GetCCFString(dl->leftKeyNamePos)) : "\pLeft", dl->leftKeyPos);
  if(dl->rightKeyPos) PrintActionList(dl->rightKeyNamePos ? StringPtr(GetCCFString(dl->rightKeyNamePos)) : "\pRight", dl->rightKeyPos);
  if(dl->VolMinusKeyPos) PrintActionList("\pVol-", dl->VolMinusKeyPos);
  if(dl->VolPlusKeyPos) PrintActionList("\pVol+", dl->VolPlusKeyPos);
  if(dl->ChanMinusKeyPos) PrintActionList("\pChan-", dl->ChanMinusKeyPos);
  if(dl->ChanPlusKeyPos) PrintActionList("\pChan+", dl->ChanPlusKeyPos);
  if(dl->MuteKeyPos) PrintActionList("\pMute", dl->MuteKeyPos);
  if(dl->panelsPos) PrintPanel(dl->panelsPos);

  puts("");
}

/***
 *  standard 16bit XModem CRC
 ***/
static UInt16 ComputeCrc(const char *bufptr, SInt32 count)
{
  assert(bufptr);
  UInt16  crc = 0;
  while(--count>=0)
  {
    crc ^= static_cast<UInt16>(*bufptr++) << 8;
    for(int i=0; i < 8; ++i)
    {
      if(crc & 0x8000)
        crc = (crc << 1) ^ 0x1021;
      else
        crc <<= 1;
    }

  }
  return crc;
}

/***
 *  load ccf file into ram
 ***/
static void   loadFile(const char *filename)
{
  FILE  *f = fopen(filename, "rb");
  assert(f);
  fseek(f, 0, SEEK_END);
  long  fSize = ftell(f); // get the filesize
  fseek(f, 0, SEEK_SET);

  // alloc memory for the ccf file
  if(gData) free(const_cast<char*>(gData));
  gData = reinterpret_cast<const char*>(malloc(fSize));
  assert(gData);

#if CHECK_LEAKEDMEM
  // alloc memory for used table (debugging)
  if(gUse) free(gUse);
  gUse = reinterpret_cast<bool*>(malloc(fSize));
  assert(gUse);
  memset(gUse, false, fSize);
  gFileSize = fSize;
#endif

  // load the ccf file into memory
  int count = fread(const_cast<char*>(gData), fSize, 1, f);
  assert(count == 1);
  fclose(f);
}

/***
 *
 ***/
static void   parseCCF()
{
  // get the fileheader
  const sHeaderStruct   *hs = GetHeader(0);
  assert(memcmp(hs->id, "@\245Z@_CCF", 8) == 0);
  printf("Version string: \"%#s\"\n", GetCCFString(hs->versionPos));
  printf("Modification date: %d:%.2d:%.2d %d.%d.%d\n", hs->hour, hs->minute, hs->seconds, hs->day, hs->month, hs->year);
  assert(strcmp(hs->id2, "CCF") == 0);
  if(hs->confProp & 2)
    puts("configuration is write-protected");
  if(hs->confProp & 4)
    puts("HOME panels are write-protected");

  // unknown values :-(
  assert(hs->filler1 == 0);
  assert(hs->filler2 == 0);
  assert(hs->filler3 == 0);
  assert(hs->filler4 == 0);
  printf("Version : %d\n", hs->version);

  // crc ok?
  if(GetCRC(hs->crcPos) != ComputeCrc(gData, hs->crcPos))
  {
    puts("Wrong CRC!");
    return;
  }

  puts("");

  if(hs->sysHomePos)
  {
    const sEntryStruct    *ss = GetEntry(hs->sysHomePos);
    printf("%#s:", GetCCFString(ss->namePos));
    PrintEntry(ss, false);
  }

  puts("");
  puts("DEVICES:");
  if(hs->devPos)
  {
    for(const sEntryStruct *dl = GetEntry(hs->devPos); dl != NULL; dl = GetEntry(dl->nextPos))
    {
      PrintEntry(dl);
      if(dl->nextPos == NULL)
        break;
    }
  }

  puts("");
  puts("MACRO GROUPS:");
  if(hs->macroPos)
  {
    for(const sEntryStruct *dl = GetEntry(hs->macroPos); dl != NULL; dl = GetEntry(dl->nextPos))
    {
      PrintEntry(dl);
      if(dl->nextPos == NULL)
        break;
    }
  }

  // ???
  if(hs->version == 1)
  {
    if(hs->macroXPosV1)
      PrintPanel(hs->macroXPosV1);
  } else {
    if(hs->macroXPosV2)
      PrintPanel(hs->macroXPosV2);
  }
}

/***
 *
 ***/
static void   PrintLeakedMemory()
{
#if CHECK_LEAKEDMEM
  // print all unused memory addresses (did we forget something or does ProntoEdit leak memory?)
  puts("");
  puts("leaked memory:");
  long  lastPos = -1;
  long  i;
  for(i=0; i<gFileSize; ++i)
  {
    if(gUse[i] == false)
    {
      if(lastPos < 0)
        lastPos = i;
    } else {
      if(lastPos >= 0)
      {
        if(lastPos != i-1)
          printf("%#lx-%#lx\n", lastPos, i-1);
        else
          printf("%#lx\n", lastPos);
        lastPos = -1;
      }
    }
  }
  if(lastPos >= 0)
  {
    if(lastPos != i-1)
      printf("%#lx-%#lx\n", lastPos, i-1);
    else
      printf("%#lx\n", lastPos);
  }
#endif
}

/***
 *
 ***/
int   main()
{
  loadFile(FNAME);
  parseCCF();
  PrintLeakedMemory();
}
