/*  Copyright 2003-2005 Guillaume Duhamel
    Copyright 2004-2007 Theo Berkau
    Copyright 2005 Fabien Coulon

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

// SH2 Interpreter Core

extern "C" {

#include "sh2core.h"
#include "sh2int.h"
#include "sh2idle.h"
#include "cs0.h"
#include "debug.h"
#include "error.h"
#include "memory.h"
#include "bios.h"
#include "yabause.h"

} //extern "C"


#define _SSH2 (WHICH_SH2?SSH2:MSH2)
#define context (&_SSH2)
#define sh (&_SSH2)

#define TEMPLATE template<int WHICH_SH2> 
TEMPLATE FASTCALL INLINE void _SH2InterpreterExec(u32 cycles)
{
   if (context->isIdle)
      SH2idleParse(context, cycles);
   else
      SH2idleCheck(context, cycles);

   while(context->cycles < cycles)
   {
      // Fetch Instruction
      context->instruction = fetchlist[(context->regs.PC >> 20) & 0x0FF](context->regs.PC);

      // Execute it
      opcodes[WHICH_SH2][context->instruction]();
   }
}
#undef context



// #define SH2_TRACE  // Uncomment to enable tracing for debug interpreter

#ifdef SH2_TRACE
#include "sh2trace.h"
#endif


opcodefunc opcodes[2][0x10000];

SH2Interface_struct SH2Interpreter = {
   SH2CORE_INTERPRETER,
   "SH2 Interpreter",
   SH2InterpreterInit,
   SH2InterpreterDeInit,
   SH2InterpreterReset,
   SH2InterpreterExec,
   NULL  // SH2WriteNotify not used
};

SH2Interface_struct SH2DebugInterpreter = {
   SH2CORE_DEBUGINTERPRETER,
   "SH2 Debugger Interpreter",
   SH2DebugInterpreterInit,
   SH2InterpreterDeInit,
   SH2InterpreterReset,
   SH2DebugInterpreterExec,
   NULL  // SH2WriteNotify not used
};

fetchfunc fetchlist[0x100];

//////////////////////////////////////////////////////////////////////////////

u32 FASTCALL FetchBios(u32 addr)
{
   return T2ReadWord(BiosRom, addr & 0x7FFFF);
}

//////////////////////////////////////////////////////////////////////////////

u32 FASTCALL FetchCs0(u32 addr)
{
   return CartridgeArea->Cs0ReadWord(addr);
}

//////////////////////////////////////////////////////////////////////////////

u32 FASTCALL FetchLWram(u32 addr)
{
   return T2ReadWord(LowWram, addr & 0xFFFFF);
}

//////////////////////////////////////////////////////////////////////////////

u32 FASTCALL FetchHWram(u32 addr)
{
   return T2ReadWord(HighWram, addr & 0xFFFFF);
}

//////////////////////////////////////////////////////////////////////////////

u32 FASTCALL FetchInvalid(UNUSED u32 addr)
{
   return 0xFFFF;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2delay(u32 addr)
{
#ifdef SH2_TRACE
   sh2_trace(sh, addr);
#endif

   // Fetch Instruction
   sh->instruction = fetchlist[(addr >> 20) & 0x0FF](addr);

   // Execute it
   opcodes[WHICH_SH2][sh->instruction]();
   sh->regs.PC -= 2;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2undecoded()
{
   int vectnum;

   if (yabsys.emulatebios)
   {
      if (BiosHandleFunc(sh))
         return;
   }

   YabSetError(YAB_ERR_SH2INVALIDOPCODE, sh);      

   // Save regs.SR on stack
   sh->regs.R[15]-=4;
   MappedMemoryWriteLong(sh->regs.R[15],sh->regs.SR.all);

   // Save regs.PC on stack
   sh->regs.R[15]-=4;
   MappedMemoryWriteLong(sh->regs.R[15],sh->regs.PC + 2);

   // What caused the exception? The delay slot or a general instruction?
   // 4 for General Instructions, 6 for delay slot
   vectnum = 4; //  Fix me

   // Jump to Exception service routine
   sh->regs.PC = MappedMemoryReadLong(sh->regs.VBR+(vectnum<<2));
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2add()
{
   sh->regs.R[INSTRUCTION_B(sh->instruction)] += sh->regs.R[INSTRUCTION_C(sh->instruction)];
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2addi()
{
   s32 cd = (s32)(s8)INSTRUCTION_CD(sh->instruction);
   s32 b = INSTRUCTION_B(sh->instruction);

   sh->regs.R[b] += cd;
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2addc()
{
   u32 tmp0, tmp1;
   s32 source = INSTRUCTION_C(sh->instruction);
   s32 dest = INSTRUCTION_B(sh->instruction);

   tmp1 = sh->regs.R[source] + sh->regs.R[dest];
   tmp0 = sh->regs.R[dest];

   sh->regs.R[dest] = tmp1 + sh->regs.SR.part.T;
   if (tmp0 > tmp1)
      sh->regs.SR.part.T = 1;
   else
      sh->regs.SR.part.T = 0;
   if (tmp1 > sh->regs.R[dest])
      sh->regs.SR.part.T = 1;
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2addv()
{
   s32 dest,src,ans;
   s32 n = INSTRUCTION_B(sh->instruction);
   s32 m = INSTRUCTION_C(sh->instruction);

   if ((s32) sh->regs.R[n] >= 0)
      dest = 0;
   else
      dest = 1;
  
   if ((s32) sh->regs.R[m] >= 0)
      src = 0;
   else
      src = 1;
  
   src += dest;
   sh->regs.R[n] += sh->regs.R[m];

   if ((s32) sh->regs.R[n] >= 0)
      ans = 0;
   else
      ans = 1;

   ans += dest;
  
   if (src == 0 || src == 2)
      if (ans == 1)
         sh->regs.SR.part.T = 1;
      else
         sh->regs.SR.part.T = 0;
   else
      sh->regs.SR.part.T = 0;

   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2y_and()
{
   sh->regs.R[INSTRUCTION_B(sh->instruction)] &= sh->regs.R[INSTRUCTION_C(sh->instruction)];
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2andi()
{
   sh->regs.R[0] &= INSTRUCTION_CD(sh->instruction);
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2andm()
{
   s32 temp;
   s32 source = INSTRUCTION_CD(sh->instruction);

   temp = (s32) MappedMemoryReadByte(sh->regs.GBR + sh->regs.R[0]);
   temp &= source;
   MappedMemoryWriteByte((sh->regs.GBR + sh->regs.R[0]),temp);
   sh->regs.PC += 2;
   sh->cycles += 3;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2bf()
{
   if (sh->regs.SR.part.T == 0)
   {
      s32 disp = (s32)(s8)sh->instruction;

      sh->regs.PC = sh->regs.PC+(disp<<1)+4;
      sh->cycles += 3;
   }
   else
   {
      sh->regs.PC+=2;
      sh->cycles++;
   }
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2bfs()
{
   if (sh->regs.SR.part.T == 0)
   {
      s32 disp = (s32)(s8)sh->instruction;
      u32 temp = sh->regs.PC;

      sh->regs.PC = sh->regs.PC + (disp << 1) + 4;

      sh->cycles += 2;
      SH2delay<WHICH_SH2>(temp + 2);
   }
   else
   {
      sh->regs.PC += 2;
      sh->cycles++;
   }
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2bra()
{
   s32 disp = INSTRUCTION_BCD(sh->instruction);
   u32 temp = sh->regs.PC;

   if ((disp&0x800) != 0)
      disp |= 0xFFFFF000;

   sh->regs.PC = sh->regs.PC + (disp<<1) + 4;

   sh->cycles += 2;
   SH2delay<WHICH_SH2>(temp + 2);
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2braf()
{
   u32 temp;
   s32 m = INSTRUCTION_B(sh->instruction);

   temp = sh->regs.PC;
   sh->regs.PC += sh->regs.R[m] + 4; 

   sh->cycles += 2;
   SH2delay<WHICH_SH2>(temp + 2);
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void SH2bsr()
{
   u32 temp;
   s32 disp = INSTRUCTION_BCD(sh->instruction);

   temp = sh->regs.PC;
   if ((disp&0x800) != 0) disp |= 0xFFFFF000;
   sh->regs.PR = sh->regs.PC + 4;
   sh->regs.PC = sh->regs.PC+(disp<<1) + 4;

   sh->cycles += 2;
   SH2delay<WHICH_SH2>(temp + 2);
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2bsrf()
{
   u32 temp = sh->regs.PC;
   sh->regs.PR = sh->regs.PC + 4;
   sh->regs.PC += sh->regs.R[INSTRUCTION_B(sh->instruction)] + 4;
   sh->cycles += 2;
   SH2delay<WHICH_SH2>(temp + 2);
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2bt()
{
   if (sh->regs.SR.part.T == 1)
   {
      s32 disp = (s32)(s8)sh->instruction;

      sh->regs.PC = sh->regs.PC+(disp<<1)+4;
      sh->cycles += 3;
   }
   else
   {
      sh->regs.PC += 2;
      sh->cycles++;
   }
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2bts()
{
   if (sh->regs.SR.part.T)
   {
      s32 disp = (s32)(s8)sh->instruction;
      u32 temp = sh->regs.PC;

      sh->regs.PC += (disp << 1) + 4;
      sh->cycles += 2;
      SH2delay<WHICH_SH2>(temp + 2);
   }
   else
   {
      sh->regs.PC+=2;
      sh->cycles++;
   }
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2clrmac()
{
   sh->regs.MACH = 0;
   sh->regs.MACL = 0;
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2clrt()
{
   sh->regs.SR.part.T = 0;
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2cmpeq()
{
   if (sh->regs.R[INSTRUCTION_B(sh->instruction)] == sh->regs.R[INSTRUCTION_C(sh->instruction)])
      sh->regs.SR.part.T = 1;
   else
      sh->regs.SR.part.T = 0;
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2cmpge()
{
   if ((s32)sh->regs.R[INSTRUCTION_B(sh->instruction)] >=
       (s32)sh->regs.R[INSTRUCTION_C(sh->instruction)])
      sh->regs.SR.part.T = 1;
   else
      sh->regs.SR.part.T = 0;
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2cmpgt()
{
   if ((s32)sh->regs.R[INSTRUCTION_B(sh->instruction)]>(s32)sh->regs.R[INSTRUCTION_C(sh->instruction)])
      sh->regs.SR.part.T = 1;
   else
      sh->regs.SR.part.T = 0;
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2cmphi()
{
   if ((u32)sh->regs.R[INSTRUCTION_B(sh->instruction)] >
       (u32)sh->regs.R[INSTRUCTION_C(sh->instruction)])
      sh->regs.SR.part.T = 1;
   else
      sh->regs.SR.part.T = 0;
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2cmphs()
{
   if ((u32)sh->regs.R[INSTRUCTION_B(sh->instruction)] >=
       (u32)sh->regs.R[INSTRUCTION_C(sh->instruction)])
      sh->regs.SR.part.T = 1;
   else
      sh->regs.SR.part.T = 0;
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2cmpim()
{
   s32 imm;
   s32 i = INSTRUCTION_CD(sh->instruction);

   imm = (s32)(s8)i;

   if (sh->regs.R[0] == (u32) imm) // FIXME: ouais � doit �re bon...
      sh->regs.SR.part.T = 1;
   else
      sh->regs.SR.part.T = 0;
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2cmppl()
{
   if ((s32)sh->regs.R[INSTRUCTION_B(sh->instruction)]>0)
      sh->regs.SR.part.T = 1;
   else
      sh->regs.SR.part.T = 0;
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2cmppz()
{
   if ((s32)sh->regs.R[INSTRUCTION_B(sh->instruction)]>=0)
      sh->regs.SR.part.T = 1;
   else
      sh->regs.SR.part.T = 0;
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2cmpstr()
{
   u32 temp;
   s32 HH,HL,LH,LL;
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);
   temp=sh->regs.R[n]^sh->regs.R[m];
   HH = (temp>>24) & 0x000000FF;
   HL = (temp>>16) & 0x000000FF;
   LH = (temp>>8) & 0x000000FF;
   LL = temp & 0x000000FF;
   HH = HH && HL && LH && LL;
   if (HH == 0)
      sh->regs.SR.part.T = 1;
   else
      sh->regs.SR.part.T = 0;
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2div0s()
{
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);
   if ((sh->regs.R[n]&0x80000000)==0)
     sh->regs.SR.part.Q = 0;
   else
     sh->regs.SR.part.Q = 1;
   if ((sh->regs.R[m]&0x80000000)==0)
     sh->regs.SR.part.M = 0;
   else
     sh->regs.SR.part.M = 1;
   sh->regs.SR.part.T = !(sh->regs.SR.part.M == sh->regs.SR.part.Q);
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2div0u()
{
   sh->regs.SR.part.M = sh->regs.SR.part.Q = sh->regs.SR.part.T = 0;
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2div1()
{
   u32 tmp0;
   u8 old_q, tmp1;
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);
  
   old_q = sh->regs.SR.part.Q;
   sh->regs.SR.part.Q = (u8)((0x80000000 & sh->regs.R[n])!=0);
   sh->regs.R[n] <<= 1;
   sh->regs.R[n]|=(u32) sh->regs.SR.part.T;

   switch(old_q)
   {
      case 0:
         switch(sh->regs.SR.part.M)
         {
            case 0:
               tmp0 = sh->regs.R[n];
               sh->regs.R[n] -= sh->regs.R[m];
               tmp1 = (sh->regs.R[n] > tmp0);
               switch(sh->regs.SR.part.Q)
               {
                  case 0:
                     sh->regs.SR.part.Q = tmp1;
                     break;
                  case 1:
                     sh->regs.SR.part.Q = (u8) (tmp1 == 0);
                     break;
               }
               break;
            case 1:
               tmp0 = sh->regs.R[n];
               sh->regs.R[n] += sh->regs.R[m];
               tmp1 = (sh->regs.R[n] < tmp0);
               switch(sh->regs.SR.part.Q)
               {
                  case 0:
                     sh->regs.SR.part.Q = (u8) (tmp1 == 0);
                     break;
                  case 1:
                     sh->regs.SR.part.Q = tmp1;
                     break;
               }
               break;
         }
         break;
      case 1:
         switch(sh->regs.SR.part.M)
         {
            case 0:
               tmp0 = sh->regs.R[n];
               sh->regs.R[n] += sh->regs.R[m];
               tmp1 = (sh->regs.R[n] < tmp0);
               switch(sh->regs.SR.part.Q)
               {
                  case 0:
                     sh->regs.SR.part.Q = tmp1;
                     break;
                  case 1:
                     sh->regs.SR.part.Q = (u8) (tmp1 == 0);
                     break;
               }
               break;
            case 1:
               tmp0 = sh->regs.R[n];
               sh->regs.R[n] -= sh->regs.R[m];
               tmp1 = (sh->regs.R[n] > tmp0);
               switch(sh->regs.SR.part.Q)
               {
                  case 0:
                     sh->regs.SR.part.Q = (u8) (tmp1 == 0);
                     break;
                  case 1:
                     sh->regs.SR.part.Q = tmp1;
                     break;
               }
               break;
         }
         break;
   }
   sh->regs.SR.part.T = (sh->regs.SR.part.Q == sh->regs.SR.part.M);
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2dmuls()
{
   u32 RnL,RnH,RmL,RmH,Res0,Res1,Res2;
   u32 temp0,temp1,temp2,temp3;
   s32 tempm,tempn,fnLmL;
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);
  
   tempn = (s32)sh->regs.R[n];
   tempm = (s32)sh->regs.R[m];
   if (tempn < 0)
      tempn = 0 - tempn;
   if (tempm < 0)
      tempm = 0 - tempm;
   if ((s32) (sh->regs.R[n] ^ sh->regs.R[m]) < 0)
      fnLmL = -1;
   else
      fnLmL = 0;
  
   temp1 = (u32) tempn;
   temp2 = (u32) tempm;

   RnL = temp1 & 0x0000FFFF;
   RnH = (temp1 >> 16) & 0x0000FFFF;
   RmL = temp2 & 0x0000FFFF;
   RmH = (temp2 >> 16) & 0x0000FFFF;
  
   temp0 = RmL * RnL;
   temp1 = RmH * RnL;
   temp2 = RmL * RnH;
   temp3 = RmH * RnH;

   Res2 = 0;
   Res1 = temp1 + temp2;
   if (Res1 < temp1)
      Res2 += 0x00010000;

   temp1 = (Res1 << 16) & 0xFFFF0000;
   Res0 = temp0 + temp1;
   if (Res0 < temp0)
      Res2++;
  
   Res2 = Res2 + ((Res1 >> 16) & 0x0000FFFF) + temp3;

   if (fnLmL < 0)
   {
      Res2 = ~Res2;
      if (Res0 == 0)
         Res2++;
      else
         Res0 =(~Res0) + 1;
   }
   sh->regs.MACH = Res2;
   sh->regs.MACL = Res0;
   sh->regs.PC += 2;
   sh->cycles += 2;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2dmulu()
{
   u32 RnL,RnH,RmL,RmH,Res0,Res1,Res2;
   u32 temp0,temp1,temp2,temp3;
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   RnL = sh->regs.R[n] & 0x0000FFFF;
   RnH = (sh->regs.R[n] >> 16) & 0x0000FFFF;
   RmL = sh->regs.R[m] & 0x0000FFFF;
   RmH = (sh->regs.R[m] >> 16) & 0x0000FFFF;

   temp0 = RmL * RnL;
   temp1 = RmH * RnL;
   temp2 = RmL * RnH;
   temp3 = RmH * RnH;
  
   Res2 = 0;
   Res1 = temp1 + temp2;
   if (Res1 < temp1)
      Res2 += 0x00010000;
  
   temp1 = (Res1 << 16) & 0xFFFF0000;
   Res0 = temp0 + temp1;
   if (Res0 < temp0)
      Res2++;
  
   Res2 = Res2 + ((Res1 >> 16) & 0x0000FFFF) + temp3;
 
   sh->regs.MACH = Res2;
   sh->regs.MACL = Res0;
   sh->regs.PC += 2;
   sh->cycles += 2;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2dt()
{
   s32 n = INSTRUCTION_B(sh->instruction);

   sh->regs.R[n]--;
   if (sh->regs.R[n] == 0)
      sh->regs.SR.part.T = 1;
   else
      sh->regs.SR.part.T = 0;
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2extsb()
{
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   sh->regs.R[n] = (u32)(s8)sh->regs.R[m];
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2extsw()
{
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   sh->regs.R[n] = (u32)(s16)sh->regs.R[m];
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2extub()
{
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   sh->regs.R[n] = (u32)(u8)sh->regs.R[m];
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2extuw()
{
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   sh->regs.R[n] = (u32)(u16)sh->regs.R[m];
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2jmp()
{
   u32 temp;
   s32 m = INSTRUCTION_B(sh->instruction);

   temp=sh->regs.PC;
   sh->regs.PC = sh->regs.R[m];
   sh->cycles += 2;
   SH2delay<WHICH_SH2>(temp + 2);
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2jsr()
{
   u32 temp;
   s32 m = INSTRUCTION_B(sh->instruction);

   temp = sh->regs.PC;
   sh->regs.PR = sh->regs.PC + 4;
   sh->regs.PC = sh->regs.R[m];
   sh->cycles += 2;
   SH2delay<WHICH_SH2>(temp + 2);
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2ldcgbr()
{
   sh->regs.GBR = sh->regs.R[INSTRUCTION_B(sh->instruction)];
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2ldcmgbr()
{
   s32 m = INSTRUCTION_B(sh->instruction);

   sh->regs.GBR = MappedMemoryReadLong(sh->regs.R[m]);
   sh->regs.R[m] += 4;
   sh->regs.PC += 2;
   sh->cycles += 3;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2ldcmsr()
{
   s32 m = INSTRUCTION_B(sh->instruction);

   sh->regs.SR.all = MappedMemoryReadLong(sh->regs.R[m]) & 0x000003F3;
   sh->regs.R[m] += 4;
   sh->regs.PC += 2;
   sh->cycles += 3;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2ldcmvbr()
{
   s32 m = INSTRUCTION_B(sh->instruction);

   sh->regs.VBR = MappedMemoryReadLong(sh->regs.R[m]);
   sh->regs.R[m] += 4;
   sh->regs.PC += 2;
   sh->cycles += 3;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2ldcsr()
{
   sh->regs.SR.all = sh->regs.R[INSTRUCTION_B(sh->instruction)]&0x000003F3;
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2ldcvbr()
{
   s32 m = INSTRUCTION_B(sh->instruction);

   sh->regs.VBR = sh->regs.R[m];
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2ldsmach()
{
   sh->regs.MACH = sh->regs.R[INSTRUCTION_B(sh->instruction)];
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2ldsmacl()
{
   sh->regs.MACL = sh->regs.R[INSTRUCTION_B(sh->instruction)];
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2ldsmmach()
{
   s32 m = INSTRUCTION_B(sh->instruction);
   sh->regs.MACH = MappedMemoryReadLong(sh->regs.R[m]);
   sh->regs.R[m] += 4;
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2ldsmmacl()
{
   s32 m = INSTRUCTION_B(sh->instruction);
   sh->regs.MACL = MappedMemoryReadLong(sh->regs.R[m]);
   sh->regs.R[m] += 4;
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2ldsmpr()
{
   s32 m = INSTRUCTION_B(sh->instruction);
   sh->regs.PR = MappedMemoryReadLong(sh->regs.R[m]);
   sh->regs.R[m] += 4;
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2ldspr()
{
   sh->regs.PR = sh->regs.R[INSTRUCTION_B(sh->instruction)];
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2macl()
{
   u32 RnL,RnH,RmL,RmH,Res0,Res1,Res2;
   u32 temp0,temp1,temp2,temp3;
   s32 tempm,tempn,fnLmL;
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   tempn = (s32) MappedMemoryReadLong(sh->regs.R[n]);
   sh->regs.R[n] += 4;
   tempm = (s32) MappedMemoryReadLong(sh->regs.R[m]);
   sh->regs.R[m] += 4;

   if ((s32) (tempn^tempm) < 0)
      fnLmL =- 1;
   else
      fnLmL = 0;
   if (tempn < 0)
      tempn = 0 - tempn;
   if (tempm < 0)
      tempm = 0 - tempm;

   temp1 = (u32) tempn;
   temp2 = (u32) tempm;

   RnL = temp1 & 0x0000FFFF;
   RnH = (temp1 >> 16) & 0x0000FFFF;
   RmL = temp2 & 0x0000FFFF;
   RmH = (temp2 >> 16) & 0x0000FFFF;

   temp0 = RmL * RnL;
   temp1 = RmH * RnL;
   temp2 = RmL * RnH;
   temp3 = RmH * RnH;

   Res2 = 0;
   Res1 = temp1 + temp2;
   if (Res1 < temp1)
      Res2 += 0x00010000;

   temp1 = (Res1 << 16) & 0xFFFF0000;
   Res0 = temp0 + temp1;
   if (Res0 < temp0)
      Res2++;

   Res2=Res2+((Res1>>16)&0x0000FFFF)+temp3;

   if(fnLmL < 0)
   {
      Res2=~Res2;
      if (Res0==0)
         Res2++;
      else
         Res0=(~Res0)+1;
   }
   if(sh->regs.SR.part.S == 1)
   {
      Res0=sh->regs.MACL+Res0;
      if (sh->regs.MACL>Res0)
         Res2++;
      if (sh->regs.MACH & 0x00008000);
      else Res2 += sh->regs.MACH | 0xFFFF0000;
      Res2+=(sh->regs.MACH&0x0000FFFF);
      if(((s32)Res2<0)&&(Res2<0xFFFF8000))
      {
         Res2=0x00008000;
         Res0=0x00000000;
      }
      if(((s32)Res2>0)&&(Res2>0x00007FFF))
      {
         Res2=0x00007FFF;
         Res0=0xFFFFFFFF;
      };

      sh->regs.MACH=Res2;
      sh->regs.MACL=Res0;
   }
   else
   {
      Res0=sh->regs.MACL+Res0;
      if (sh->regs.MACL>Res0)
         Res2++;
      Res2+=sh->regs.MACH;

      sh->regs.MACH=Res2;
      sh->regs.MACL=Res0;
   }

   sh->regs.PC+=2;
   sh->cycles += 3;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2macw()
{
   s32 tempm,tempn,dest,src,ans;
   u32 templ;
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   tempn=(s32) MappedMemoryReadWord(sh->regs.R[n]);
   sh->regs.R[n]+=2;
   tempm=(s32) MappedMemoryReadWord(sh->regs.R[m]);
   sh->regs.R[m]+=2;
   templ=sh->regs.MACL;
   tempm=((s32)(s16)tempn*(s32)(s16)tempm);

   if ((s32)sh->regs.MACL>=0)
      dest=0;
   else
      dest=1;
   if ((s32)tempm>=0)
   {
      src=0;
      tempn=0;
   }
   else
   {
      src=1;
      tempn=0xFFFFFFFF;
   }
   src+=dest;
   sh->regs.MACL+=tempm;
   if ((s32)sh->regs.MACL>=0)
      ans=0;
   else
      ans=1;
   ans+=dest;
   if (sh->regs.SR.part.S == 1)
   {
      if (ans==1)
      {
         if (src==0)
            sh->regs.MACL=0x7FFFFFFF;
         if (src==2)
            sh->regs.MACL=0x80000000;
      }
   }
   else
   {
      sh->regs.MACH+=tempn;
      if (templ>sh->regs.MACL)
         sh->regs.MACH+=1;
   }
   sh->regs.PC+=2;
   sh->cycles += 3;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2mov()
{
   sh->regs.R[INSTRUCTION_B(sh->instruction)]=sh->regs.R[INSTRUCTION_C(sh->instruction)];
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2mova()
{
   s32 disp = INSTRUCTION_CD(sh->instruction);

   sh->regs.R[0]=((sh->regs.PC+4)&0xFFFFFFFC)+(disp<<2);
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2movbl()
{
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   sh->regs.R[n] = (s32)(s8)MappedMemoryReadByte(sh->regs.R[m]);
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2movbl0()
{
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   sh->regs.R[n] = (s32)(s8)MappedMemoryReadByte(sh->regs.R[m] + sh->regs.R[0]);
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2movbl4()
{
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 disp = INSTRUCTION_D(sh->instruction);

   sh->regs.R[0] = (s32)(s8)MappedMemoryReadByte(sh->regs.R[m] + disp);
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2movblg()
{
   s32 disp = INSTRUCTION_CD(sh->instruction);
  
   sh->regs.R[0] = (s32)(s8)MappedMemoryReadByte(sh->regs.GBR + disp);
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2movbm()
{
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   MappedMemoryWriteByte((sh->regs.R[n] - 1),sh->regs.R[m]);
   sh->regs.R[n] -= 1;
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2movbp()
{
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   sh->regs.R[n] = (s32)(s8)MappedMemoryReadByte(sh->regs.R[m]);
   if (n != m)
     sh->regs.R[m] += 1;
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2movbs()
{
   int b = INSTRUCTION_B(sh->instruction);
   int c = INSTRUCTION_C(sh->instruction);

   MappedMemoryWriteByte(sh->regs.R[b], sh->regs.R[c]);
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2movbs0()
{
   MappedMemoryWriteByte(sh->regs.R[INSTRUCTION_B(sh->instruction)] + sh->regs.R[0],
                         sh->regs.R[INSTRUCTION_C(sh->instruction)]);
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2movbs4()
{
   s32 disp = INSTRUCTION_D(sh->instruction);
   s32 n = INSTRUCTION_C(sh->instruction);

   MappedMemoryWriteByte(sh->regs.R[n]+disp,sh->regs.R[0]);
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2movbsg()
{
   s32 disp = INSTRUCTION_CD(sh->instruction);

   MappedMemoryWriteByte(sh->regs.GBR + disp,sh->regs.R[0]);
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2movi()
{
   s32 i = INSTRUCTION_CD(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   sh->regs.R[n] = (s32)(s8)i;
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2movli()
{
   s32 disp = INSTRUCTION_CD(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   sh->regs.R[n] = MappedMemoryReadLong(((sh->regs.PC + 4) & 0xFFFFFFFC) + (disp << 2));
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2movll()
{
   sh->regs.R[INSTRUCTION_B(sh->instruction)] = MappedMemoryReadLong(sh->regs.R[INSTRUCTION_C(sh->instruction)]);
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2movll0()
{
   sh->regs.R[INSTRUCTION_B(sh->instruction)] = MappedMemoryReadLong(sh->regs.R[INSTRUCTION_C(sh->instruction)] + sh->regs.R[0]);
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2movll4()
{
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 disp = INSTRUCTION_D(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   sh->regs.R[n] = MappedMemoryReadLong(sh->regs.R[m] + (disp << 2));
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2movllg()
{
   s32 disp = INSTRUCTION_CD(sh->instruction);

   sh->regs.R[0] = MappedMemoryReadLong(sh->regs.GBR + (disp << 2));
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2movlm()
{
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   MappedMemoryWriteLong(sh->regs.R[n] - 4,sh->regs.R[m]);
   sh->regs.R[n] -= 4;
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2movlp()
{
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   sh->regs.R[n] = MappedMemoryReadLong(sh->regs.R[m]);
   if (n != m) sh->regs.R[m] += 4;
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2movls()
{
   int b = INSTRUCTION_B(sh->instruction);
   int c = INSTRUCTION_C(sh->instruction);

   MappedMemoryWriteLong(sh->regs.R[b], sh->regs.R[c]);
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2movls0()
{
   MappedMemoryWriteLong(sh->regs.R[INSTRUCTION_B(sh->instruction)] + sh->regs.R[0],
                         sh->regs.R[INSTRUCTION_C(sh->instruction)]);
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2movls4()
{
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 disp = INSTRUCTION_D(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   MappedMemoryWriteLong(sh->regs.R[n]+(disp<<2),sh->regs.R[m]);
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2movlsg()
{
   s32 disp = INSTRUCTION_CD(sh->instruction);

   MappedMemoryWriteLong(sh->regs.GBR+(disp<<2),sh->regs.R[0]);
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2movt()
{
   sh->regs.R[INSTRUCTION_B(sh->instruction)] = (0x00000001 & sh->regs.SR.all);
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2movwi()
{
   s32 disp = INSTRUCTION_CD(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   sh->regs.R[n] = (s32)(s16)MappedMemoryReadWord(sh->regs.PC + (disp<<1) + 4);
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2movwl()
{
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   sh->regs.R[n] = (s32)(s16)MappedMemoryReadWord(sh->regs.R[m]);
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2movwl0()
{
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   sh->regs.R[n] = (s32)(s16)MappedMemoryReadWord(sh->regs.R[m]+sh->regs.R[0]);
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2movwl4()
{
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 disp = INSTRUCTION_D(sh->instruction);

   sh->regs.R[0] = (s32)(s16)MappedMemoryReadWord(sh->regs.R[m]+(disp<<1));
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2movwlg()
{
   s32 disp = INSTRUCTION_CD(sh->instruction);

   sh->regs.R[0] = (s32)(s16)MappedMemoryReadWord(sh->regs.GBR+(disp<<1));
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2movwm()
{
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   MappedMemoryWriteWord(sh->regs.R[n] - 2,sh->regs.R[m]);
   sh->regs.R[n] -= 2;
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2movwp()
{
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   sh->regs.R[n] = (s32)(s16)MappedMemoryReadWord(sh->regs.R[m]);
   if (n != m)
      sh->regs.R[m] += 2;
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2movws()
{
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   MappedMemoryWriteWord(sh->regs.R[n],sh->regs.R[m]);
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2movws0()
{
   MappedMemoryWriteWord(sh->regs.R[INSTRUCTION_B(sh->instruction)] + sh->regs.R[0],
                         sh->regs.R[INSTRUCTION_C(sh->instruction)]);
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2movws4()
{
   s32 disp = INSTRUCTION_D(sh->instruction);
   s32 n = INSTRUCTION_C(sh->instruction);

   MappedMemoryWriteWord(sh->regs.R[n]+(disp<<1),sh->regs.R[0]);
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2movwsg()
{
   s32 disp = INSTRUCTION_CD(sh->instruction);

   MappedMemoryWriteWord(sh->regs.GBR+(disp<<1),sh->regs.R[0]);
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2mull()
{
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   sh->regs.MACL = sh->regs.R[n] * sh->regs.R[m];
   sh->regs.PC+=2;
   sh->cycles += 2;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2muls()
{
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   sh->regs.MACL = ((s32)(s16)sh->regs.R[n]*(s32)(s16)sh->regs.R[m]);
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2mulu()
{
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   sh->regs.MACL = ((u32)(u16)sh->regs.R[n] * (u32)(u16)sh->regs.R[m]);
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2neg()
{
   sh->regs.R[INSTRUCTION_B(sh->instruction)]=0-sh->regs.R[INSTRUCTION_C(sh->instruction)];
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2negc()
{
   u32 temp;
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);
  
   temp=0-sh->regs.R[m];
   sh->regs.R[n] = temp - sh->regs.SR.part.T;
   if (0 < temp)
      sh->regs.SR.part.T=1;
   else
      sh->regs.SR.part.T=0;
   if (temp < sh->regs.R[n])
      sh->regs.SR.part.T=1;
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2nop()
{
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2y_not()
{
   sh->regs.R[INSTRUCTION_B(sh->instruction)] = ~sh->regs.R[INSTRUCTION_C(sh->instruction)];
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2y_or()
{
   sh->regs.R[INSTRUCTION_B(sh->instruction)] |= sh->regs.R[INSTRUCTION_C(sh->instruction)];
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2ori()
{
   sh->regs.R[0] |= INSTRUCTION_CD(sh->instruction);
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2orm()
{
   s32 temp;
   s32 source = INSTRUCTION_CD(sh->instruction);

   temp = (s32) MappedMemoryReadByte(sh->regs.GBR + sh->regs.R[0]);
   temp |= source;
   MappedMemoryWriteByte(sh->regs.GBR + sh->regs.R[0],temp);
   sh->regs.PC += 2;
   sh->cycles += 3;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2rotcl()
{
   s32 temp;
   s32 n = INSTRUCTION_B(sh->instruction);

   if ((sh->regs.R[n]&0x80000000)==0)
      temp=0;
   else
      temp=1;

   sh->regs.R[n]<<=1;

   if (sh->regs.SR.part.T == 1)
      sh->regs.R[n]|=0x00000001;
   else
      sh->regs.R[n]&=0xFFFFFFFE;

   if (temp==1)
      sh->regs.SR.part.T=1;
   else
      sh->regs.SR.part.T=0;

   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2rotcr()
{
   s32 temp;
   s32 n = INSTRUCTION_B(sh->instruction);

   if ((sh->regs.R[n]&0x00000001)==0)
      temp=0;
   else
      temp=1;

   sh->regs.R[n]>>=1;

   if (sh->regs.SR.part.T == 1)
      sh->regs.R[n]|=0x80000000;
   else
      sh->regs.R[n]&=0x7FFFFFFF;

   if (temp==1)
      sh->regs.SR.part.T=1;
   else
      sh->regs.SR.part.T=0;

   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2rotl()
{
   s32 n = INSTRUCTION_B(sh->instruction);

   if ((sh->regs.R[n]&0x80000000)==0)
      sh->regs.SR.part.T=0;
   else
      sh->regs.SR.part.T=1;

   sh->regs.R[n]<<=1;

   if (sh->regs.SR.part.T==1)
      sh->regs.R[n]|=0x00000001;
   else
      sh->regs.R[n]&=0xFFFFFFFE;

   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2rotr()
{
   s32 n = INSTRUCTION_B(sh->instruction);

   if ((sh->regs.R[n]&0x00000001)==0)
      sh->regs.SR.part.T = 0;
   else
      sh->regs.SR.part.T = 1;

   sh->regs.R[n]>>=1;

   if (sh->regs.SR.part.T == 1)
      sh->regs.R[n]|=0x80000000;
   else
      sh->regs.R[n]&=0x7FFFFFFF;

   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2rte()
{
   u32 temp;
   temp=sh->regs.PC;
   sh->regs.PC = MappedMemoryReadLong(sh->regs.R[15]);
   sh->regs.R[15] += 4;
   sh->regs.SR.all = MappedMemoryReadLong(sh->regs.R[15]) & 0x000003F3;
   sh->regs.R[15] += 4;
   sh->cycles += 4;
   SH2delay<WHICH_SH2>(temp + 2);
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2rts()
{
   u32 temp;

   temp = sh->regs.PC;
   sh->regs.PC = sh->regs.PR;

   sh->cycles += 2;
   SH2delay<WHICH_SH2>(temp + 2);
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2sett()
{
   sh->regs.SR.part.T = 1;
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2shal()
{
   s32 n = INSTRUCTION_B(sh->instruction);
   if ((sh->regs.R[n] & 0x80000000) == 0)
      sh->regs.SR.part.T = 0;
   else
      sh->regs.SR.part.T = 1;
   sh->regs.R[n] <<= 1;
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2shar()
{
   s32 temp;
   s32 n = INSTRUCTION_B(sh->instruction);

   if ((sh->regs.R[n]&0x00000001)==0)
      sh->regs.SR.part.T = 0;
   else
      sh->regs.SR.part.T = 1;

   if ((sh->regs.R[n]&0x80000000)==0)
      temp = 0;
   else
      temp = 1;

   sh->regs.R[n] >>= 1;

   if (temp == 1)
      sh->regs.R[n] |= 0x80000000;
   else
      sh->regs.R[n] &= 0x7FFFFFFF;

   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2shll()
{
   s32 n = INSTRUCTION_B(sh->instruction);

   if ((sh->regs.R[n]&0x80000000)==0)
      sh->regs.SR.part.T=0;
   else
      sh->regs.SR.part.T=1;
 
   sh->regs.R[n]<<=1;
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2shll2()
{
   sh->regs.R[INSTRUCTION_B(sh->instruction)] <<= 2;
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2shll8()
{
   sh->regs.R[INSTRUCTION_B(sh->instruction)]<<=8;
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2shll16()
{
   sh->regs.R[INSTRUCTION_B(sh->instruction)]<<=16;
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2shlr()
{
   s32 n = INSTRUCTION_B(sh->instruction);

   if ((sh->regs.R[n]&0x00000001)==0)
      sh->regs.SR.part.T=0;
   else
      sh->regs.SR.part.T=1;

   sh->regs.R[n]>>=1;
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2shlr2()
{
   s32 n = INSTRUCTION_B(sh->instruction);
   sh->regs.R[n]>>=2;
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2shlr8()
{
   s32 n = INSTRUCTION_B(sh->instruction);
   sh->regs.R[n]>>=8;
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2shlr16()
{
   s32 n = INSTRUCTION_B(sh->instruction);
   sh->regs.R[n]>>=16;
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2stcgbr()
{
   s32 n = INSTRUCTION_B(sh->instruction);
   sh->regs.R[n]=sh->regs.GBR;
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2stcmgbr()
{
   s32 n = INSTRUCTION_B(sh->instruction);
   sh->regs.R[n]-=4;
   MappedMemoryWriteLong(sh->regs.R[n],sh->regs.GBR);
   sh->regs.PC+=2;
   sh->cycles += 2;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2stcmsr()
{
   s32 n = INSTRUCTION_B(sh->instruction);
   sh->regs.R[n]-=4;
   MappedMemoryWriteLong(sh->regs.R[n],sh->regs.SR.all);
   sh->regs.PC+=2;
   sh->cycles += 2;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2stcmvbr()
{
   s32 n = INSTRUCTION_B(sh->instruction);
   sh->regs.R[n]-=4;
   MappedMemoryWriteLong(sh->regs.R[n],sh->regs.VBR);
   sh->regs.PC+=2;
   sh->cycles += 2;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2stcsr()
{
   s32 n = INSTRUCTION_B(sh->instruction);
   sh->regs.R[n] = sh->regs.SR.all;
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2stcvbr()
{
   s32 n = INSTRUCTION_B(sh->instruction);
   sh->regs.R[n]=sh->regs.VBR;
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2stsmach()
{
   s32 n = INSTRUCTION_B(sh->instruction);
   sh->regs.R[n]=sh->regs.MACH;
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2stsmacl()
{
   s32 n = INSTRUCTION_B(sh->instruction);
   sh->regs.R[n]=sh->regs.MACL;
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2stsmmach()
{
   s32 n = INSTRUCTION_B(sh->instruction);
   sh->regs.R[n] -= 4;
   MappedMemoryWriteLong(sh->regs.R[n],sh->regs.MACH); 
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2stsmmacl()
{
   s32 n = INSTRUCTION_B(sh->instruction);
   sh->regs.R[n] -= 4;
   MappedMemoryWriteLong(sh->regs.R[n],sh->regs.MACL);
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2stsmpr()
{
   s32 n = INSTRUCTION_B(sh->instruction);
   sh->regs.R[n] -= 4;
   MappedMemoryWriteLong(sh->regs.R[n],sh->regs.PR);
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2stspr()
{
   s32 n = INSTRUCTION_B(sh->instruction);
   sh->regs.R[n] = sh->regs.PR;
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2sub()
{
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);
   sh->regs.R[n]-=sh->regs.R[m];
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2subc()
{
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);
   u32 tmp0,tmp1;
  
   tmp1 = sh->regs.R[n] - sh->regs.R[m];
   tmp0 = sh->regs.R[n];
   sh->regs.R[n] = tmp1 - sh->regs.SR.part.T;

   if (tmp0 < tmp1)
      sh->regs.SR.part.T = 1;
   else
      sh->regs.SR.part.T = 0;

   if (tmp1 < sh->regs.R[n])
      sh->regs.SR.part.T = 1;

   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2subv()
{
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);
   s32 dest,src,ans;

   if ((s32)sh->regs.R[n]>=0)
      dest=0;
   else
      dest=1;

   if ((s32)sh->regs.R[m]>=0)
      src=0;
   else
      src=1;

   src+=dest;
   sh->regs.R[n]-=sh->regs.R[m];

   if ((s32)sh->regs.R[n]>=0)
      ans=0;
   else
      ans=1;

   ans+=dest;

   if (src==1)
   {
     if (ans==1)
        sh->regs.SR.part.T=1;
     else
        sh->regs.SR.part.T=0;
   }
   else
      sh->regs.SR.part.T=0;

   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2swapb()
{
   u32 temp0,temp1;
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   temp0=sh->regs.R[m]&0xffff0000;
   temp1=(sh->regs.R[m]&0x000000ff)<<8;
   sh->regs.R[n]=(sh->regs.R[m]>>8)&0x000000ff;
   sh->regs.R[n]=sh->regs.R[n]|temp1|temp0;
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2swapw()
{
   u32 temp;
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);
   temp=(sh->regs.R[m]>>16)&0x0000FFFF;
   sh->regs.R[n]=sh->regs.R[m]<<16;
   sh->regs.R[n]|=temp;
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2tas()
{
   s32 temp;
   s32 n = INSTRUCTION_B(sh->instruction);

   temp=(s32) MappedMemoryReadByte(sh->regs.R[n]);

   if (temp==0)
      sh->regs.SR.part.T=1;
   else
      sh->regs.SR.part.T=0;

   temp|=0x00000080;
   MappedMemoryWriteByte(sh->regs.R[n],temp);
   sh->regs.PC+=2;
   sh->cycles += 4;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2trapa()
{
   s32 imm = INSTRUCTION_CD(sh->instruction);

   sh->regs.R[15]-=4;
   MappedMemoryWriteLong(sh->regs.R[15],sh->regs.SR.all);
   sh->regs.R[15]-=4;
   MappedMemoryWriteLong(sh->regs.R[15],sh->regs.PC + 2);
   sh->regs.PC = MappedMemoryReadLong(sh->regs.VBR+(imm<<2));
   sh->cycles += 8;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2tst()
{
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   if ((sh->regs.R[n]&sh->regs.R[m])==0)
      sh->regs.SR.part.T = 1;
   else
      sh->regs.SR.part.T = 0;

   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2tsti()
{
   s32 temp;
   s32 i = INSTRUCTION_CD(sh->instruction);

   temp=sh->regs.R[0]&i;

   if (temp==0)
      sh->regs.SR.part.T = 1;
   else
      sh->regs.SR.part.T = 0;

   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2tstm()
{
   s32 temp;
   s32 i = INSTRUCTION_CD(sh->instruction);

   temp=(s32) MappedMemoryReadByte(sh->regs.GBR+sh->regs.R[0]);
   temp&=i;

   if (temp==0)
      sh->regs.SR.part.T = 1;
   else
      sh->regs.SR.part.T = 0;

   sh->regs.PC+=2;
   sh->cycles += 3;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2y_xor()
{
   int b = INSTRUCTION_B(sh->instruction);
   int c = INSTRUCTION_C(sh->instruction);

   sh->regs.R[b] ^= sh->regs.R[c];
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2xori()
{
   s32 source = INSTRUCTION_CD(sh->instruction);
   sh->regs.R[0] ^= source;
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2xorm()
{
   s32 source = INSTRUCTION_CD(sh->instruction);
   s32 temp;

   temp = (s32) MappedMemoryReadByte(sh->regs.GBR + sh->regs.R[0]);
   temp ^= source;
   MappedMemoryWriteByte(sh->regs.GBR + sh->regs.R[0],temp);
   sh->regs.PC += 2;
   sh->cycles += 3;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2xtrct()
{
   u32 temp;
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   temp=(sh->regs.R[m]<<16)&0xFFFF0000;
   sh->regs.R[n]=(sh->regs.R[n]>>16)&0x0000FFFF;
   sh->regs.R[n]|=temp;
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE void FASTCALL SH2sleep()
{
   sh->cycles += 3;
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE opcodefunc decode(u16 instruction)
{
   switch (INSTRUCTION_A(instruction))
   {
      case 0:
         switch (INSTRUCTION_D(instruction))
         {
            case 2:
               switch (INSTRUCTION_C(instruction))
               {
                  case 0: return &SH2stcsr<WHICH_SH2>;
                  case 1: return &SH2stcgbr<WHICH_SH2>;
                  case 2: return &SH2stcvbr<WHICH_SH2>;
                  default: return &SH2undecoded<WHICH_SH2>;
               }
     
            case 3:
               switch (INSTRUCTION_C(instruction))
               {
                  case 0: return &SH2bsrf<WHICH_SH2>;
                  case 2: return &SH2braf<WHICH_SH2>;
                  default: return &SH2undecoded<WHICH_SH2>;
               }
     
            case 4: return &SH2movbs0<WHICH_SH2>;
            case 5: return &SH2movws0<WHICH_SH2>;
            case 6: return &SH2movls0<WHICH_SH2>;
            case 7: return &SH2mull<WHICH_SH2>;
            case 8:
               switch (INSTRUCTION_C(instruction))
               {
                  case 0: return &SH2clrt<WHICH_SH2>;
                  case 1: return &SH2sett<WHICH_SH2>;
                  case 2: return &SH2clrmac<WHICH_SH2>;
                  default: return &SH2undecoded<WHICH_SH2>;
               }     
            case 9:
               switch (INSTRUCTION_C(instruction))
               {
                  case 0: return &SH2nop<WHICH_SH2>;
                  case 1: return &SH2div0u<WHICH_SH2>;
                  case 2: return &SH2movt<WHICH_SH2>;
                  default: return &SH2undecoded<WHICH_SH2>;
               }     
            case 10:
               switch (INSTRUCTION_C(instruction))
               {
                  case 0: return &SH2stsmach<WHICH_SH2>;
                  case 1: return &SH2stsmacl<WHICH_SH2>;
                  case 2: return &SH2stspr<WHICH_SH2>;
                  default: return &SH2undecoded<WHICH_SH2>;
               }     
            case 11:
               switch (INSTRUCTION_C(instruction))
               {
                  case 0: return &SH2rts<WHICH_SH2>;
                  case 1: return &SH2sleep<WHICH_SH2>;
                  case 2: return &SH2rte<WHICH_SH2>;
                  default: return &SH2undecoded<WHICH_SH2>;
               }     
            case 12: return &SH2movbl0<WHICH_SH2>;
            case 13: return &SH2movwl0<WHICH_SH2>;
            case 14: return &SH2movll0<WHICH_SH2>;
            case 15: return &SH2macl<WHICH_SH2>;
            default: return &SH2undecoded<WHICH_SH2>;
         }
   
      case 1: return &SH2movls4<WHICH_SH2>;
      case 2:
         switch (INSTRUCTION_D(instruction))
         {
            case 0: return &SH2movbs<WHICH_SH2>;
            case 1: return &SH2movws<WHICH_SH2>;
            case 2: return &SH2movls<WHICH_SH2>;
            case 4: return &SH2movbm<WHICH_SH2>;
            case 5: return &SH2movwm<WHICH_SH2>;
            case 6: return &SH2movlm<WHICH_SH2>;
            case 7: return &SH2div0s<WHICH_SH2>;
            case 8: return &SH2tst<WHICH_SH2>;
            case 9: return &SH2y_and<WHICH_SH2>;
            case 10: return &SH2y_xor<WHICH_SH2>;
            case 11: return &SH2y_or<WHICH_SH2>;
            case 12: return &SH2cmpstr<WHICH_SH2>;
            case 13: return &SH2xtrct<WHICH_SH2>;
            case 14: return &SH2mulu<WHICH_SH2>;
            case 15: return &SH2muls<WHICH_SH2>;
            default: return &SH2undecoded<WHICH_SH2>;
         }
   
      case 3:
         switch(INSTRUCTION_D(instruction))
         {
            case 0:  return &SH2cmpeq<WHICH_SH2>;
            case 2:  return &SH2cmphs<WHICH_SH2>;
            case 3:  return &SH2cmpge<WHICH_SH2>;
            case 4:  return &SH2div1<WHICH_SH2>;
            case 5:  return &SH2dmulu<WHICH_SH2>;
            case 6:  return &SH2cmphi<WHICH_SH2>;
            case 7:  return &SH2cmpgt<WHICH_SH2>;
            case 8:  return &SH2sub<WHICH_SH2>;
            case 10: return &SH2subc<WHICH_SH2>;
            case 11: return &SH2subv<WHICH_SH2>;
            case 12: return &SH2add<WHICH_SH2>;
            case 13: return &SH2dmuls<WHICH_SH2>;
            case 14: return &SH2addc<WHICH_SH2>;
            case 15: return &SH2addv<WHICH_SH2>;
            default: return &SH2undecoded<WHICH_SH2>;
         }
   
      case 4:
         switch(INSTRUCTION_D(instruction))
         {
            case 0:
               switch(INSTRUCTION_C(instruction))
               {
                  case 0: return &SH2shll<WHICH_SH2>;
                  case 1: return &SH2dt<WHICH_SH2>;
                  case 2: return &SH2shal<WHICH_SH2>;
                  default: return &SH2undecoded<WHICH_SH2>;
               }
            case 1:
               switch(INSTRUCTION_C(instruction))
               {
                  case 0: return &SH2shlr<WHICH_SH2>;
                  case 1: return &SH2cmppz<WHICH_SH2>;
                  case 2: return &SH2shar<WHICH_SH2>;
                  default: return &SH2undecoded<WHICH_SH2>;
               }     
            case 2:
               switch(INSTRUCTION_C(instruction))
               {
                  case 0: return &SH2stsmmach<WHICH_SH2>;
                  case 1: return &SH2stsmmacl<WHICH_SH2>;
                  case 2: return &SH2stsmpr<WHICH_SH2>;
                  default: return &SH2undecoded<WHICH_SH2>;
               }
            case 3:
               switch(INSTRUCTION_C(instruction))
               {
                  case 0: return &SH2stcmsr<WHICH_SH2>;
                  case 1: return &SH2stcmgbr<WHICH_SH2>;
                  case 2: return &SH2stcmvbr<WHICH_SH2>;
                  default: return &SH2undecoded<WHICH_SH2>;
               }
            case 4:
               switch(INSTRUCTION_C(instruction))
               {
                  case 0: return &SH2rotl<WHICH_SH2>;
                  case 2: return &SH2rotcl<WHICH_SH2>;
                  default: return &SH2undecoded<WHICH_SH2>;
               }     
            case 5:
               switch(INSTRUCTION_C(instruction))
               {
                  case 0: return &SH2rotr<WHICH_SH2>;
                  case 1: return &SH2cmppl<WHICH_SH2>;
                  case 2: return &SH2rotcr<WHICH_SH2>;
                  default: return &SH2undecoded<WHICH_SH2>;
               }                 
            case 6:
               switch(INSTRUCTION_C(instruction))
               {
                  case 0: return &SH2ldsmmach<WHICH_SH2>;
                  case 1: return &SH2ldsmmacl<WHICH_SH2>;
                  case 2: return &SH2ldsmpr<WHICH_SH2>;
                  default: return &SH2undecoded<WHICH_SH2>;
               }     
            case 7:
               switch(INSTRUCTION_C(instruction))
               {
                  case 0: return &SH2ldcmsr<WHICH_SH2>;
                  case 1: return &SH2ldcmgbr<WHICH_SH2>;
                  case 2: return &SH2ldcmvbr<WHICH_SH2>;
                  default: return &SH2undecoded<WHICH_SH2>;
               }     
            case 8:
               switch(INSTRUCTION_C(instruction))
               {
                  case 0: return &SH2shll2<WHICH_SH2>;
                  case 1: return &SH2shll8<WHICH_SH2>;
                  case 2: return &SH2shll16<WHICH_SH2>;
                  default: return &SH2undecoded<WHICH_SH2>;
               }     
            case 9:
               switch(INSTRUCTION_C(instruction))
               {
                  case 0: return &SH2shlr2<WHICH_SH2>;
                  case 1: return &SH2shlr8<WHICH_SH2>;
                  case 2: return &SH2shlr16<WHICH_SH2>;
                  default: return &SH2undecoded<WHICH_SH2>;
               }     
            case 10:
               switch(INSTRUCTION_C(instruction))
               {
                  case 0: return &SH2ldsmach<WHICH_SH2>;
                  case 1: return &SH2ldsmacl<WHICH_SH2>;
                  case 2: return &SH2ldspr<WHICH_SH2>;
                  default: return &SH2undecoded<WHICH_SH2>;
               }     
            case 11:
               switch(INSTRUCTION_C(instruction))
               {
                  case 0: return &SH2jsr<WHICH_SH2>;
                  case 1: return &SH2tas<WHICH_SH2>;
                  case 2: return &SH2jmp<WHICH_SH2>;
                  default: return &SH2undecoded<WHICH_SH2>;
               }     
            case 14:
               switch(INSTRUCTION_C(instruction))
               {
                  case 0: return &SH2ldcsr<WHICH_SH2>;
                  case 1: return &SH2ldcgbr<WHICH_SH2>;
                  case 2: return &SH2ldcvbr<WHICH_SH2>;
                  default: return &SH2undecoded<WHICH_SH2>;
               }
            case 15: return &SH2macw<WHICH_SH2>;
            default: return &SH2undecoded<WHICH_SH2>;
         }
      case 5: return &SH2movll4<WHICH_SH2>;
      case 6:
         switch (INSTRUCTION_D(instruction))
         {
            case 0:  return &SH2movbl<WHICH_SH2>;
            case 1:  return &SH2movwl<WHICH_SH2>;
            case 2:  return &SH2movll<WHICH_SH2>;
            case 3:  return &SH2mov<WHICH_SH2>;
            case 4:  return &SH2movbp<WHICH_SH2>;
            case 5:  return &SH2movwp<WHICH_SH2>;
            case 6:  return &SH2movlp<WHICH_SH2>;
            case 7:  return &SH2y_not<WHICH_SH2>;
            case 8:  return &SH2swapb<WHICH_SH2>;
            case 9:  return &SH2swapw<WHICH_SH2>;
            case 10: return &SH2negc<WHICH_SH2>;
            case 11: return &SH2neg<WHICH_SH2>;
            case 12: return &SH2extub<WHICH_SH2>;
            case 13: return &SH2extuw<WHICH_SH2>;
            case 14: return &SH2extsb<WHICH_SH2>;
            case 15: return &SH2extsw<WHICH_SH2>;
         }
   
      case 7: return &SH2addi<WHICH_SH2>;
      case 8:
         switch (INSTRUCTION_B(instruction))
         {
            case 0:  return &SH2movbs4<WHICH_SH2>;
            case 1:  return &SH2movws4<WHICH_SH2>;
            case 4:  return &SH2movbl4<WHICH_SH2>;
            case 5:  return &SH2movwl4<WHICH_SH2>;
            case 8:  return &SH2cmpim<WHICH_SH2>;
            case 9:  return &SH2bt<WHICH_SH2>;
            case 11: return &SH2bf<WHICH_SH2>;
            case 13: return &SH2bts<WHICH_SH2>;
            case 15: return &SH2bfs<WHICH_SH2>;
            default: return &SH2undecoded<WHICH_SH2>;
         }   
      case 9: return &SH2movwi<WHICH_SH2>;
      case 10: return &SH2bra<WHICH_SH2>;
      case 11: return &SH2bsr<WHICH_SH2>;
      case 12:
         switch(INSTRUCTION_B(instruction))
         {
            case 0:  return &SH2movbsg<WHICH_SH2>;
            case 1:  return &SH2movwsg<WHICH_SH2>;
            case 2:  return &SH2movlsg<WHICH_SH2>;
            case 3:  return &SH2trapa<WHICH_SH2>;
            case 4:  return &SH2movblg<WHICH_SH2>;
            case 5:  return &SH2movwlg<WHICH_SH2>;
            case 6:  return &SH2movllg<WHICH_SH2>;
            case 7:  return &SH2mova<WHICH_SH2>;
            case 8:  return &SH2tsti<WHICH_SH2>;
            case 9:  return &SH2andi<WHICH_SH2>;
            case 10: return &SH2xori<WHICH_SH2>;
            case 11: return &SH2ori<WHICH_SH2>;
            case 12: return &SH2tstm<WHICH_SH2>;
            case 13: return &SH2andm<WHICH_SH2>;
            case 14: return &SH2xorm<WHICH_SH2>;
            case 15: return &SH2orm<WHICH_SH2>;
         }
   
      case 13: return &SH2movli<WHICH_SH2>;
      case 14: return &SH2movi<WHICH_SH2>;
      default: return &SH2undecoded<WHICH_SH2>;
   }
}

//////////////////////////////////////////////////////////////////////////////

TEMPLATE int _SH2InterpreterInit()
{
   int i;

   // Initialize any internal variables
   for(i = 0;i < 0x10000;i++)
      opcodes[WHICH_SH2][i] = decode<WHICH_SH2>(i);

   for (i = 0; i < 0x100; i++)
   {
      switch (i)
      {
         case 0x000: // Bios              
            fetchlist[i] = FetchBios;
            break;
         case 0x002: // Low Work Ram
            fetchlist[i] = FetchLWram;
            break;
         case 0x020: // CS0
            fetchlist[i] = FetchCs0;
            break;
         case 0x060: // High Work Ram
         case 0x061: 
         case 0x062: 
         case 0x063: 
         case 0x064: 
         case 0x065: 
         case 0x066: 
         case 0x067: 
         case 0x068: 
         case 0x069: 
         case 0x06A: 
         case 0x06B: 
         case 0x06C: 
         case 0x06D: 
         case 0x06E: 
         case 0x06F:
            fetchlist[i] = FetchHWram;
            break;
         default:
            fetchlist[i] = FetchInvalid;
            break;
      }
   }
   
   SH2ClearCodeBreakpoints(&MSH2);
   SH2ClearCodeBreakpoints(&SSH2);
   SH2ClearMemoryBreakpoints(&MSH2);
   SH2ClearMemoryBreakpoints(&SSH2);
   MSH2.breakpointEnabled = 0;
   SSH2.breakpointEnabled = 0;  
   
   return 0;
}

int SH2DebugInterpreterInit() {

  _SH2InterpreterInit<0>();
  _SH2InterpreterInit<1>();
  MSH2.breakpointEnabled = 1;
  SSH2.breakpointEnabled = 1;  
  return 0;
}

//////////////////////////////////////////////////////////////////////////////

void SH2InterpreterDeInit()
{
   // DeInitialize any internal variables here
}

//////////////////////////////////////////////////////////////////////////////

int SH2InterpreterReset()
{
   // Reset any internal variables here
   return 0;
}

//////////////////////////////////////////////////////////////////////////////

static INLINE void SH2UBCInterrupt(SH2_struct *context, u32 flag)
{
   if (15 > context->regs.SR.part.I) // Since UBC's interrupt are always level 15
   {
      context->regs.R[15] -= 4;
      MappedMemoryWriteLong(context->regs.R[15], context->regs.SR.all);
      context->regs.R[15] -= 4;
      MappedMemoryWriteLong(context->regs.R[15], context->regs.PC);
      context->regs.SR.part.I = 15;
      context->regs.PC = MappedMemoryReadLong(context->regs.VBR + (12 << 2));
      LOG("interrupt successfully handled\n");
   }
   context->onchip.BRCR |= flag;
}

//////////////////////////////////////////////////////////////////////////////

FASTCALL void SH2DebugInterpreterExec(SH2_struct *context, u32 cycles)
{
   
   while(context->cycles < cycles)
   {
#ifdef EMULATEUBC   	   
      int ubcinterrupt=0, ubcflag=0;
#endif
	
      SH2HandleBreakpoints(context);

#ifdef SH2_TRACE
      sh2_trace(context, context->regs.PC);
#endif

#ifdef EMULATEUBC
      if (context->onchip.BBRA & (BBR_CPA_CPU | BBR_IDA_INST | BBR_RWA_READ)) // Break on cpu, instruction, read cycles
      {
         if (context->onchip.BARA.all == (context->regs.PC & (~context->onchip.BAMRA.all)))
         {
            LOG("Trigger UBC A interrupt: PC = %08X\n", context->regs.PC);
            if (!(context->onchip.BRCR & BRCR_PCBA))
            {
               // Break before instruction fetch
	           SH2UBCInterrupt(context, BRCR_CMFCA);
            }
            else
            {
            	// Break after instruction fetch
               ubcinterrupt=1;
               ubcflag = BRCR_CMFCA;
            }
         }
      }
      else if(context->onchip.BBRB & (BBR_CPA_CPU | BBR_IDA_INST | BBR_RWA_READ)) // Break on cpu, instruction, read cycles
      {
         if (context->onchip.BARB.all == (context->regs.PC & (~context->onchip.BAMRB.all)))
         {
            LOG("Trigger UBC B interrupt: PC = %08X\n", context->regs.PC);
            if (!(context->onchip.BRCR & BRCR_PCBB))
            {
          	   // Break before instruction fetch
       	       SH2UBCInterrupt(context, BRCR_CMFCB);
            }
            else
            {
               // Break after instruction fetch
               ubcinterrupt=1;
               ubcflag = BRCR_CMFCB;
            }
         }
      }
#endif

      // Fetch Instruction
      context->instruction = fetchlist[(context->regs.PC >> 20) & 0x0FF](context->regs.PC);

      // Execute it
      opcodes[DECIDE_WHICH_SH2(context)][context->instruction]();

#ifdef EMULATEUBC
	  if (ubcinterrupt)
	     SH2UBCInterrupt(context, ubcflag);
#endif
   }

#ifdef SH2_TRACE
   SH2_TRACE_ADD_CYCLES(cycles);
#endif
}

//////////////////////////////////////////////////////////////////////////////

extern "C" {

FASTCALL void SH2InterpreterExec(SH2_struct *context, u32 cycles)
{
	if(context == &MSH2) _SH2InterpreterExec<0>(cycles);
	else _SH2InterpreterExec<1>(cycles);
}

int SH2InterpreterInit()
{
	_SH2InterpreterInit<0>();
	_SH2InterpreterInit<1>();
	return 0;
}

//////////////////////////////////////////////////////////////////////////////

} //extern "C"