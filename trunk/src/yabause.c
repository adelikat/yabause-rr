/*  Copyright 2003-2005 Guillaume Duhamel
    Copyright 2004-2006 Theo Berkau
    Copyright 2006      Anders Montonen

    This file is part of Yabause.

    Yabause is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Yabause is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Yabause; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <sys/types.h>
#ifdef WIN32
#include <windows.h>
#endif
#include <string.h>
#include "yabause.h"
#include "cheat.h"
#include "cs0.h"
#include "cs2.h"
#include "debug.h"
#include "error.h"
#include "memory.h"
#include "m68kcore.h"
#include "peripheral.h"
#include "scsp.h"
#include "scu.h"
#include "sh2core.h"
#include "smpc.h"
#include "vdp2.h"
#include "yui.h"
#include "bios.h"
#include "movie.h"
#ifdef HAVE_LIBSDL
 #ifdef __APPLE__
  #include <SDL/SDL.h>
 #else
  #include "SDL.h"
 #endif
#endif
#if defined(_MSC_VER) || !defined(HAVE_SYS_TIME_H)
#include <time.h>
#else
#include <sys/time.h>
#endif
#ifdef _arch_dreamcast
#include <arch/timer.h>
#endif
#ifdef GEKKO
#include <ogc/lwp_watchdog.h>
extern long long gettime();
#endif
#ifdef PSP
#include "psp/common.h"
#endif

#ifdef SYS_PROFILE_H
 #include SYS_PROFILE_H
#else
 #define DONT_PROFILE
 #include "profile.h"
#endif

void FCEUMOV_AddInputState();

//////////////////////////////////////////////////////////////////////////////

yabsys_struct yabsys;
const char *bupfilename = NULL;
u64 tickfreq;

//////////////////////////////////////////////////////////////////////////////

#ifndef NO_CLI
void print_usage(const char *program_name) {
   printf("Yabause v" VERSION "\n");
   printf("\n"
          "Purpose:\n"
          "  This program is intended to be a Sega Saturn emulator\n"
          "\n"
          "Usage: %s [OPTIONS]...\n", program_name);
   printf("   -h         --help                 Print help and exit\n");
   printf("   -b STRING  --bios=STRING          bios file\n");
   printf("   -i STRING  --iso=STRING           iso/cue file\n");
   printf("   -c STRING  --cdrom=STRING         cdrom path\n");
   printf("   -ns        --nosound              turn sound off\n");
   printf("   -a         --autostart            autostart emulation\n");
   printf("   -f         --fullscreen           start in fullscreen mode\n");
}
#endif

//////////////////////////////////////////////////////////////////////////////

void YabauseChangeTiming(int freqtype) {
   // Setup all the variables related to timing
   yabsys.DecilineCount = 0;
   yabsys.LineCount = 0;
   yabsys.CurSH2FreqType = freqtype;

   switch (freqtype)
   {
      case CLKTYPE_26MHZ:
         if (!yabsys.IsPal)
            yabsys.DecilineStop = 26846587 / 263 / 10 / 60; // I'm guessing this is wrong
         else
            yabsys.DecilineStop = 26846587 / 312 / 10 / 50; // I'm guessing this is wrong

         yabsys.Duf = 26846587 / 100000;

         break;
      case CLKTYPE_28MHZ:
         if (!yabsys.IsPal)
            yabsys.DecilineStop = 28636360 / 263 / 10 / 60; // I'm guessing this is wrong
         else
            yabsys.DecilineStop = 28636360 / 312 / 10 / 50; // I'm guessing this is wrong

         yabsys.Duf = 28636360 / 100000;
         break;
      default: break;
   }

   yabsys.CycleCountII = 0;
}

//////////////////////////////////////////////////////////////////////////////

int YabauseInit(yabauseinit_struct *init)
{     
   // Initialize both cpu's
   if (SH2Init(init->sh2coretype) != 0)
   {
      YabSetError(YAB_ERR_CANNOTINIT, _("SH2"));
      return -1;
   }

   if ((BiosRom = T2MemoryInit(0x80000)) == NULL)
      return -1;

   if ((HighWram = T2MemoryInit(0x100000)) == NULL)
      return -1;

   if ((LowWram = T2MemoryInit(0x100000)) == NULL)
      return -1;

   if ((BupRam = T1MemoryInit(0x10000)) == NULL)
      return -1;

   if (LoadBackupRam(init->buppath) != 0)
      FormatBackupRam(BupRam, 0x10000);

   bupfilename = init->buppath;

   if (VideoInit(init->vidcoretype) != 0)
   {
      YabSetError(YAB_ERR_CANNOTINIT, _("Video"));
      return -1;
   }

   // Initialize input core
   if (PerInit(init->percoretype) != 0)
   {
      YabSetError(YAB_ERR_CANNOTINIT, _("Peripheral"));
      return -1;
   }

   if (CartInit(init->cartpath, init->carttype) != 0)
   {
      YabSetError(YAB_ERR_CANNOTINIT, _("Cartridge"));
      return -1;
   }

   if (Cs2Init(init->carttype, init->cdcoretype, init->cdpath, init->mpegpath, init->netlinksetting) != 0)
   {
      YabSetError(YAB_ERR_CANNOTINIT, _("CS2"));
      return -1;
   }

   if (ScuInit() != 0)
   {
      YabSetError(YAB_ERR_CANNOTINIT, _("SCU"));
      return -1;
   }

   if (M68KInit(init->m68kcoretype) != 0)
   {
      YabSetError(YAB_ERR_CANNOTINIT, _("M68K"));
      return -1;
   }

   if (ScspInit(init->sndcoretype) != 0)
   {
      YabSetError(YAB_ERR_CANNOTINIT, _("SCSP/M68K"));
      return -1;
   }

   if (Vdp1Init() != 0)
   {
      YabSetError(YAB_ERR_CANNOTINIT, _("VDP1"));
      return -1;
   }

   if (Vdp2Init() != 0)
   {
      YabSetError(YAB_ERR_CANNOTINIT, _("VDP2"));
      return -1;
   }

   if (SmpcInit(init->regionid) != 0)
   {
      YabSetError(YAB_ERR_CANNOTINIT, _("SMPC"));
      return -1;
   }

   if (CheatInit() != 0)
   {
      YabSetError(YAB_ERR_CANNOTINIT, _("Cheat System"));
      return -1;
   }

   MappedMemoryInit();

   YabauseSetVideoFormat(init->flags & 0x1);
   YabauseChangeTiming(CLKTYPE_26MHZ);
   
   if (init->frameskip)
       EnableAutoFrameSkip();

   if (init->biospath != NULL && strlen(init->biospath))
   {
      if (LoadBios(init->biospath) != 0)
      {
         YabSetError(YAB_ERR_FILENOTFOUND, (void *)init->biospath);
         return -2;
      }
      yabsys.emulatebios = 0;
   }
   else
      yabsys.emulatebios = 1;

   yabsys.usequickload = 0;

   YabauseResetNoLoad();

   if (yabsys.usequickload || yabsys.emulatebios)
   {
      if (YabauseQuickLoadGame() != 0)
      {
         if (yabsys.emulatebios)
         {
            YabSetError(YAB_ERR_CANNOTINIT, _("Game"));
            return -2;
         }
         else
            YabauseResetNoLoad();
      }
   }

   return 0;
}

//////////////////////////////////////////////////////////////////////////////

void YabauseDeInit(void) {
   SH2DeInit();

   if (BiosRom)
      T2MemoryDeInit(BiosRom);
   BiosRom = NULL;

   if (HighWram)
      T2MemoryDeInit(HighWram);
   HighWram = NULL;

   if (LowWram)
      T2MemoryDeInit(LowWram);
   LowWram = NULL;

   if (BupRam)
   {
      if (T123Save(BupRam, 0x10000, 1, bupfilename) != 0)
         YabSetError(YAB_ERR_FILEWRITE, (void *)bupfilename);

      T1MemoryDeInit(BupRam);
   }
   BupRam = NULL;

   CartDeInit();
   Cs2DeInit();
   ScuDeInit();
   ScspDeInit();
   Vdp1DeInit();
   Vdp2DeInit();
   SmpcDeInit();
   PerDeInit();
   VideoDeInit();
   CheatDeInit();
}

//////////////////////////////////////////////////////////////////////////////

void YabauseResetNoLoad(void) {
   SH2Reset(&MSH2);
   YabauseStopSlave();
   memset(HighWram, 0, 0x100000);
   memset(LowWram, 0, 0x100000);

   // Reset CS0 area here
   // Reset CS1 area here
   Cs2Reset();
   ScuReset();
   ScspReset();
   yabsys.IsM68KRunning = 0;
   Vdp1Reset();
   Vdp2Reset();
   SmpcReset();

   SH2PowerOn(&MSH2);
}

//////////////////////////////////////////////////////////////////////////////

void YabauseReset(void) {
   YabauseResetNoLoad();

   currFrameCounter=0;

   if (yabsys.usequickload || yabsys.emulatebios)
   {
      if (YabauseQuickLoadGame() != 0)
      {
         if (yabsys.emulatebios)
            YabSetError(YAB_ERR_CANNOTINIT, _("Game"));
         else
            YabauseResetNoLoad();
      }
   }
}

//////////////////////////////////////////////////////////////////////////////

void YabauseResetButton(void) {
   // This basically emulates the reset button behaviour of the saturn. This
   // is the better way of reseting the system since some operations (like
   // backup ram access) shouldn't be interrupted and this allows for that.

   SmpcResetButton();
}

//////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////

int M68k_centicycles;  // Fractional cycle counter for M68k

int YabauseExec(void) {
   int M68k_cycles;              // Integral M68k cycles to execute this time
   

   int oneframeexec=0;

//   DoMovie();
   FCEUMOV_AddInputState();

   while (!oneframeexec)
   {
#ifdef NO_DECILINE
      const unsigned int sh2cycles = yabsys.DecilineStop * 10;
#else
      const unsigned int sh2cycles = yabsys.DecilineStop;
#endif

      PROFILE_START("Total Emulation");

#ifdef NO_DECILINE

      PROFILE_START("MSH2");
      SH2Exec(MSH2, sh2cycles - yabsys.DecilineStop);
      PROFILE_STOP("MSH2");
      PROFILE_START("SSH2");
      if (yabsys.IsSSH2Running)
         SH2Exec(SSH2, sh2cycles - yabsys.DecilineStop);
      PROFILE_STOP("SSH2");

      PROFILE_START("hblankin");
      Vdp2HBlankIN();
      PROFILE_STOP("hblankin");

      PROFILE_START("MSH2");
      SH2Exec(MSH2, yabsys.DecilineStop);
      PROFILE_STOP("MSH2");
      PROFILE_START("SSH2");
      if (yabsys.IsSSH2Running)
         SH2Exec(SSH2, yabsys.DecilineStop);
      PROFILE_STOP("SSH2");

#else // !NO_DECILINE

      PROFILE_START("MSH2");
      SH2Exec(&MSH2, sh2cycles);
      PROFILE_STOP("MSH2");

      PROFILE_START("SSH2");
      if (yabsys.IsSSH2Running)
         SH2Exec(&SSH2, sh2cycles);
      PROFILE_STOP("SSH2");

#endif

      PROFILE_START("SCU");
      ScuExec(sh2cycles / 2);
      PROFILE_STOP("SCU");

#ifndef NO_DECILINE
      yabsys.DecilineCount++;
      if(yabsys.DecilineCount == 9)
      {
         // HBlankIN
         PROFILE_START("hblankin");
         Vdp2HBlankIN();
         PROFILE_STOP("hblankin");
      }
      else if (yabsys.DecilineCount == 10)
#endif
      {
         // HBlankOUT
         PROFILE_START("hblankout");
         Vdp2HBlankOUT();
         PROFILE_STOP("hblankout");
         PROFILE_START("SCSP");
         ScspExec();
         PROFILE_STOP("SCSP");
         yabsys.DecilineCount = 0;
         yabsys.LineCount++;
         if (yabsys.LineCount == yabsys.VBlankLineCount)
         {
            PROFILE_START("vblankin");
            // VBlankIN
            SmpcINTBACKEnd();
            Vdp2VBlankIN();
            PROFILE_STOP("vblankin");
            CheatDoPatches();
         }
         else if (yabsys.LineCount == yabsys.MaxLineCount)
         {
            // VBlankOUT
            PROFILE_START("VDP1/VDP2");
            Vdp2VBlankOUT();
            yabsys.LineCount = 0;
            oneframeexec = 1;
            PROFILE_STOP("VDP1/VDP2");
         }
      }

#ifdef PSP_TIMING_TWEAKS
      yabsys.CycleCountII += sh2cycles;
#else
      yabsys.CycleCountII += sh2cycles + MSH2.cycles;
#endif

      {
         u32 usec = 0;
         while (yabsys.CycleCountII > yabsys.Duf)
         {
             yabsys.CycleCountII -= yabsys.Duf;
             usec += 10;
         }
         if (usec > 0)
         {
            PROFILE_START("SMPC");
            SmpcExec(usec);
            PROFILE_STOP("SMPC");
            PROFILE_START("CDB");
            Cs2Exec(usec);
            PROFILE_STOP("CDB");
         }
      }

      PROFILE_START("68K");
#ifdef NO_DECILINE
      if (yabsys.IsPal)
      {
         /* 11.2896MHz / 50Hz / 262.5 lines / 10 calls/line = 86.01 cycles/call */
         M68k_cycles = 860;
         M68k_centicycles += 10;
      }
      else
      {
         /* 11.2896MHz / 60Hz / 262.5 lines / 10 calls/line = 71.68 cycles/call */
         M68k_cycles = 716;
         M68k_centicycles += 80;
      }
      if (M68k_centicycles >= 100) {
         M68k_cycles++;
         M68k_centicycles -= 100;
      }
#else
      if (yabsys.IsPal)
      {
         /* 11.2896MHz / 50Hz / 262.5 lines / 10 calls/line = 86.01 cycles/call */
         M68k_cycles = 86;
         M68k_centicycles += 1;
      }
      else
      {
         /* 11.2896MHz / 60Hz / 262.5 lines / 10 calls/line = 71.68 cycles/call */
         M68k_cycles = 71;
         M68k_centicycles += 68;
      }
      if (M68k_centicycles >= 100) {
         M68k_cycles++;
         M68k_centicycles -= 100;
      }
#endif
      M68KExec(M68k_cycles);
      PROFILE_STOP("68K");

      PROFILE_STOP("Total Emulation");
   }
   return 0;
}

//////////////////////////////////////////////////////////////////////////////

void YabauseStartSlave(void) {
   if (yabsys.emulatebios)
   {
      SSH2.regs.R[15] = 0x06001000;
      SSH2.regs.VBR = 0x06000400;
      SSH2.regs.PC = MappedMemoryReadLong(0x06000250);
   }
   else
      SH2PowerOn(&SSH2);

   yabsys.IsSSH2Running = 1;
}

//////////////////////////////////////////////////////////////////////////////

void YabauseStopSlave(void) {
   SH2Reset(&SSH2);
   yabsys.IsSSH2Running = 0;
}

//////////////////////////////////////////////////////////////////////////////

u64 YabauseGetTicks(void) {
#ifdef WIN32
   u64 ticks;
   QueryPerformanceCounter((LARGE_INTEGER *)&ticks);
   return ticks;
#elif defined(_arch_dreamcast)
   return (u64) timer_ms_gettime64();
#elif defined(GEKKO)  
   return gettime();
#elif defined(PSP)
   return sceKernelGetSystemTimeWide();
#elif defined(HAVE_GETTIMEOFDAY)
   struct timeval tv;
   gettimeofday(&tv, NULL);
   return (u64)tv.tv_sec * 1000000 + tv.tv_usec;
#elif defined(HAVE_LIBSDL)
   return (u64)SDL_GetTicks();
#endif
}

//////////////////////////////////////////////////////////////////////////////

void YabauseSetVideoFormat(int type) {
   yabsys.IsPal = type;
   yabsys.MaxLineCount = type ? 313 : 263;
#ifdef WIN32
   QueryPerformanceFrequency((LARGE_INTEGER *)&yabsys.tickfreq);
#elif defined(_arch_dreamcast)
   yabsys.tickfreq = 1000;
#elif defined(GEKKO)
   yabsys.tickfreq = secs_to_ticks(1);
#elif defined(PSP)
   yabsys.tickfreq = 1000000;
#elif defined(HAVE_GETTIMEOFDAY)
   yabsys.tickfreq = 1000000;
#elif defined(HAVE_LIBSDL)
   yabsys.tickfreq = 1000;
#endif
   yabsys.OneFrameTime = yabsys.tickfreq / (type ? 50 : 60);
   Vdp2Regs->TVSTAT = Vdp2Regs->TVSTAT | (type & 0x1);
   ScspChangeVideoFormat(type);
   YabauseChangeTiming(yabsys.CurSH2FreqType);
   lastticks = YabauseGetTicks();
}

//////////////////////////////////////////////////////////////////////////////

void YabauseSpeedySetup(void)
{
   u32 data;
   int i;

   if (yabsys.emulatebios)
      BiosInit();
   else
   {
      // Setup the vector table area, etc.(all bioses have it at 0x00000600-0x00000810)
      for (i = 0; i < 0x210; i+=4)
      {
         data = MappedMemoryReadLong(0x00000600+i);
         MappedMemoryWriteLong(0x06000000+i, data);
      }

      // Setup the bios function pointers, etc.(all bioses have it at 0x00000820-0x00001100)
      for (i = 0; i < 0x8E0; i+=4)
      {
         data = MappedMemoryReadLong(0x00000820+i);
         MappedMemoryWriteLong(0x06000220+i, data);
      }

      // I'm not sure this is really needed
      for (i = 0; i < 0x700; i+=4)
      {
         data = MappedMemoryReadLong(0x00001100+i);
         MappedMemoryWriteLong(0x06001100+i, data);
      }

      // Fix some spots in 0x06000210-0x0600032C area
      MappedMemoryWriteLong(0x06000234, 0x000002AC);
      MappedMemoryWriteLong(0x06000238, 0x000002BC);
      MappedMemoryWriteLong(0x0600023C, 0x00000350);
      MappedMemoryWriteLong(0x06000240, 0x32524459);
      MappedMemoryWriteLong(0x0600024C, 0x00000000);
      MappedMemoryWriteLong(0x06000268, MappedMemoryReadLong(0x00001344));
      MappedMemoryWriteLong(0x0600026C, MappedMemoryReadLong(0x00001348));
      MappedMemoryWriteLong(0x0600029C, MappedMemoryReadLong(0x00001354));
      MappedMemoryWriteLong(0x060002C4, MappedMemoryReadLong(0x00001104));
      MappedMemoryWriteLong(0x060002C8, MappedMemoryReadLong(0x00001108));
      MappedMemoryWriteLong(0x060002CC, MappedMemoryReadLong(0x0000110C));
      MappedMemoryWriteLong(0x060002D0, MappedMemoryReadLong(0x00001110));
      MappedMemoryWriteLong(0x060002D4, MappedMemoryReadLong(0x00001114));
      MappedMemoryWriteLong(0x060002D8, MappedMemoryReadLong(0x00001118));
      MappedMemoryWriteLong(0x060002DC, MappedMemoryReadLong(0x0000111C));
      MappedMemoryWriteLong(0x06000328, 0x000004C8);
      MappedMemoryWriteLong(0x0600032C, 0x00001800);

      // Fix SCU interrupts
      for (i = 0; i < 0x80; i+=4)
         MappedMemoryWriteLong(0x06000A00+i, 0x0600083C);
   }

   // Set the cpu's, etc. to sane states

   // Set CD block to a sane state
   Cs2Area->reg.HIRQ = 0xFC1;
   Cs2Area->isdiskchanged = 0;
   Cs2Area->reg.CR1 = (Cs2Area->status << 8) | ((Cs2Area->options & 0xF) << 4) | (Cs2Area->repcnt & 0xF);
   Cs2Area->reg.CR2 = (Cs2Area->ctrladdr << 8) | Cs2Area->track;
   Cs2Area->reg.CR3 = (Cs2Area->index << 8) | ((Cs2Area->FAD >> 16) & 0xFF);
   Cs2Area->reg.CR4 = (u16) Cs2Area->FAD; 
   Cs2Area->satauth = 4;

   // Set Master SH2 registers accordingly
   for (i = 0; i < 15; i++)
      MSH2.regs.R[i] = 0x00000000;
   MSH2.regs.R[15] = 0x06002000;
   MSH2.regs.SR.all = 0x00000000;
   MSH2.regs.GBR = 0x00000000;
   MSH2.regs.VBR = 0x06000000;
   MSH2.regs.MACH = 0x00000000;
   MSH2.regs.MACL = 0x00000000;
   MSH2.regs.PR = 0x00000000;

   // Set SCU registers to sane states
   ScuRegs->D1AD = ScuRegs->D2AD = 0;
   ScuRegs->D0EN = 0x101;
   ScuRegs->IST = 0x2006;
   ScuRegs->AIACK = 0x1;
   ScuRegs->ASR0 = ScuRegs->ASR1 = 0x1FF01FF0;
   ScuRegs->AREF = 0x1F;
   ScuRegs->RSEL = 0x1;

   // Set SMPC registers to sane states
   SmpcRegs->COMREG = 0x10;
   SmpcInternalVars->resd = 0;

   // Set VDP1 registers to sane states
   Vdp1Regs->EDSR = 3;
   Vdp1Regs->localX = 160;
   Vdp1Regs->localY = 112;
   Vdp1Regs->systemclipX2 = 319;
   Vdp1Regs->systemclipY2 = 223;

   // Set VDP2 registers to sane states
   memset(Vdp2Regs, 0, sizeof(Vdp2));
   Vdp2Regs->TVMD = 0x8000;
   Vdp2Regs->TVSTAT = 0x020A;
   Vdp2Regs->CYCA0L = 0x0F44;
   Vdp2Regs->CYCA0U = 0xFFFF;
   Vdp2Regs->CYCA1L = 0xFFFF;
   Vdp2Regs->CYCA1U = 0xFFFF;
   Vdp2Regs->CYCB0L = 0xFFFF;
   Vdp2Regs->CYCB0U = 0xFFFF;
   Vdp2Regs->CYCB1L = 0xFFFF;
   Vdp2Regs->CYCB1U = 0xFFFF;
   Vdp2Regs->BGON = 0x0001;
   Vdp2Regs->PNCN0 = 0x8000;
   Vdp2Regs->MPABN0 = 0x0303;
   Vdp2Regs->MPCDN0 = 0x0303;
   Vdp2Regs->ZMXN0.all = 0x00010000;
   Vdp2Regs->ZMYN0.all = 0x00010000;
   Vdp2Regs->ZMXN1.all = 0x00010000;
   Vdp2Regs->ZMYN1.all = 0x00010000;
   Vdp2Regs->BKTAL = 0x4000;
   Vdp2Regs->SPCTL = 0x0020;
   Vdp2Regs->PRINA = 0x0007;
   Vdp2Regs->CLOFEN = 0x0001;
   Vdp2Regs->COAR = 0x0200;
   Vdp2Regs->COAG = 0x0200;
   Vdp2Regs->COAB = 0x0200;
   VIDCore->Vdp2SetResolution(Vdp2Regs->TVMD);
   VIDCore->Vdp2SetPriorityNBG0(Vdp2Regs->PRINA & 0x7);
   VIDCore->Vdp2SetPriorityNBG1((Vdp2Regs->PRINA >> 8) & 0x7);
   VIDCore->Vdp2SetPriorityNBG2(Vdp2Regs->PRINB & 0x7);
   VIDCore->Vdp2SetPriorityNBG3((Vdp2Regs->PRINB >> 8) & 0x7);
   VIDCore->Vdp2SetPriorityRBG0(Vdp2Regs->PRIR & 0x7);
}

//////////////////////////////////////////////////////////////////////////////

int YabauseQuickLoadGame(void)
{
   partition_struct * lgpartition;
   u8 *buffer;
   u32 addr;
   u32 size;
   u32 blocks;
   unsigned int i, i2;
   dirrec_struct dirrec;

   Cs2Area->outconcddev = Cs2Area->filter + 0;
   Cs2Area->outconcddevnum = 0;

   // read in lba 0/FAD 150
   if ((lgpartition = Cs2ReadUnFilteredSector(150)) == NULL)
      return -1;

   // Make sure we're dealing with a saturn game
   buffer = lgpartition->block[lgpartition->numblocks - 1]->data;

   YabauseSpeedySetup();

   if (memcmp(buffer, "SEGA SEGASATURN", 15) == 0)
   {
      // figure out how many more sectors we need to read
      size = (buffer[0xE0] << 24) |
             (buffer[0xE1] << 16) |
             (buffer[0xE2] << 8) |
              buffer[0xE3];
      blocks = size >> 11;
      if ((size % 2048) != 0) 
         blocks++;


      // Figure out where to load the first program
      addr = (buffer[0xF0] << 24) |
             (buffer[0xF1] << 16) |
             (buffer[0xF2] << 8) |
              buffer[0xF3];

      // Free Block
      lgpartition->size = 0;
      Cs2FreeBlock(lgpartition->block[lgpartition->numblocks - 1]);
      lgpartition->blocknum[lgpartition->numblocks - 1] = 0xFF;
      lgpartition->numblocks = 0;

      // Copy over ip to 0x06002000
      for (i = 0; i < blocks; i++)
      {
         if ((lgpartition = Cs2ReadUnFilteredSector(150+i)) == NULL)
            return -1;

         buffer = lgpartition->block[lgpartition->numblocks - 1]->data;

         if (size >= 2048)
         {
            for (i2 = 0; i2 < 2048; i2++)
               MappedMemoryWriteByte(0x06002000 + (i * 0x800) + i2, buffer[i2]);
         }
         else
         {
            for (i2 = 0; i2 < size; i2++)
               MappedMemoryWriteByte(0x06002000 + (i * 0x800) + i2, buffer[i2]);
         }

         size -= 2048;

         // Free Block
         lgpartition->size = 0;
         Cs2FreeBlock(lgpartition->block[lgpartition->numblocks - 1]);
         lgpartition->blocknum[lgpartition->numblocks - 1] = 0xFF;
         lgpartition->numblocks = 0;
      }

      // Ok, now that we've loaded the ip, now it's time to load the
      // First Program

      // Figure out where the first program is located
      if ((lgpartition = Cs2ReadUnFilteredSector(166)) == NULL)
         return -1;

      // Figure out root directory's location

      // Retrieve directory record's lba
      Cs2CopyDirRecord(lgpartition->block[lgpartition->numblocks - 1]->data + 0x9C, &dirrec);

      // Free Block
      lgpartition->size = 0;
      Cs2FreeBlock(lgpartition->block[lgpartition->numblocks - 1]);
      lgpartition->blocknum[lgpartition->numblocks - 1] = 0xFF;
      lgpartition->numblocks = 0;

      // Now then, fetch the root directory's records
      if ((lgpartition = Cs2ReadUnFilteredSector(dirrec.lba+150)) == NULL)
         return -1;

      buffer = lgpartition->block[lgpartition->numblocks - 1]->data;

      // Skip the first two records, read in the last one
      for (i = 0; i < 3; i++)
      {
         Cs2CopyDirRecord(buffer, &dirrec);
         buffer += dirrec.recordsize;
      }

      size = dirrec.size;
      blocks = size >> 11;
      if ((dirrec.size % 2048) != 0)
         blocks++;

      // Free Block
      lgpartition->size = 0;
      Cs2FreeBlock(lgpartition->block[lgpartition->numblocks - 1]);
      lgpartition->blocknum[lgpartition->numblocks - 1] = 0xFF;
      lgpartition->numblocks = 0;

      // Copy over First Program to addr
      for (i = 0; i < blocks; i++)
      {
         if ((lgpartition = Cs2ReadUnFilteredSector(150+dirrec.lba+i)) == NULL)
            return -1;

         buffer = lgpartition->block[lgpartition->numblocks - 1]->data;

         if (size >= 2048)
         {
            for (i2 = 0; i2 < 2048; i2++)
               MappedMemoryWriteByte(addr + (i * 0x800) + i2, buffer[i2]);
         }
         else
         {
            for (i2 = 0; i2 < size; i2++)
               MappedMemoryWriteByte(addr + (i * 0x800) + i2, buffer[i2]);
         }

         size -= 2048;

         // Free Block
         lgpartition->size = 0;
         Cs2FreeBlock(lgpartition->block[lgpartition->numblocks - 1]);
         lgpartition->blocknum[lgpartition->numblocks - 1] = 0xFF;
         lgpartition->numblocks = 0;
      }

      // Now setup SH2 registers to start executing at ip code
      MSH2.regs.PC = 0x06002E00;
   }
   else
   {
      // Ok, we're not. Time to bail!

      // Free Block
      lgpartition->size = 0;
      Cs2FreeBlock(lgpartition->block[lgpartition->numblocks - 1]);
      lgpartition->blocknum[lgpartition->numblocks - 1] = 0xFF;
      lgpartition->numblocks = 0;

      return -1;
   }

   return 0;
}

//////////////////////////////////////////////////////////////////////////////