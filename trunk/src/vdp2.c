/*  Copyright 2003-2005 Guillaume Duhamel
    Copyright 2004-2007 Theo Berkau

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

#include <stdlib.h>
#include "vdp2.h"
#include "debug.h"
#include "scu.h"
#include "sh2core.h"
#include "vdp1.h"
#include "yabause.h"
#include "movie.h"

char OSDMessage[64] = "";	//adelikat: For showing on screen messages such as savestate loaded/saved
int OSDMessageTimer = 120;

u8 * Vdp2Ram;
u8 * Vdp2ColorRam;
Vdp2 * Vdp2Regs;
Vdp2Internal_struct Vdp2Internal;
Vdp2External_struct Vdp2External;

static int autoframeskipenab=0;
static int framestoskip=0;
static int framesskipped=0;
static int throttlespeed=0;
static int skipnextframe=0;
u64 lastticks=0;
static u64 curticks=0;
static u64 diffticks=0;
static u32 framecount=0;
static u64 onesecondticks=0;
static int fps;
static int fpsframecount=0;
static u64 fpsticks;
static int fpstoggle=0;

//////////////////////////////////////////////////////////////////////////////
int GetOSDToggle()
{
	return fpstoggle;
}

//////////////////////////////////////////////////////////////////////////////
void SetODSToggle(int toggle)
{
	fpstoggle = toggle;
}

//////////////////////////////////////////////////////////////////////////////
//adelikat: This function will handle the OSDMessage variable properly, it should be used by other functions for displaying information to the user
void DisplayMessage(const char* str, ...)
{
	strcpy(OSDMessage, str);
	OSDMessageTimer = 120;
}

//////////////////////////////////////////////////////////////////////////////

u8 FASTCALL Vdp2RamReadByte(u32 addr) {
   addr &= 0x7FFFF;
   return T1ReadByte(Vdp2Ram, addr);
}

//////////////////////////////////////////////////////////////////////////////

u16 FASTCALL Vdp2RamReadWord(u32 addr) {
   addr &= 0x7FFFF;
   return T1ReadWord(Vdp2Ram, addr);
}

//////////////////////////////////////////////////////////////////////////////

u32 FASTCALL Vdp2RamReadLong(u32 addr) {
   addr &= 0x7FFFF;
   return T1ReadLong(Vdp2Ram, addr);
}

//////////////////////////////////////////////////////////////////////////////

void FASTCALL Vdp2RamWriteByte(u32 addr, u8 val) {
   addr &= 0x7FFFF;
   T1WriteByte(Vdp2Ram, addr, val);
}

//////////////////////////////////////////////////////////////////////////////

void FASTCALL Vdp2RamWriteWord(u32 addr, u16 val) {
   addr &= 0x7FFFF;
   T1WriteWord(Vdp2Ram, addr, val);
}

//////////////////////////////////////////////////////////////////////////////

void FASTCALL Vdp2RamWriteLong(u32 addr, u32 val) {
   addr &= 0x7FFFF;
   T1WriteLong(Vdp2Ram, addr, val);
}

//////////////////////////////////////////////////////////////////////////////

u8 FASTCALL Vdp2ColorRamReadByte(u32 addr) {
   addr &= 0xFFF;
   return T2ReadByte(Vdp2ColorRam, addr);
}

//////////////////////////////////////////////////////////////////////////////

u16 FASTCALL Vdp2ColorRamReadWord(u32 addr) {
   addr &= 0xFFF;
   return T2ReadWord(Vdp2ColorRam, addr);
}

//////////////////////////////////////////////////////////////////////////////

u32 FASTCALL Vdp2ColorRamReadLong(u32 addr) {
   addr &= 0xFFF;
   return T2ReadLong(Vdp2ColorRam, addr);
}

//////////////////////////////////////////////////////////////////////////////

void FASTCALL Vdp2ColorRamWriteByte(u32 addr, u8 val) {
   addr &= 0xFFF;
   T2WriteByte(Vdp2ColorRam, addr, val);
}

//////////////////////////////////////////////////////////////////////////////

void FASTCALL Vdp2ColorRamWriteWord(u32 addr, u16 val) {
   addr &= 0xFFF;
   T2WriteWord(Vdp2ColorRam, addr, val);
//   if (Vdp2Internal.ColorMode == 0)
//      T1WriteWord(Vdp2ColorRam, addr + 0x800, val);
}

//////////////////////////////////////////////////////////////////////////////

void FASTCALL Vdp2ColorRamWriteLong(u32 addr, u32 val) {
   addr &= 0xFFF;
   T2WriteLong(Vdp2ColorRam, addr, val);
}

//////////////////////////////////////////////////////////////////////////////

int Vdp2Init(void) {
   if ((Vdp2Regs = (Vdp2 *) calloc(1, sizeof(Vdp2))) == NULL)
      return -1;

   if ((Vdp2Ram = T1MemoryInit(0x80000)) == NULL)
      return -1;

   if ((Vdp2ColorRam = T2MemoryInit(0x1000)) == NULL)
      return -1;

   Vdp2Reset();
   return 0;
}

//////////////////////////////////////////////////////////////////////////////

void Vdp2DeInit(void) {
   if (Vdp2Regs)
      free(Vdp2Regs);
   Vdp2Regs = NULL;

   if (Vdp2Ram)
      T1MemoryDeInit(Vdp2Ram);
   Vdp2Ram = NULL;

   if (Vdp2ColorRam)
      T2MemoryDeInit(Vdp2ColorRam);
   Vdp2ColorRam = NULL;
}

//////////////////////////////////////////////////////////////////////////////

void Vdp2Reset(void) {
   Vdp2Regs->TVMD = 0x0000;
   Vdp2Regs->EXTEN = 0x0000;
   Vdp2Regs->TVSTAT = Vdp2Regs->TVSTAT & 0x1;
   Vdp2Regs->VRSIZE = 0x0000; // fix me(version should be set)
   Vdp2Regs->RAMCTL = 0x0000;
   Vdp2Regs->BGON = 0x0000;
   Vdp2Regs->CHCTLA = 0x0000;
   Vdp2Regs->CHCTLB = 0x0000;
   Vdp2Regs->BMPNA = 0x0000;
   Vdp2Regs->MPOFN = 0x0000;
   Vdp2Regs->MPABN2 = 0x0000;
   Vdp2Regs->MPCDN2 = 0x0000;
   Vdp2Regs->SCXIN0 = 0x0000;
   Vdp2Regs->SCXDN0 = 0x0000;
   Vdp2Regs->SCYIN0 = 0x0000;
   Vdp2Regs->SCYDN0 = 0x0000;
   Vdp2Regs->ZMXN0.all = 0x00000000;
   Vdp2Regs->ZMYN0.all = 0x00000000;
   Vdp2Regs->SCXIN1 = 0x0000;
   Vdp2Regs->SCXDN1 = 0x0000;
   Vdp2Regs->SCYIN1 = 0x0000;
   Vdp2Regs->SCYDN1 = 0x0000;
   Vdp2Regs->ZMXN1.all = 0x00000000;
   Vdp2Regs->ZMYN1.all = 0x00000000;
   Vdp2Regs->SCXN2 = 0x0000;
   Vdp2Regs->SCYN2 = 0x0000;
   Vdp2Regs->SCXN3 = 0x0000;
   Vdp2Regs->SCYN3 = 0x0000;
   Vdp2Regs->ZMCTL = 0x0000;
   Vdp2Regs->SCRCTL = 0x0000;
   Vdp2Regs->VCSTA.all = 0x00000000;
   Vdp2Regs->BKTAU = 0x0000;
   Vdp2Regs->BKTAL = 0x0000;
   Vdp2Regs->RPMD = 0x0000;
   Vdp2Regs->RPRCTL = 0x0000;
   Vdp2Regs->KTCTL = 0x0000;
   Vdp2Regs->KTAOF = 0x0000;
   Vdp2Regs->OVPNRA = 0x0000;
   Vdp2Regs->OVPNRB = 0x0000;
   Vdp2Regs->WPSX0 = 0x0000;
   Vdp2Regs->WPSY0 = 0x0000;
   Vdp2Regs->WPEX0 = 0x0000;
   Vdp2Regs->WPEY0 = 0x0000;
   Vdp2Regs->WPSX1 = 0x0000;
   Vdp2Regs->WPSY1 = 0x0000;
   Vdp2Regs->WPEX1 = 0x0000;
   Vdp2Regs->WPEY1 = 0x0000;
   Vdp2Regs->WCTLA = 0x0000;
   Vdp2Regs->WCTLB = 0x0000;
   Vdp2Regs->WCTLC = 0x0000;
   Vdp2Regs->WCTLD = 0x0000;
   Vdp2Regs->SPCTL = 0x0000;
   Vdp2Regs->SDCTL = 0x0000;
   Vdp2Regs->CRAOFA = 0x0000;
   Vdp2Regs->CRAOFB = 0x0000;
   Vdp2Regs->LNCLEN = 0x0000;
   Vdp2Regs->SFPRMD = 0x0000;
   Vdp2Regs->CCCTL = 0x0000;
   Vdp2Regs->SFCCMD = 0x0000;
   Vdp2Regs->PRISA = 0x0000;
   Vdp2Regs->PRISB = 0x0000;
   Vdp2Regs->PRISC = 0x0000;
   Vdp2Regs->PRISD = 0x0000;
   Vdp2Regs->PRINA = 0x0000;
   Vdp2Regs->PRINB = 0x0000;
   Vdp2Regs->PRIR = 0x0000;
   Vdp2Regs->CCRNA = 0x0000;
   Vdp2Regs->CCRNB = 0x0000;
   Vdp2Regs->CLOFEN = 0x0000;
   Vdp2Regs->CLOFSL = 0x0000;
   Vdp2Regs->COAR = 0x0000;
   Vdp2Regs->COAG = 0x0000;
   Vdp2Regs->COAB = 0x0000;
   Vdp2Regs->COBR = 0x0000;
   Vdp2Regs->COBG = 0x0000;
   Vdp2Regs->COBB = 0x0000;

   yabsys.VBlankLineCount = 224;
   Vdp2Internal.ColorMode = 0;

   Vdp2External.disptoggle = 0xFF;
}

//////////////////////////////////////////////////////////////////////////////

void Vdp2VBlankIN(void) {
   Vdp2Regs->TVSTAT |= 0x0008;
   ScuSendVBlankIN();

   if (yabsys.IsSSH2Running)
      SH2SendInterrupt(SSH2, 0x43, 0x6);
}

//////////////////////////////////////////////////////////////////////////////

void Vdp2HBlankIN(void) {
   Vdp2Regs->TVSTAT |= 0x0004;
   ScuSendHBlankIN();

   if (yabsys.IsSSH2Running)
      SH2SendInterrupt(SSH2, 0x41, 0x2);
}

//////////////////////////////////////////////////////////////////////////////

void Vdp2HBlankOUT(void) {
   Vdp2Regs->TVSTAT &= ~0x0004;
}

//////////////////////////////////////////////////////////////////////////////

void FPSDisplay(void)
{
   if (fpstoggle)
   {
      if (!OSDMessageTimer)
		OSDMessage[0] = 0;
	  VIDCore->OnScreenDebugMessage("%02d/%02d FPS %d %d %s %s %s", fps, yabsys.IsPal ? 50 : 60, framecounter, lagframecounter, MovieStatus, InputDisplayString, OSDMessage);
	  if (OSDMessageTimer > 0)
		  OSDMessageTimer--;
      fpsframecount++;
      if(YabauseGetTicks() >= fpsticks + yabsys.tickfreq)
      {
         fps = fpsframecount;
         fpsframecount = 0;
         fpsticks = YabauseGetTicks();
      }
   }
}

//////////////////////////////////////////////////////////////////////////////

void ToggleFPS(void)
{
   fpstoggle ^= 1;
}

//////////////////////////////////////////////////////////////////////////////

void SpeedThrottleEnable(void) {
   throttlespeed = 1;
}

//////////////////////////////////////////////////////////////////////////////

void SpeedThrottleDisable(void) {
   throttlespeed = 0;
}

//////////////////////////////////////////////////////////////////////////////

void Vdp2VBlankOUT(void) {
   Vdp2Regs->TVSTAT = (Vdp2Regs->TVSTAT & ~0x0008) | 0x0002;

   if (!skipnextframe)
   {
      VIDCore->Vdp2DrawStart();

      if (Vdp2Regs->TVMD & 0x8000) {
         VIDCore->Vdp2DrawScreens();
         Vdp1Draw();
      }
      else
         Vdp1NoDraw();

      FPSDisplay();
      VIDCore->Vdp2DrawEnd();

      framesskipped = 0;

      if (framestoskip > 0)
         skipnextframe = 1;
   }
   else
   {
      framestoskip--;

      if (framestoskip < 1)
         skipnextframe = 0;
      else
         skipnextframe = 1;
      Vdp1NoDraw();

      framesskipped++;
   }

   // Do Frame Skip/Frame Limiting/Speed Throttling here
   if (throttlespeed)
   {
      // Should really depend on how fast we're rendering the frames
      if (framestoskip < 1)
         framestoskip = 6;
   }
   //when in frame advance, disable frame skipping
   else if (autoframeskipenab && FrameAdvanceVariable == 0)
   {
      framecount++;

      if (framecount > (yabsys.IsPal ? 50 : 60))
      {
         framecount = 1;
         onesecondticks = 0;
      }

      curticks = YabauseGetTicks();
      diffticks = curticks-lastticks;

      if ((onesecondticks+diffticks) > ((yabsys.OneFrameTime * (u64)framecount) + (yabsys.OneFrameTime / 2)) &&
          framesskipped < 9)
      {
         // Skip the next frame
         skipnextframe = 1;

         // How many frames should we skip?
         framestoskip = 1;
      }
      else if ((onesecondticks+diffticks) < ((yabsys.OneFrameTime * (u64)framecount) - (yabsys.OneFrameTime / 2)))
      {
         // Check to see if we need to limit speed at all
         for (;;)
         {
            curticks = YabauseGetTicks();
            diffticks = curticks-lastticks;
            if ((onesecondticks+diffticks) >= (yabsys.OneFrameTime * (u64)framecount))
               break;
         }
      }

      onesecondticks += diffticks;
      lastticks = curticks;
   }

   ScuSendVBlankOUT();
}

//////////////////////////////////////////////////////////////////////////////

u8 FASTCALL Vdp2ReadByte(u32 addr) {
   LOG("VDP2 register byte read = %08X\n", addr);
   addr &= 0x1FF;
   return 0;
}

//////////////////////////////////////////////////////////////////////////////

u16 FASTCALL Vdp2ReadWord(u32 addr) {
   addr &= 0x1FF;

   switch (addr)
   {
      case 0x000:
         return Vdp2Regs->TVMD;
      case 0x002:
         if (!(Vdp2Regs->EXTEN & 0x200))
         {
            // Latch HV counter on read
            // Vdp2Regs->HCNT = ?;
            Vdp2Regs->VCNT = yabsys.LineCount;
            Vdp2Regs->TVSTAT |= 0x200;
         }

         return Vdp2Regs->EXTEN;
      case 0x004:
         // Clear External latch and sync flags
         Vdp2Regs->TVSTAT &= 0xFCFF;

         // if TVMD's DISP bit is cleared, TVSTAT's VBLANK bit is always set
         if (Vdp2Regs->TVMD & 0x8000)
            return Vdp2Regs->TVSTAT;
         else
            return (Vdp2Regs->TVSTAT | 0x8);
      case 0x006:         
         return Vdp2Regs->VRSIZE;
      case 0x008:
         return Vdp2Regs->HCNT;
      case 0x00A:
         return Vdp2Regs->VCNT;
      default:
      {
         LOG("Unhandled VDP2 word read: %08X\n", addr);
         break;
      }
   }

   return 0;
}

//////////////////////////////////////////////////////////////////////////////

u32 FASTCALL Vdp2ReadLong(u32 addr) {
   LOG("VDP2 register long read = %08X\n", addr);
   addr &= 0x1FF;
   return 0;
}

//////////////////////////////////////////////////////////////////////////////

void FASTCALL Vdp2WriteByte(u32 addr, UNUSED u8 val) {
   LOG("VDP2 register byte write = %08X\n", addr);
   addr &= 0x1FF;
}

//////////////////////////////////////////////////////////////////////////////

void FASTCALL Vdp2WriteWord(u32 addr, u16 val) {
   addr &= 0x1FF;

   switch (addr)
   {
      case 0x000:
         VIDCore->Vdp2SetResolution(val);
         Vdp2Regs->TVMD = val;
         yabsys.VBlankLineCount = 224+(val & 0x30);
         return;
      case 0x002:
         Vdp2Regs->EXTEN = val;
         return;
      case 0x004:
         // TVSTAT is read-only
         return;
      case 0x006:
         Vdp2Regs->VRSIZE = val;
         return;
      case 0x008:
         // HCNT is read-only
         return;
      case 0x00A:
         // VCNT is read-only
         return;
      case 0x00C:
         // Reserved
         return;
      case 0x00E:
         Vdp2Regs->RAMCTL = val;
         Vdp2Internal.ColorMode = (val >> 12) & 0x3;
         return;
      case 0x010:
         Vdp2Regs->CYCA0L = val;
         return;
      case 0x012:
         Vdp2Regs->CYCA0U = val;
         return;
      case 0x014:
         Vdp2Regs->CYCA1L = val;
         return;
      case 0x016:
         Vdp2Regs->CYCA1U = val;
         return;
      case 0x018:
         Vdp2Regs->CYCB0L = val;
         return;
      case 0x01A:
         Vdp2Regs->CYCB0U = val;
         return;
      case 0x01C:
         Vdp2Regs->CYCB1L = val;
         return;
      case 0x01E:
         Vdp2Regs->CYCB1U = val;
         return;
      case 0x020:
         Vdp2Regs->BGON = val;
         return;
      case 0x022:
         Vdp2Regs->MZCTL = val;
         return;
      case 0x024:
         Vdp2Regs->SFSEL = val;
         return;
      case 0x026:
         Vdp2Regs->SFCODE = val;
         return;
      case 0x028:
         Vdp2Regs->CHCTLA = val;
         return;
      case 0x02A:
         Vdp2Regs->CHCTLB = val;
         return;
      case 0x02C:
         Vdp2Regs->BMPNA = val;
         return;
      case 0x02E:
         Vdp2Regs->BMPNB = val;
         return;
      case 0x030:
         Vdp2Regs->PNCN0 = val;
         return;
      case 0x032:
         Vdp2Regs->PNCN1 = val;
         return;
      case 0x034:
         Vdp2Regs->PNCN2 = val;
         return;
      case 0x036:
         Vdp2Regs->PNCN3 = val;
         return;
      case 0x038:
         Vdp2Regs->PNCR = val;
         return;
      case 0x03A:
         Vdp2Regs->PLSZ = val;
         return;
      case 0x03C:
         Vdp2Regs->MPOFN = val;
         return;
      case 0x03E:
         Vdp2Regs->MPOFR = val;
         return;
      case 0x040:
         Vdp2Regs->MPABN0 = val;
         return;
      case 0x042:
         Vdp2Regs->MPCDN0 = val;
         return;
      case 0x044:
         Vdp2Regs->MPABN1 = val;
         return;
      case 0x046:
         Vdp2Regs->MPCDN1 = val;
         return;
      case 0x048:
         Vdp2Regs->MPABN2 = val;
         return;
      case 0x04A:
         Vdp2Regs->MPCDN2 = val;
         return;
      case 0x04C:
         Vdp2Regs->MPABN3 = val;
         return;
      case 0x04E:
         Vdp2Regs->MPCDN3 = val;
         return;
      case 0x050:
         Vdp2Regs->MPABRA = val;
         return;
      case 0x052:
         Vdp2Regs->MPCDRA = val;
         return;
      case 0x054:
         Vdp2Regs->MPEFRA = val;
         return;
      case 0x056:
         Vdp2Regs->MPGHRA = val;
         return;
      case 0x058:
         Vdp2Regs->MPIJRA = val;
         return;
      case 0x05A:
         Vdp2Regs->MPKLRA = val;
         return;
      case 0x05C:
         Vdp2Regs->MPMNRA = val;
         return;
      case 0x05E:
         Vdp2Regs->MPOPRA = val;
         return;
      case 0x060:
         Vdp2Regs->MPABRB = val;
         return;
      case 0x062:
         Vdp2Regs->MPCDRB = val;
         return;
      case 0x064:
         Vdp2Regs->MPEFRB = val;
         return;
      case 0x066:
         Vdp2Regs->MPGHRB = val;
         return;
      case 0x068:
         Vdp2Regs->MPIJRB = val;
         return;
      case 0x06A:
         Vdp2Regs->MPKLRB = val;
         return;
      case 0x06C:
         Vdp2Regs->MPMNRB = val;
         return;
      case 0x06E:
         Vdp2Regs->MPOPRB = val;
         return;
      case 0x070:
         Vdp2Regs->SCXIN0 = val;
         return;
      case 0x072:
         Vdp2Regs->SCXDN0 = val;
         return;
      case 0x074:
         Vdp2Regs->SCYIN0 = val;
         return;
      case 0x076:
         Vdp2Regs->SCYDN0 = val;
         return;
      case 0x078:
         Vdp2Regs->ZMXN0.part.I = val;
         return;
      case 0x07A:
         Vdp2Regs->ZMXN0.part.D = val;
         return;
      case 0x07C:
         Vdp2Regs->ZMYN0.part.I = val;
         return;
      case 0x07E:
         Vdp2Regs->ZMYN0.part.D = val;
         return;
      case 0x080:
         Vdp2Regs->SCXIN1 = val;
         return;
      case 0x082:
         Vdp2Regs->SCXDN1 = val;
         return;
      case 0x084:
         Vdp2Regs->SCYIN1 = val;
         return;
      case 0x086:
         Vdp2Regs->SCYDN1 = val;
         return;
      case 0x088:
         Vdp2Regs->ZMXN1.part.I = val;
         return;
      case 0x08A:
         Vdp2Regs->ZMXN1.part.D = val;
         return;
      case 0x08C:
         Vdp2Regs->ZMYN1.part.I = val;
         return;
      case 0x08E:
         Vdp2Regs->ZMYN1.part.D = val;
         return;
      case 0x090:
         Vdp2Regs->SCXN2 = val;
         return;
      case 0x092:
         Vdp2Regs->SCYN2 = val;
         return;
      case 0x094:
         Vdp2Regs->SCXN3 = val;
         return;
      case 0x096:
         Vdp2Regs->SCYN3 = val;
         return;
      case 0x098:
         Vdp2Regs->ZMCTL = val;
         return;
      case 0x09A:
         Vdp2Regs->SCRCTL = val;
         return;
      case 0x09C:
         Vdp2Regs->VCSTA.part.U = val;
         return;
      case 0x09E:
         Vdp2Regs->VCSTA.part.L = val;
         return;
      case 0x0A0:
         Vdp2Regs->LSTA0.part.U = val;
         return;
      case 0x0A2:
         Vdp2Regs->LSTA0.part.L = val;
         return;
      case 0x0A4:
         Vdp2Regs->LSTA1.part.U = val;
         return;
      case 0x0A6:
         Vdp2Regs->LSTA1.part.L = val;
         return;
      case 0x0A8:
         Vdp2Regs->LCTA.part.U = val;
         return;
      case 0x0AA:
         Vdp2Regs->LCTA.part.L = val;
         return;
      case 0x0AC:
         Vdp2Regs->BKTAU = val;
         return;
      case 0x0AE:
         Vdp2Regs->BKTAL = val;
         return;
      case 0x0B0:
         Vdp2Regs->RPMD = val;
         return;
      case 0x0B2:
         Vdp2Regs->RPRCTL = val;
         return;
      case 0x0B4:
         Vdp2Regs->KTCTL = val;
         return;
      case 0x0B6:
         Vdp2Regs->KTAOF = val;
         return;
      case 0x0B8:
         Vdp2Regs->OVPNRA = val;
         return;
      case 0x0BA:
         Vdp2Regs->OVPNRB = val;
         return;
      case 0x0BC:
         Vdp2Regs->RPTA.part.U = val;
         return;
      case 0x0BE:
         Vdp2Regs->RPTA.part.L = val;
         return;
      case 0x0C0:
         Vdp2Regs->WPSX0 = val;
         return;
      case 0x0C2:
         Vdp2Regs->WPSY0 = val;
         return;
      case 0x0C4:
         Vdp2Regs->WPEX0 = val;
         return;
      case 0x0C6:
         Vdp2Regs->WPEY0 = val;
         return;
      case 0x0C8:
         Vdp2Regs->WPSX1 = val;
         return;
      case 0x0CA:
         Vdp2Regs->WPSY1 = val;
         return;
      case 0x0CC:
         Vdp2Regs->WPEX1 = val;
         return;
      case 0x0CE:
         Vdp2Regs->WPEY1 = val;
         return;
      case 0x0D0:
         Vdp2Regs->WCTLA = val;
         return;
      case 0x0D2:
         Vdp2Regs->WCTLB = val;
         return;
      case 0x0D4:
         Vdp2Regs->WCTLC = val;
         return;
      case 0x0D6:
         Vdp2Regs->WCTLD = val;
         return;
      case 0x0D8:
         Vdp2Regs->LWTA0.part.U = val;
         return;
      case 0x0DA:
         Vdp2Regs->LWTA0.part.L = val;
         return;
      case 0x0DC:
         Vdp2Regs->LWTA1.part.U = val;
         return;
      case 0x0DE:
         Vdp2Regs->LWTA1.part.L = val;
         return;
      case 0x0E0:
         Vdp2Regs->SPCTL = val;
         return;
      case 0x0E2:
         Vdp2Regs->SDCTL = val;
         return;
      case 0x0E4:
         Vdp2Regs->CRAOFA = val;
         return;
      case 0x0E6:
         Vdp2Regs->CRAOFB = val;
         return;     
      case 0x0E8:
         Vdp2Regs->LNCLEN = val;
         return;
      case 0x0EA:
         Vdp2Regs->SFPRMD = val;
         return;
      case 0x0EC:
         Vdp2Regs->CCCTL = val;
         return;     
      case 0x0EE:
         Vdp2Regs->SFCCMD = val;
         return;
      case 0x0F0:
         Vdp2Regs->PRISA = val;
         return;
      case 0x0F2:
         Vdp2Regs->PRISB = val;
         return;
      case 0x0F4:
         Vdp2Regs->PRISC = val;
         return;
      case 0x0F6:
         Vdp2Regs->PRISD = val;
         return;
      case 0x0F8:
         VIDCore->Vdp2SetPriorityNBG0(val & 0x7);
         VIDCore->Vdp2SetPriorityNBG1((val >> 8) & 0x7);
         Vdp2Regs->PRINA = val;
         return;
      case 0x0FA:
         VIDCore->Vdp2SetPriorityNBG2(val & 0x7);
         VIDCore->Vdp2SetPriorityNBG3((val >> 8) & 0x7);
         Vdp2Regs->PRINB = val;
         return;
      case 0x0FC:
         VIDCore->Vdp2SetPriorityRBG0(val & 0x7);
         Vdp2Regs->PRIR = val;
         return;
      case 0x0FE:
         // Reserved
         return;
      case 0x100:
         Vdp2Regs->CCRSA = val;
         return;
      case 0x102:
         Vdp2Regs->CCRSB = val;
         return;
      case 0x104:
         Vdp2Regs->CCRSC = val;
         return;
      case 0x106:
         Vdp2Regs->CCRSD = val;
         return;
      case 0x108:
         Vdp2Regs->CCRNA = val;
         return;
      case 0x10A:
         Vdp2Regs->CCRNB = val;
         return;
      case 0x10C:
         Vdp2Regs->CCRR = val;
         return;
      case 0x10E:
         Vdp2Regs->CCRLB = val;
         return;
      case 0x110:
         Vdp2Regs->CLOFEN = val;
         return;
      case 0x112:
         Vdp2Regs->CLOFSL = val;
         return;
      case 0x114:
         Vdp2Regs->COAR = val;
         return;
      case 0x116:
         Vdp2Regs->COAG = val;
         return;
      case 0x118:
         Vdp2Regs->COAB = val;
         return;
      case 0x11A:
         Vdp2Regs->COBR = val;
         return;
      case 0x11C:
         Vdp2Regs->COBG = val;
         return;
      case 0x11E:
         Vdp2Regs->COBB = val;
         return;
      default:
      {
         LOG("Unhandled VDP2 word write: %08X\n", addr);
         break;
      }
   }
}

//////////////////////////////////////////////////////////////////////////////

void FASTCALL Vdp2WriteLong(u32 addr, u32 val) {
   addr &= 0x1FF;

   switch (addr)
   {
      case 0x000:
         Vdp2Regs->TVMD = val >> 16;
         Vdp2Regs->EXTEN = val & 0xFFFF;
         VIDCore->Vdp2SetResolution(Vdp2Regs->TVMD);
         yabsys.VBlankLineCount = 224+(Vdp2Regs->TVMD & 0x30);
         return;
      case 0x004:
         Vdp2Regs->VRSIZE = val & 0xFFFF;
         return;
      case 0x008:
         // read-only
         return;
      case 0x00C:
         Vdp2Regs->RAMCTL = val & 0xFFFF;
         return;
      case 0x010:
         Vdp2Regs->CYCA0L = val >> 16;
         Vdp2Regs->CYCA0U = val & 0xFFFF;
         return;
      case 0x014:
         Vdp2Regs->CYCA1L = val >> 16;
         Vdp2Regs->CYCA1U = val & 0xFFFF;
         return;
      case 0x018:
         Vdp2Regs->CYCB0L = val >> 16;
         Vdp2Regs->CYCB0U = val & 0xFFFF;
         return;
      case 0x01C:
         Vdp2Regs->CYCB1L = val >> 16;
         Vdp2Regs->CYCB1U = val & 0xFFFF;
         return;
      case 0x020:
         Vdp2Regs->BGON = val >> 16;
         Vdp2Regs->MZCTL = val & 0xFFFF;
         return;
      case 0x024:
         Vdp2Regs->SFSEL = val >> 16;
         Vdp2Regs->SFCODE = val & 0xFFFF;
         return;
      case 0x028:
         Vdp2Regs->CHCTLA = val >> 16;
         Vdp2Regs->CHCTLB = val & 0xFFFF;
         return;
      case 0x02C:
         Vdp2Regs->BMPNA = val >> 16;
         Vdp2Regs->BMPNB = val & 0xFFFF;
         return;
      case 0x030:
         Vdp2Regs->PNCN0 = val >> 16;
         Vdp2Regs->PNCN1 = val & 0xFFFF;
         return;
      case 0x034:
         Vdp2Regs->PNCN2 = val >> 16;
         Vdp2Regs->PNCN3 = val & 0xFFFF;
         return;
      case 0x038:
         Vdp2Regs->PNCR = val >> 16;
         Vdp2Regs->PLSZ = val & 0xFFFF;
         return;
      case 0x03C:
         Vdp2Regs->MPOFN = val >> 16;
         Vdp2Regs->MPOFR = val & 0xFFFF;
         return;
      case 0x040:
         Vdp2Regs->MPABN0 = val >> 16;
         Vdp2Regs->MPCDN0 = val & 0xFFFF;
         return;
      case 0x044:
         Vdp2Regs->MPABN1 = val >> 16;
         Vdp2Regs->MPCDN1 = val & 0xFFFF;
         return;
      case 0x048:
         Vdp2Regs->MPABN2 = val >> 16;
         Vdp2Regs->MPCDN2 = val & 0xFFFF;
         return;
      case 0x04C:
         Vdp2Regs->MPABN3 = val >> 16;
         Vdp2Regs->MPCDN3 = val & 0xFFFF;
         return;
      case 0x050:
         Vdp2Regs->MPABRA = val >> 16;
         Vdp2Regs->MPCDRA = val & 0xFFFF;
         return;
      case 0x054:
         Vdp2Regs->MPEFRA = val >> 16;
         Vdp2Regs->MPGHRA = val & 0xFFFF;
         return;
      case 0x058:
         Vdp2Regs->MPIJRA = val >> 16;
         Vdp2Regs->MPKLRA = val & 0xFFFF;
         return;
      case 0x05C:
         Vdp2Regs->MPMNRA = val >> 16;
         Vdp2Regs->MPOPRA = val & 0xFFFF;
         return;
      case 0x060:
         Vdp2Regs->MPABRB = val >> 16;
         Vdp2Regs->MPCDRB = val & 0xFFFF;
         return;
      case 0x064:
         Vdp2Regs->MPEFRB = val >> 16;
         Vdp2Regs->MPGHRB = val & 0xFFFF;
         return;
      case 0x068:
         Vdp2Regs->MPIJRB = val >> 16;
         Vdp2Regs->MPKLRB = val & 0xFFFF;
         return;
      case 0x06C:
         Vdp2Regs->MPMNRB = val >> 16;
         Vdp2Regs->MPOPRB = val & 0xFFFF;
         return;
      case 0x070:
         Vdp2Regs->SCXIN0 = val >> 16;
         // SCXDN0 here
         return;
      case 0x074:
         Vdp2Regs->SCYIN0 = val >> 16;
         // SCYDN0 here
         return;
      case 0x078:
         Vdp2Regs->ZMXN0.all = val;
         return;
      case 0x07C:
         Vdp2Regs->ZMYN0.all = val;
         return;
      case 0x080:
         Vdp2Regs->SCXIN1 = val >> 16;
         // SCXDN1 here
         return;
      case 0x084:
         Vdp2Regs->SCYIN1 = val >> 16;
         // SCYDN1 here
         return;
      case 0x088:
         Vdp2Regs->ZMXN1.all = val;
         return;
      case 0x08C:
         Vdp2Regs->ZMYN1.all = val;
         return;
      case 0x090:
         Vdp2Regs->SCXN2 = val >> 16;
         Vdp2Regs->SCYN2 = val & 0xFFFF;
         return;
      case 0x094:
         Vdp2Regs->SCXN3 = val >> 16;
         Vdp2Regs->SCYN3 = val & 0xFFFF;
         return;
      case 0x09C:
         Vdp2Regs->VCSTA.all = val;
         return;
      case 0x0A0:
         Vdp2Regs->LSTA0.all = val;
         return;
      case 0x0A4:
         Vdp2Regs->LSTA1.all = val;
         return;
      case 0x0AC:
         Vdp2Regs->BKTAU = val >> 16;
         Vdp2Regs->BKTAL = val & 0xFFFF;
         return;
      case 0x0D0:
         Vdp2Regs->WCTLA = val >> 16;
         Vdp2Regs->WCTLB = val & 0xFFFF;
         return;
      case 0x0D8:
         Vdp2Regs->LWTA0.all = val;
         return;
      case 0x0DC:
         Vdp2Regs->LWTA1.all = val;
         return;
      case 0x0E0:
         Vdp2Regs->SPCTL = val >> 16;
         Vdp2Regs->SDCTL = val & 0xFFFF;
         return;
      case 0x0E4:
         Vdp2Regs->CRAOFA = val >> 16;
         Vdp2Regs->CRAOFB = val & 0xFFFF;
         return;
      case 0x0E8:
         Vdp2Regs->LNCLEN = val >> 16;
         Vdp2Regs->SFPRMD = val & 0xFFFF;
         return;
      case 0x0EC:
         Vdp2Regs->CCCTL = val >> 16;
         Vdp2Regs->SFCCMD = val & 0xFFFF;
         return;
      case 0x0F0:
         Vdp2Regs->PRISA = val >> 16;
         Vdp2Regs->PRISB = val & 0xFFFF;
         return;
      case 0x0F4:
         Vdp2Regs->PRISC = val >> 16;
         Vdp2Regs->PRISD = val & 0xFFFF;
         return;
      case 0x0F8:
         Vdp2Regs->PRINA = val >> 16;
         Vdp2Regs->PRINB = val & 0xFFFF;
         VIDCore->Vdp2SetPriorityNBG0(Vdp2Regs->PRINA & 0x7);
         VIDCore->Vdp2SetPriorityNBG1((Vdp2Regs->PRINA >> 8) & 0x7);
         VIDCore->Vdp2SetPriorityNBG2(Vdp2Regs->PRINB & 0x7);
         VIDCore->Vdp2SetPriorityNBG3((Vdp2Regs->PRINB >> 8) & 0x7);
         return;
      case 0x100:
         Vdp2Regs->CCRSA = val >> 16;
         Vdp2Regs->CCRSB = val & 0xFFFF;
         return;
      case 0x104:
         Vdp2Regs->CCRSC = val >> 16;
         Vdp2Regs->CCRSD = val & 0xFFFF;
         return;
      case 0x108:
         Vdp2Regs->CCRNA = val >> 16;
         Vdp2Regs->CCRNB = val & 0xFFFF;
         return;
      case 0x10C:
         Vdp2Regs->CCRR = val >> 16;
         Vdp2Regs->CCRLB = val & 0xFFFF;
         return;
      case 0x110:
         Vdp2Regs->CLOFEN = val >> 16;
         Vdp2Regs->CLOFSL = val & 0xFFFF;
         return;
      case 0x114:
         Vdp2Regs->COAR = val >> 16;
         Vdp2Regs->COAG = val & 0xFFFF;
         return;
      case 0x118:
         Vdp2Regs->COAB = val >> 16;
         Vdp2Regs->COBR = val & 0xFFFF;
         return;
      case 0x11C:
         Vdp2Regs->COBG = val >> 16;
         Vdp2Regs->COBB = val & 0xFFFF;
         return;
      case 0x120:
      case 0x124:
      case 0x128:
      case 0x12C:
      case 0x130:
      case 0x134:
      case 0x138:
      case 0x13C:
      case 0x140:
      case 0x144:
      case 0x148:
      case 0x14C:
      case 0x150:
      case 0x154:
      case 0x158:
      case 0x15C:
      case 0x160:
      case 0x164:
      case 0x168:
      case 0x16C:
      case 0x170:
      case 0x174:
      case 0x178:
      case 0x17C:
      case 0x180:
      case 0x184:
      case 0x188:
      case 0x18C:
      case 0x190:
      case 0x194:
      case 0x198:
      case 0x19C:
      case 0x1A0:
      case 0x1A4:
      case 0x1A8:
      case 0x1AC:
      case 0x1B0:
      case 0x1B4:
      case 0x1B8:
      case 0x1BC:
      case 0x1C0:
      case 0x1C4:
      case 0x1C8:
      case 0x1CC:
      case 0x1D0:
      case 0x1D4:
      case 0x1D8:
      case 0x1DC:
      case 0x1E0:
      case 0x1E4:
      case 0x1E8:
      case 0x1EC:
      case 0x1F0:
      case 0x1F4:
      case 0x1F8:
      case 0x1FC:
         // Reserved
         return;
      default:
      {
         LOG("Unhandled VDP2 long write: %08X\n", addr);
         break;
      }
   }
}

//////////////////////////////////////////////////////////////////////////////

int Vdp2SaveState(FILE *fp)
{
   int offset;
   IOCheck_struct check;

   offset = StateWriteHeader(fp, "VDP2", 1);

   // Write registers
   ywrite(&check, (void *)Vdp2Regs, sizeof(Vdp2), 1, fp);

   // Write VDP2 ram
   ywrite(&check, (void *)Vdp2Ram, 0x80000, 1, fp);

   // Write CRAM
   ywrite(&check, (void *)Vdp2ColorRam, 0x1000, 1, fp);

   // Write internal variables
   ywrite(&check, (void *)&Vdp2Internal, sizeof(Vdp2Internal_struct), 1, fp);

   return StateFinishHeader(fp, offset);
}

//////////////////////////////////////////////////////////////////////////////

int Vdp2LoadState(FILE *fp, UNUSED int version, int size)
{
   IOCheck_struct check;

   // Read registers
   yread(&check, (void *)Vdp2Regs, sizeof(Vdp2), 1, fp);

   // Read VDP2 ram
   yread(&check, (void *)Vdp2Ram, 0x80000, 1, fp);

   // Read CRAM
   yread(&check, (void *)Vdp2ColorRam, 0x1000, 1, fp);

   // Read internal variables
   yread(&check, (void *)&Vdp2Internal, sizeof(Vdp2Internal_struct), 1, fp);

   VIDCore->Vdp2SetResolution(Vdp2Regs->TVMD);
   VIDCore->Vdp2SetPriorityNBG0(Vdp2Regs->PRINA & 0x7);
   VIDCore->Vdp2SetPriorityNBG1((Vdp2Regs->PRINA >> 8) & 0x7);
   VIDCore->Vdp2SetPriorityNBG2(Vdp2Regs->PRINB & 0x7);
   VIDCore->Vdp2SetPriorityNBG3((Vdp2Regs->PRINB >> 8) & 0x7);
   VIDCore->Vdp2SetPriorityRBG0(Vdp2Regs->PRIR & 0x7);

   return size;
}

//////////////////////////////////////////////////////////////////////////////

void ToggleNBG0(void)
{
   Vdp2External.disptoggle ^= 0x1;
}

//////////////////////////////////////////////////////////////////////////////

void ToggleNBG1(void)
{
   Vdp2External.disptoggle ^= 0x2;
}

//////////////////////////////////////////////////////////////////////////////

void ToggleNBG2(void)
{
   Vdp2External.disptoggle ^= 0x4;
}

//////////////////////////////////////////////////////////////////////////////

void ToggleNBG3(void)
{
   Vdp2External.disptoggle ^= 0x8;
}

//////////////////////////////////////////////////////////////////////////////

void ToggleRBG0(void)
{
   Vdp2External.disptoggle ^= 0x10;
}

//////////////////////////////////////////////////////////////////////////////

void ToggleFullScreen(void)
{
   if (VIDCore->IsFullscreen())
   {
      VIDCore->Resize(320, 224, 0);
   }
   else
   {
      VIDCore->Resize(640, 480, 1);
   }
}

//////////////////////////////////////////////////////////////////////////////

void EnableAutoFrameSkip(void)
{
   autoframeskipenab = 1;
   lastticks = YabauseGetTicks();
}

//////////////////////////////////////////////////////////////////////////////

void DisableAutoFrameSkip(void)
{
   autoframeskipenab = 0;
}

//////////////////////////////////////////////////////////////////////////////