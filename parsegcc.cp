/* CCF = Cabernet Configuration File
   Version 4.8
       - with support for hidden panels)
       - with support for CCF version 2 (Beep and Timer actions)

   WANRING: only works on BIG-ENDIAN machines!!!
   (because the ccf file format is also big endian)

   done 1999 Markus Fritze, <http://www.markus-fritze.de/>

   Modified June 2001 by Michael Durket

      Now works properly on all machines regardless of their
      byte-order (uses netinet/in.h to provide endian conversion
      macros present in Unix systems and most other operating
      systems that have TCP/IP stacks)

      Corrected data structures (added fields for home bitmap,
      different field layouts in header depending on version,
      timer list pointer field in header).

      Changed leaked memory feature to be a map feature which
      details the layout order of a CCF file.

      Eliminated non-standard typedefs SInt32, UInt16, SInt16
      and SInt8 (most of which weren't really signed values
      anyway).

      Eliminated #pragmas, which are compiler-dependent.

                                                               */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <netinet/in.h>

typedef unsigned long offset;

typedef struct header
   {
      offset version;  /* CCF version string */
      unsigned long filler1;
      char id[8];      /* 0x40A55A405F434346 or "@\xA5Z@_CCF" */
      offset CRC;      /* offset to the checksum (same as end) */

      /* The date and time this CCF file was last modified */

      unsigned short year;
      unsigned char month;
      unsigned char day;
      unsigned char unused;
      unsigned char hour;
      unsigned char minute;
      unsigned char seconds;

      unsigned long filler3;
      char id2[4];      /* "CCF\0" */


      /* The following two fields indicate the Pronto App version major and
         minor numbers. (Minor version 2 is the one with Timer support). */

      unsigned short App_minor;
      unsigned short App_major;
      offset end;                 /* Address of 1 byte beyond the end of the CCF (points to CRC) */
      offset header_end;
      offset Home;
      offset Devices;
      offset Macros;
      union
         {
            struct
               {
                  unsigned long confProp;
                  offset macroXPos;
                  unsigned short unknown;
               } v1;

            struct
               {
                  offset Timer_list;
                  unsigned long confProp;
                  offset macroXPos;
                  unsigned short unknown;
               } v2;
         } u;
   } HEADER;

/* A device or macro is described by the following structure. The only
   actual difference in the Pronto between devices and macros is
   a macro cannot define values for the 2 soft keys (the left and
   right key) at the bottom of the Pronto. */

typedef struct device
   {
        offset next;
        offset name;
        offset home_bitmap_offset;
        unsigned long unknown1;
        offset actions;
        offset LeftSoftKey;
        offset RightSoftKey;
        offset VolumeDown;
        offset VolumeUp;
        offset ChannelDown;
        offset ChannelUp;
        offset Mute;
        unsigned long unknown2;
        offset LeftSoftKeyName;
        offset RightSoftKeyName;
        offset panels;
        unsigned char attr;
   } DEVICE;

/* An element is either a frame or button. An element descriptor
   describes the location of the element within its container
   (panel or frame), the element's type and a pointer to the
   element definition. */

typedef enum element_type
   {
      ELEMENT_TYPE_FRAME = 0,
      ELEMENT_TYPE_BUTTON = 1
   } ELEMENT_TYPE;

typedef struct element_descriptor
   {
      unsigned short x     __attribute__ ((__packed__));
      unsigned short y     __attribute__ ((__packed__));
      offset subPanelPos   __attribute__ ((__packed__));
      unsigned char type;
   } ELEMENT_DESCRIPTOR __attribute__ ((__packed__));

typedef struct panel
  {
     offset next;
     offset name;       /* name of the panel (Bit 31 is set when panel is hidden) */
     unsigned char count;   /* number of entries, always (?) == count2 */
     unsigned char count2;
     ELEMENT_DESCRIPTOR data[0];
   } PANEL __attribute__ ((__packed__));

typedef struct button
   {
      unsigned short w;
      unsigned short h;
      offset actionPos;
      offset name;
      unsigned long unknown;
      unsigned short attr;
      offset bitmap1 __attribute__ ((__packed__));
      offset bitmap2 __attribute__ ((__packed__));
      offset bitmap3 __attribute__ ((__packed__));
      offset bitmap4 __attribute__ ((__packed__));
      unsigned char inactiveUnselected;
      unsigned char inactiveSelected;
      unsigned char activeUnselected;
      unsigned char activeSelected;
   } BUTTON;

typedef struct frame
   {
      unsigned short w;
      unsigned short h;
      offset name;
      offset bitmap;
      unsigned long unknown;
      unsigned short attr;

      /* Remember that frames can contain other frames and buttons */

      unsigned char count;   /* number of entries, always (?) == count2 */
      unsigned char count2;
      ELEMENT_DESCRIPTOR data[0];
   } FRAME;

typedef struct element
   {
        union
        {
           FRAME def __attribute__ ((__packed__));
           BUTTON button __attribute__ ((__packed__));
        };
} ELEMENT __attribute__ ((__packed__));

typedef struct bitmap
   {
      unsigned short size;  /* size of the structure in bytes */
      unsigned short w;
      unsigned short h;

      /* The mode word is encoded as follows (I think):
         (Bits are numbered 0-7 with bit 0 the MSB, the low order
          bits of the mode word don't appear to be used, or may
          not even be part of the mode)

         0     If on, means the image is run length encoded (RLE)
               (appears to only apply to 4 color images)

         1-2   Don't know

         3-4   The color index of the '1' pixel color in a 2 color
               bitmap

         5-6   The color index of the '0' pixel color in a 2 color
               bitmap

           7   The number of bits/pixel - 1 (ie 0 for a 2 color bitmap
               and 1 for a 4 color bitmap).

         These are all guesses at this point based on experimentation on
         various CCF files.

         In ths special case where there is no data present for the bitmap,
         then the bitmap is completely filled in with the color specified in
         bits 3-4 (even for a 4 color bit map since with no data, there's
         no other way to figure out the colors).

         RLE bitmaps are described in the RLE routine later in this code.
      */

      unsigned short mode;
      unsigned char data[0];
   } BITMAP __attribute__ ((__packed__));

typedef enum action_type
   {
       ACTION_IR_CODE = 1,
       ACTION_BUTTON_ALIAS = 2,
       ACTION_PANEL_JUMP = 3,
       ACTION_DELAY = 4,
       ACTION_KEY_ALIAS = 5,
       ACTION_DEVICE_ALIAS = 6,
       ACTION_TIMER = 7,
       ACTION_BEEP = 8
   } ACTION_TYPE;

typedef struct ircode_action
   {
      unsigned long unused;
      offset ir_entry;
   } IRCODE_ACTION;

typedef struct button_alias_action
   {
       offset device;
       offset button;
   } BUTTON_ALIAS_ACTION;

typedef enum jump_code
   {
       SCROLL_DOWN_JUMP_CODE = 0xDDDDDDDD,
       SCROLL_UP_JUMP_CODE = 0xEEEEEEEE,
       MOUSE_MODE_JUMP_CODE = 0xFFFFFFFF
   } JUMP_CODE;

typedef struct jump_action
   {
       offset device;
       unsigned long jump_code_or_panel;
   } JUMP_ACTION;

typedef struct delay_action
   {
       unsigned long unused;
       unsigned long delay_in_ms;
   } DELAY_ACTION;

typedef enum key_alias
   {
      LEFT_KEY_ALIAS = 0,
      RIGHT_KEY_ALIAS = 1,
      VOLUME_DOWN_ALIAS = 2,
      VOLUME_UP_ALIAS = 3,
      CHANNEL_DOWN_ALIAS = 4,
      CHANNEL_UP_ALIAS = 5,
      MUTE_ALIAS = 6
   } KEY_ALIAS;

typedef struct key_alias_action
   {
      offset device;
      KEY_ALIAS key;
   } KEY_ALIAS_ACTION;

typedef struct device_alias_action
   {
      offset device;
      unsigned long unused;
   } DEVICE_ALIAS_ACTION;

typedef struct timer_action
   {
      unsigned long unused;
      offset timer_entry;
   } TIMER_ACTION;

typedef struct beep_action
   {
      unsigned long unused;
      unsigned long value;
   } BEEP_ACTION;

typedef struct unknown_action
   {
      unsigned long p1;
      unsigned long p2;
   } UNKNOWN_ACTION;

typedef struct action
   {
      unsigned char type;   /* See ACTION_TYPE above */
      union
         {
            IRCODE_ACTION ircode;
            BUTTON_ALIAS_ACTION button;
            JUMP_ACTION jump;
            DELAY_ACTION delay;
            KEY_ALIAS_ACTION key;
            DEVICE_ALIAS_ACTION device;
            TIMER_ACTION timer;
            BEEP_ACTION beep;
            UNKNOWN_ACTION unknown;
         } u __attribute__ ((__packed__));
   } ACTION __attribute__ ((__packed__));

typedef struct action_list
   {
      unsigned char count;  /* number of entries, always (?) == count2 */
      unsigned char count2;
      ACTION data[0] __attribute__ ((__packed__));
   } ACTION_LIST __attribute__ ((__packed__));

typedef struct ir_code
   {
      unsigned short size;  /* size of the structure in bytes */
      offset name __attribute__ ((__packed__));
      unsigned short details;                        // 0x0000 = pulse-width coding
                                                // 0x0100 = pulse-position coding
                                                // 0x5000 = RC5 (phase modulation)
                                                // 0x5001 = RC5x (phase modulation)
                                                // 0x6000 = RC6 (phase modulation)
      unsigned short carrier;
      unsigned short onceCounter;
      unsigned short repeatCounter;

      union
         {
            struct
               {
                  unsigned short device;
                  unsigned short command;
               } RC5;

            struct
               {
                  unsigned short device;
                  unsigned short command;
                  unsigned short data;
               } RC5x;

            struct
               {
                  unsigned short device;
                  unsigned short command;
               } RC6;

            struct
               {
                  unsigned short  data[0];
               } LRN;
         };
   } IR_CODE __attribute__ ((__packed__));

typedef struct day_time
   {
      unsigned short day_mask;
      unsigned short hours_mins;
   } DAY_TIME;

typedef struct timer
   {
      offset previous;
      DAY_TIME start;
      DAY_TIME stop;
      ACTION start_action __attribute__ ((__packed__));
      ACTION stop_action __attribute__ ((__packed__));
   } TIMER __attribute__ ((__packed__));

typedef struct
   {
      unsigned char length;
      unsigned char s[1];
   } String;

/***
 *
 ***/
unsigned char *gData;
unsigned char *gUse;
long  gFileSize;

/* A set of characters used to simulate colors for bitmaps when
   making ASCII drawings. */

static unsigned char color_map[4] = {'#', '+', '.', ' '};

/* A required forward reference since we recurse on frames */

static void PrintElement(ELEMENT_DESCRIPTOR *ed);

static inline unsigned char *GetPosPtr(offset pos)
   {
      return (ntohl(pos) + gData);
   }

static HEADER *GetHeader(offset pos)
   {
      HEADER *p = (HEADER *)(GetPosPtr(pos));

      assert(p);

      memset(gUse + ntohl(pos), 'H', sizeof(HEADER));

      return p;
   }

static unsigned short GetCRC(offset pos)
   {
      unsigned short *crcptr;

      memset(gUse + ntohl(pos), '?', sizeof(unsigned short));

      crcptr = (unsigned short *) GetPosPtr(pos);

      return (ntohs(*crcptr));
   }

static inline PANEL *GetPanel(offset pos)
   {
      PANEL *p = (PANEL *) GetPosPtr(pos);

      assert(p);

      memset(gUse + ntohl(pos), 'P', sizeof(PANEL) + (9 * p->count));

      return p;
  }

static inline DEVICE *GetEntry(offset pos)
   {
      DEVICE *p = (DEVICE *) GetPosPtr(pos);

      assert(p);

      memset(gUse + ntohl(pos), 'D', 65  /* gcc won't pack the structure */);

      return p;
   }

static inline ACTION_LIST *GetActionList(offset pos)
{
        ACTION_LIST *p = (ACTION_LIST*)(GetPosPtr(pos));
        assert(p);

        memset(gUse + ntohl(pos),
               'L',
               sizeof(ACTION_LIST) + (p->count * sizeof(ACTION)));

        return p;
}

static inline IR_CODE *GetIRCode(offset pos)
{
        IR_CODE *p = (IR_CODE*)(GetPosPtr(pos));

        assert(p);

        memset(gUse + ntohl(pos), 'I', ntohs(p->size));

        return p;
}

static inline ELEMENT *GetElement(offset pos, ELEMENT_TYPE type)
   {
      ELEMENT *p = (ELEMENT *) GetPosPtr(pos);

      assert(p);

      if (type == ELEMENT_TYPE_FRAME)
         {
            FRAME *f = (FRAME *) p;

            memset(gUse + ntohl(pos),
                   'F',
                   sizeof(FRAME) + (f->count * 9));
         }

      else
         memset(gUse + ntohl(pos), 'B', sizeof(BUTTON));

      return p;
   }

static inline BITMAP *GetBitmap(offset pos)
{
        BITMAP *p = (BITMAP *) GetPosPtr(pos);

        assert(p);

        memset(gUse + ntohl(pos), 'X', ntohs(p->size));

        return p;
}

static inline TIMER *GetTimerEntry(offset pos)
{
        TIMER *p = (TIMER*)(GetPosPtr(pos));

        assert(p);

        memset(gUse + ntohl(pos), 'T', sizeof(TIMER));

        return p;
}

static String *GetCCFString(offset pos)
   {
      static String null_string = {0, {0}};

      String *p = (String *) (GetPosPtr(pos));

      if (pos == 0)
         return &null_string;

       assert(p);

       memset(gUse + ntohl(pos), 'S', p->length + 1);

       return p;
    }

static void printfStringAtOffset(char *format, offset at)
   {
      String *s = GetCCFString(at);

      printf(format, s->length, s->length, s->s);
   }

static void printStringAtOffset(offset at)
   {
      printfStringAtOffset("%*.*s", at);
   }

static void printDayTime(DAY_TIME *p)
   {
      static char *days[] = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun", "Weekly" };
      int i;

      for (i = 0; i < 8; i++)
          if (ntohs(p->day_mask) & (0x0100 << i))
             printf("%s ", days[i]);

      printf("%d:%02d\n", ntohs(p->hours_mins) / 60, ntohs(p->hours_mins) % 60);
   }

/***
 *      2 bit color index => string
 ***/
static char *GetColor(char color)
{
        switch(color & 3)
        {
        default:
        case 0: return "Black";
        case 1: return "Dark Gray";
        case 2: return "Light Gray";
        case 3: return "White";
        }
}

/***
 *      8 bit font index => string
 ***/
static char *GetFont(char font)
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

static void printColor(char *s, unsigned color)
   {
      printf("%s Color %s", s, GetColor(color));
   }

static void printFont(unsigned font)
   {
      printf("Font %s\n",GetFont(font));
   }

static void printTextAttributes(unsigned font,
                                unsigned color)
   {
      printColor("Text", color);
      printf(", ");
      printFont(font);
   }

static void printTextAndBackgroundColors(char *s,
                                         unsigned char attr)
   {
      printf("%s Text Color %s, Background Color %s\n",
             s,
             attr >> 4,
             attr & 0x0F);
   }
/***
 *
 ***/
static void PrintAction(ACTION *si)
{
        IR_CODE *irs;
        DEVICE  *es;
        PANEL   *ps;
        ELEMENT *pss;

        switch(si->type)
        {
           case ACTION_IR_CODE:
              {
                 IR_CODE *irs;

                 irs = GetIRCode(si->u.ircode.ir_entry);

                 printfStringAtOffset("[C] %*.*s ", irs->name);

                switch(ntohs(irs->details))
                {
                case 0x0100:    // pulse-position coding
                                printf("(pulse-position coding)\n");
                                goto cont;
                case 0x0000:    // learned
                                printf("(pulse-width coding)\n");
                cont:
#if 0
                                int             index;
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
                                printf("%d %d\n", ntohs(irs->RC5.device), ntohs(irs->RC5.command));
                                break;
                case 0x5001:
                                printf("%d %d %d\n",
                                       ntohs(irs->RC5x.device),
                                       ntohs(irs->RC5x.command),
                                       ntohs(irs->RC5x.data));
                                break;
                case 0x6000:
                                printf("%d %d\n", ntohs(irs->RC6.device), ntohs(irs->RC6.command));
                                break;
                default:
                                printf("unknown: %x\n", ntohs(irs->details));
                                printf("Carrier: %x\n", ntohs(irs->carrier));
                                printf("Once   : %x\n", ntohs(irs->onceCounter));
                                printf("Repeat : %x\n", ntohs(irs->repeatCounter));
                                break;
                }
                }
                break;

        case ACTION_BUTTON_ALIAS:
           {
              DEVICE  *es;
              ELEMENT *pss;

              es = GetEntry(si->u.button.device);

              if (es->name)
                 printfStringAtOffset("[B] %*.*s - ", es->name);

              pss = GetElement(si->u.button.button, ELEMENT_TYPE_BUTTON);

              if (pss->button.name)
                 printStringAtOffset(pss->button.name);

              printf("\n");
           }
           break;

        case ACTION_PANEL_JUMP:      // Jump (last possible action in a macro)
           {
              printf("Jump: ");

              switch (si->u.jump.jump_code_or_panel)
                 {
                     case SCROLL_DOWN_JUMP_CODE:
                        printf("Scroll down");
                        break;

                     case SCROLL_UP_JUMP_CODE:
                        printf("Scroll up");
                        break;

                     case MOUSE_MODE_JUMP_CODE:
                        printf("Mouse mode");
                        break;

                    default:
                       {
                          DEVICE *es;
                          PANEL *ps;

                          es = GetEntry(si->u.jump.device);

                          if (es->name)
                             printfStringAtOffset("%*.*s - ", es->name);

                          ps = GetPanel(si->u.jump.jump_code_or_panel);

                          if (ps->name)
                             printStringAtOffset(ps->name);

                          printf("\n");
                        }
                        break;
                 }

              printf("\n");
           }
           break;

        case ACTION_DELAY:
           printf("[D] %.1f sec\n", (float) (ntohl(si->u.delay.delay_in_ms)) / 1000);
           break;

        case ACTION_KEY_ALIAS:
           {
              DEVICE  *es;
              static char *key_names[] =
                 {
                    "Left", "Right", "Vol-", "Vol+", "Chan-", "Chan+", "Mute"
                 };

              printf("[K] ");

              es = GetEntry(si->u.key.device);

              if (es->name)
                 printfStringAtOffset("%*.*s - ", es->name);

              if (     (ntohl(si->u.key.key) >= LEFT_KEY_ALIAS)
                    && (ntohl(si->u.key.key) <= MUTE_ALIAS)
                 )
                 printf(key_names[ntohl(si->u.key.key)]);

              else
                 printf("Unknown code value %d", ntohl(si->u.key.key));

              printf("\n");
           }
           break;

        case ACTION_DEVICE_ALIAS:
           {
              DEVICE  *es;

              es = GetEntry(si->u.device.device);

              if (es->name)
                 printfStringAtOffset("[A] %*.*s - ", es->name);
           }
           break;

        case ACTION_TIMER:
           {
              TIMER *ts;

              printf("[T] TimerAction\n");

              ts = GetTimerEntry(si->u.timer.timer_entry);

              printf("Start: ");

              PrintAction(&ts->start_action);

              printf("       ");

              printDayTime(&ts->start);

              printf("Stop : ");

              PrintAction(&ts->stop_action);

              printf("       ");

              printDayTime(&ts->stop);
           }
           break;

        case ACTION_BEEP:
           {
              unsigned long val = ntohl(si->u.beep.value);

              printf("[S] %ld Hz %ld %% %ld0 ms\n", (val >> 8) & 0xFFFF, val & 0xFF, (val >> 24) & 0xFF);
           }
           break;

        default:
           printf("unknown Action Type : %d : %ld/%lx\n",
                  si->type,
                  si->u.unknown.p1,
                  si->u.unknown.p2);
           break;
        }
   }

static void PrintActionList(char *name, unsigned char name_length, offset pos)
   {
      ACTION_LIST *sa = GetActionList(pos);
      int i;

      printf("%*.*s: ", name_length, name_length, name);

      for (i = 0; i < sa->count; i++)
          PrintAction(&sa->data[i]);
   }

/* Bitmaps in the Pronto can be encoded or unencoded. The only encoded
   bitmaps appear to be the ones representing the buttons in the default
   macro group. All others are unencoded.

   The encoded bitmaps encode pixel runs of more than 4 consecutive pixels
   of the same value. Each byte represents an encoded run or just 3
   pixel values as follows:

      If encoded, the byte looks like this (with bit number 0 as the
      most-significant bit of the byte):

          Bit 0 = on  (means this byte is RLE encoded)
          Bits 1-3 run count - 1
          Bits 4-5 remainder after dividing run by 4
          Bits 6-7 the pixel value (0-3)

      If not encoded, bits 0-1 are both 0, and bits 2-3, 4-5 and 6-7
      represent the pixel values.

*/

static void PrintRLEBitmap(BITMAP *si)
   {
      unsigned char *dataPtr = si->data;
      unsigned char *s;
      unsigned char *mapstart;
      unsigned size;
      unsigned char *bitMap = "#+. ";
      int h;
      int i;

      mapstart = s = (unsigned char *) malloc(ntohs(si->h) * ntohs(si->w));

      assert(s);

      size = ntohs(si->size) - 8;   /* Number of encoded pixels */

      for (i = 0; i < size; i++)
          {
             if (*dataPtr & 0x80)
                {
                   int count;

                   count =    ((((*dataPtr & 0x70) >> 4) + 1) * 4)
                            + ((*dataPtr & 0x0C) >> 2);

                   memset(s, (*dataPtr & 0x03), count);

                   s += count;
                }

             else    /* Not encoded */
                {
                   s[0] = (*dataPtr & 0x30) >> 4;
                   s[1] = (*dataPtr & 0xC0) >> 2;
                   s[2] = (*dataPtr & 0x03);

                   s += 3;
                }

             dataPtr += 1;
          }

      s = mapstart;

      for (h = 0; h < ntohs(si->h); h++)
         {
            int w;

            for (w = 0; w < ntohs(si->w); w++)
                putchar(bitMap[*s++]);

            printf("\n");
         }

      free(mapstart);
   }

static void PrintNormalBitmap(BITMAP *si)
   {
      unsigned char bitMap[4];
      unsigned char *dataPtr = si->data;
      unsigned char mode_byte = (ntohs(si->mode) >> 8);
      int bits_per_pixel = (mode_byte & 0x01) + 1;
      int bitMask = (1 << bits_per_pixel) - 1;
      int h;

      if (bits_per_pixel == 1)
         {
            bitMap[0] = color_map[(mode_byte >> 1) & 0x03];
            bitMap[1] = color_map[(mode_byte >> 3) & 0x03];
         }

      else
         {
            memcpy(bitMap, color_map, sizeof bitMap);
         }

      for (h = 0; h < ntohs(si->h); h++)
         {
            int bitPos = 8;
            int w;

            for (w = 0; w < ntohs(si->w); w++)
                {
                   if (bitPos == 0)
                      {
                         dataPtr += 1;
                         bitPos = 8 - bits_per_pixel;
                      }

                   else
                      bitPos -= bits_per_pixel;

                   putchar(bitMap[((*dataPtr) >> bitPos) & bitMask]);
                }

            dataPtr += 1;

            /* Have to figure this out better, but apparently
               4 color data scan lines start on even byte boundaries.
               (Will 16 color or 256 color ones start on higher
                boundaries?) */

            if ((bits_per_pixel == 2) && (((unsigned long) dataPtr) & 1))
               dataPtr += 1;

            printf("\n");
         }
   }
static void PrintFilledBitmap(BITMAP *si)
   {
      unsigned char fill_color = color_map[(ntohs(si->mode) >> 11) & 0x03];
      int h;

      for (h = 0; h < ntohs(si->h); h++)
         {
            int w;

            for (w = 0; w < ntohs(si->w); w++)
                putchar(fill_color);

            printf("\n");
         }
   }

static void PrintBitmap(offset pos)
   {
      BITMAP *si = GetBitmap(pos);

      printf("BITMAP(P:0x%X,S:%d,W:%d,H:%d,M:0x%04X)\n",
             ntohl(pos),
             ntohs(si->size),
             ntohs(si->w),
             ntohs(si->h),
             ntohs(si->mode));

      if (ntohs(si->size) <= 8)
         {
            PrintFilledBitmap(si);
         }

      else
         {
            if (ntohs(si->mode) & 0x8000)  /* It's an RLE encoded bitmap */
               PrintRLEBitmap(si);

            else
               PrintNormalBitmap(si);
         }
   }

/***
 *
 ***/
static void PrintFrame(FRAME *f)
   {
      int i;

      if (f->name)
         printfStringAtOffset("Name : %*.*s\n", f->name);

      printf("Size : (%d,%d)\n", ntohs(f->w), ntohs(f->h));

      printTextAttributes((ntohs(f->attr) >> 8),
                          (ntohs(f->attr) & 0x03) >> 2);

      /* A frame has either a bitmap or a background color,
         but not both. If it has a bitmap, it cannot be
         resized. */

      if (f->bitmap)
         {
            printf("\n");
            PrintBitmap(f->bitmap);
            printf("\n");
         }

      else
         printColor("Background", (ntohs(f->attr) & 0x0C) >> 2);

      printf("\n");

      /* A frame can contain other frames and buttons... */

      for (i = 0; i < f->count2; i++)
          PrintElement(&(f->data[i]));
   }

static void PrintButton(BUTTON *b)
   {
      if (b->name)
         printfStringAtOffset("   Name: %*.*s\n", b->name);

      printf("   Size: (%d,%d)\n", ntohs(b->w), ntohs(b->h));

      printf("   ");

      printFont(ntohs(b->attr) >> 8);

      /* A button can from 0 to 4 bitmaps. If it has no bitmaps,
         it can be resized. */

      if (    (b->bitmap1)
           || (b->bitmap2)
           || (b->bitmap3)
           || (b->bitmap4)
         )
         {
            if (b->bitmap1)
               {
                  printf("\n");
                  PrintBitmap(b->bitmap1);
                  printf("\n");
               }

            if (b->bitmap2)
               {
                  printf("\n");
                  PrintBitmap(b->bitmap2);
                  printf("\n");
               }

            if (b->bitmap3)
               {
                  printf("\n");
                  PrintBitmap(b->bitmap3);
                  printf("\n");
               }

            if (b->bitmap4)
               {
                  printf("\n");
                  PrintBitmap(b->bitmap4);
                  printf("\n");
               }
         }

      else   /* No bitmaps */
         {
            printTextAndBackgroundColors("Inactive Unselected",
                                         b->inactiveUnselected);

            printTextAndBackgroundColors("Inactive Selected",
                                         b->inactiveSelected);

            printTextAndBackgroundColors("Active Unselected",
                                         b->activeUnselected);

            printTextAndBackgroundColors("Active Selected",
                                         b->activeSelected);
         }

      if (b->actionPos)
         PrintActionList("   Action", sizeof "Action" - 1, b->actionPos);
   }

static void PrintElement(ELEMENT_DESCRIPTOR *ed)
   {
        ELEMENT *ps2 = GetElement(ed->subPanelPos, ed->type);

        switch (ed->type)
           {
              case ELEMENT_TYPE_FRAME:
                 printf("Frame:\n");

                 printf("Pos: (%d,%d)\n", ntohs(ed->x), ntohs(ed->y));

                 PrintFrame(&ps2->def);
                 break;

              case ELEMENT_TYPE_BUTTON:
                 printf("Button:\n");

                 printf("   Position: (%d,%d)\n", ntohs(ed->x), ntohs(ed->y));

                 PrintButton(&ps2->button);
                 break;
           }
   }

/***
 *      Print panel with all elements
 ***/
static void PrintPanel(offset at)
   {
      unsigned long pos = at;

      while (pos)
         {
            PANEL *ps = GetPanel(pos);
            int isHidden = ((ntohl(ps->name) & 0x80000000L) == 0x80000000L);
            int i;

            printf("Panelname: ");

            if (isHidden)
               putchar('[');

            printStringAtOffset(htonl(ntohl(ps->name) & 0x7FFFFFFFL));

            if (isHidden)
               putchar(']');

            printf("\n");

            for (i = 0; i < ps->count; i++)
                PrintElement(&(ps->data[i]));

            pos = ps->next;
         }
   }

/***
 *      Print an entry (device, macrolist, home, etc.) with panels
 ***/
static void PrintDevice(DEVICE *d, int printName)
   {
      assert(d);

      if (printName)
         printfStringAtOffset("%*.*s", d->name);

      if (d->attr)
         {
            printf(" (");

            if (d->attr & 0x40)
               printf("Template");

            if (d->attr & 0x01)
               {
                  if (d->attr & 0x40)
                     putchar(',');

                  printf("Read Only");
               }

            if (d->attr & 0x20)
               {
                  if (d->attr != 0x20)
                     putchar(',');

                  printf("Separator");
               }

            printf(")");
         }

        printf("\n");

        if (d->actions)
           PrintActionList("Action", sizeof "Action" - 1, d->actions);

        if (d->LeftSoftKey)
           {
              if (d->LeftSoftKeyName)
                 {
                    String *s = GetCCFString(d->LeftSoftKeyName);

                    PrintActionList(s->s, s->length, d->LeftSoftKey);
                 }

              else
                 PrintActionList("Left", sizeof "Left" - 1, d->LeftSoftKey);
           }

        else
           {
              if (d->LeftSoftKeyName)   /* Account for the string in the map */
                 {
                    String *s = GetCCFString(d->LeftSoftKeyName);
                 }
           }

        if (d->RightSoftKey)
           {
              if (d->RightSoftKeyName)
                 {
                    String *s = GetCCFString(d->RightSoftKeyName);

                    PrintActionList(s->s, s->length, d->RightSoftKey);
                 }

              else
                 PrintActionList("Right", sizeof "Right" - 1, d->RightSoftKey);
           }

        else
           {
              if (d->RightSoftKeyName)   /* Account for the string in the map */
                 {
                    String *s = GetCCFString(d->RightSoftKeyName);
                 }
           }

        if (d->VolumeDown)
           PrintActionList("Vol-", sizeof "Vol-" - 1, d->VolumeDown);

        if (d->VolumeUp)
           PrintActionList("Vol+", sizeof "Vol+" - 1, d->VolumeUp);

        if (d->ChannelDown)
           PrintActionList("Chan-", sizeof "Chan-" - 1, d->ChannelDown);

        if (d->ChannelUp)
           PrintActionList("Chan+", sizeof "Chan+" - 1, d->ChannelUp);

        if (d->Mute)
           PrintActionList("Mute", sizeof "Mute" - 1, d->Mute);

        if (d->panels)
           PrintPanel(d->panels);

        puts("");
   }

/***
 *      standard 16bit XModem CRC
 ***/

static unsigned short ComputeCRC(unsigned char *bufptr, long count)
   {
      unsigned short crc = 0;

      assert(bufptr);

      while (--count >= 0)
         {
            int i;

            crc ^= (unsigned short) ((*bufptr++) << 8);

            for (i = 0; i < 8; ++i)
               {
                  if (crc & 0x8000)
                     crc = (crc << 1) ^ 0x1021;

                  else
                     crc <<= 1;
               }
         }

      return crc;
   }


static void parseCCF()
   {
      HEADER *hdr = GetHeader(0);
      unsigned long prop;

      assert(memcmp(hdr->id, "@\245Z@_CCF", 8) == 0);
      assert(strcmp(hdr->id2, "CCF") == 0);

      /* unknown values :-( */

      assert(hdr->filler1 == 0);
      assert(hdr->filler3 == 0);

      printfStringAtOffset("Version string: \"%*.*s\"\n", hdr->version);

      printf("Modification date: %d:%.2d:%.2d %d/%d/%d\n",
              hdr->hour,
              hdr->minute,
              hdr->seconds,
              hdr->month,
              hdr->day,
              ntohs(hdr->year));


      if (ntohl(hdr->App_minor) >= 2)
         prop = ntohl(hdr->u.v2.confProp);
      else
         prop = ntohl(hdr->u.v1.confProp);

      if (prop & 2)
         puts("Configuration is write-protected");

      if (prop & 4)
         puts("HOME panels are write-protected");

      printf("Version : %d.%d\n", ntohs(hdr->App_major), ntohs(hdr->App_minor));

      // CRC ok?

      if (GetCRC(hdr->CRC) != ComputeCRC((unsigned char *) gData, ntohl(hdr->end)))
         {
            puts("Wrong CRC!");

            return;
         }

      puts("");

      if (hdr->Home)
        {
           DEVICE *ss = GetEntry(hdr->Home);

           printStringAtOffset(ss->name);

           PrintDevice(ss, 0);

           if (ss->home_bitmap_offset)
              {
                 printf("Home Bitmap:\n\n");
                 PrintBitmap(ss->home_bitmap_offset);
              }
        }

        puts("");

        puts("Devices:");

        if (hdr->Devices)
        {
                DEVICE *dl;
                for(dl = GetEntry(hdr->Devices); dl != NULL; dl = GetEntry(dl->next))
                {
                        PrintDevice(dl,1);
                        if(dl->next == 0)
                                break;
                }
        }

        puts("");

        puts("Macros:");

        if(hdr->Macros)
        {
                DEVICE *dl;
                for(dl = GetEntry(hdr->Macros); dl != NULL; dl = GetEntry(dl->next))
                {
                        PrintDevice(dl,1);
                        if(dl->next == 0)
                                break;
                }
        }

        /* Print the default macro group panel */

        if (ntohs(hdr->App_minor) >= 2)
           {
              if(hdr->u.v2.macroXPos)
                PrintPanel(hdr->u.v2.macroXPos);
           }

        else
           {
              if  (hdr->u.v1.macroXPos)
                 PrintPanel(hdr->u.v1.macroXPos);
           }
   }

/* Print a file containing a marker byte for every byte in the
   CCF file that shows the type of data (header, bitmap, bitmap
   panel, etc) that is contained in the CCF file. This helps
   in determining how a CCF file is arranged in memory by the
   ProntoEdit software. */

static void PrintMapFile(char *fn)
   {
      FILE *f = fopen(fn, "w");

      assert(f);

      fwrite(gUse, gFileSize, 1, f);

      fclose(f);
   }

/***
 *      load ccf file into ram
 ***/
static void loadFile(char *filename)
   {
      FILE *f = fopen(filename, "rb");
      long fSize;
      int count;

      assert(f);

      fseek(f, 0, SEEK_END);

      fSize = ftell(f);

      fseek(f, 0, SEEK_SET);

      /* Get enough memory to read the entire file */

      gData = (unsigned char *) malloc(fSize);

      assert(gData);

      /* Get memory for the map data structure */

      gUse = (unsigned char *) malloc(fSize);

      assert(gUse);

      memset(gUse, 0, fSize);

      gFileSize = fSize;

      /* Load the ccf file into memory */

      count = fread((unsigned char *) gData, fSize, 1, f);

      assert(count == 1);

      fclose(f);
   }

int main(int argc,
         char *argv[])
   {
      loadFile(argv[1]);

      parseCCF();

      if (argc > 2)
         PrintMapFile(argv[2]);
   }
