/*  Copyright 2003-2006 Guillaume Duhamel
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

#ifndef VIDSHARED_H
#define VIDSHARED_H

#include "core.h"
#include "vdp2.h"
#include "debug.h"

typedef struct
{
   int vertices[8];
   int cellw, cellh;
   int flipfunction;
   int priority;

   int mapwh;
   int planew, planeh;
   int pagewh;
   int patternwh;
   int patterndatasize;
   int specialfunction;
   u32 addr, charaddr, paladdr;
   int colornumber;
   int isbitmap;
   u16 supplementdata;
   int auxmode;
   int enable;
   int x, y;
   int alpha;
   int coloroffset;
   int transparencyenable;
   int specialprimode;

   s32 cor;
   s32 cog;
   s32 cob;

   float coordincx, coordincy;
   FAST_FUNC_PTR(void,PlaneAddr)(void *, int);
   FAST_FUNC_PTR(u32,PostPixelFetchCalc)(void *, u32);
   int patternpixelwh;
   int draww;
   int drawh;
   int rotatenum;
   int rotatemode;
   int mosaicxmask;
   int mosaicymask;
   int islinescroll;
   u32 linescrolltbl;
   int wctl;
   int islinewindow;
   int isverticalscroll;
   u32 verticalscrolltbl;
   int verticalscrollinc;
} vdp2draw_struct;

typedef struct
{
   float Xst;
   float Yst;
   float Zst;
   float deltaXst;
   float deltaYst;
   float deltaX;
   float deltaY;
   float A;
   float B;
   float C;
   float D;
   float E;
   float F;
   float Px;
   float Py;
   float Pz;
   float Cx;
   float Cy;
   float Cz;
   float Mx;
   float My;
   float kx;
   float ky;
   float KAst;
   float deltaKAst;
   float deltaKAx;
   u32 coeftbladdr;
   int coefenab;
   int coefmode;
   int coefdatasize;
   float Xp;
   float Yp;
   float dX;
   float dY;
   int screenover;
   int msb;
} vdp2rotationparameter_struct;

#define FP_SIZE 16
typedef s32 fixed32;

typedef struct
{
   fixed32 Xst;
   fixed32 Yst;
   fixed32 Zst;
   fixed32 deltaXst;
   fixed32 deltaYst;
   fixed32 deltaX;
   fixed32 deltaY;
   fixed32 A;
   fixed32 B;
   fixed32 C;
   fixed32 D;
   fixed32 E;
   fixed32 F;
   fixed32 Px;
   fixed32 Py;
   fixed32 Pz;
   fixed32 Cx;
   fixed32 Cy;
   fixed32 Cz;
   fixed32 Mx;
   fixed32 My;
   fixed32 kx;
   fixed32 ky;
   fixed32 KAst;
   fixed32 deltaKAst;
   fixed32 deltaKAx;
   u32 coeftbladdr;
   int coefenab;
   int coefmode;
   int coefdatasize;
   fixed32 Xp;
   fixed32 Yp;
   fixed32 dX;
   fixed32 dY;
   int screenover;
   int msb;
} vdp2rotationparameterfp_struct;

typedef struct
{
   int xstart, ystart;
   int xend, yend;
} clipping_struct;

#define tofixed(v) ((v) * (1 << FP_SIZE))
#define toint(v) ((v) >> FP_SIZE)
#define touint(v) ((u16)((v) >> FP_SIZE))
#define tofloat(v) ((float)(v) / (float)(1 << FP_SIZE))
#define mulfixed(a,b) ((fixed32)((s64)(a) * (s64)(b) >> FP_SIZE))
#define divfixed(a,b) (((s64)(a) << FP_SIZE) / (b))

void FASTCALL Vdp2NBG0PlaneAddr(vdp2draw_struct *info, int i);
void FASTCALL Vdp2NBG1PlaneAddr(vdp2draw_struct *info, int i);
void FASTCALL Vdp2NBG2PlaneAddr(vdp2draw_struct *info, int i);
void FASTCALL Vdp2NBG3PlaneAddr(vdp2draw_struct *info, int i);
void Vdp2ReadRotationTable(int which, vdp2rotationparameter_struct *parameter);
void Vdp2ReadRotationTableFP(int which, vdp2rotationparameterfp_struct *parameter);
void FASTCALL Vdp2ParameterAPlaneAddr(vdp2draw_struct *info, int i);
void FASTCALL Vdp2ParameterBPlaneAddr(vdp2draw_struct *info, int i);
float Vdp2ReadCoefficientMode0_2(vdp2rotationparameter_struct *parameter, u32 addr);
fixed32 Vdp2ReadCoefficientMode0_2FP(vdp2rotationparameterfp_struct *parameter, u32 addr);

void VideoInitGlut();

//////////////////////////////////////////////////////////////////////////////

static INLINE int GenerateRotatedXPos(vdp2rotationparameter_struct *p, int x, int y)
{
   float Xsp = p->A * ((p->Xst + p->deltaXst * y) - p->Px) +
               p->B * ((p->Yst + p->deltaYst * y) - p->Py) +
               p->C * (p->Zst - p->Pz);

   return (int)(p->kx * (Xsp + p->dX * (float)x) + p->Xp);
}

//////////////////////////////////////////////////////////////////////////////

static INLINE void GenerateRotatedVarFP(vdp2rotationparameterfp_struct *p, fixed32 *xmul, fixed32 *ymul, fixed32 *C, fixed32 *F)
{
   *xmul = p->Xst - p->Px;
   *ymul = p->Yst - p->Py;
   *C = mulfixed(p->C, (p->Zst - p->Pz));
   *F = mulfixed(p->F, (p->Zst - p->Pz));
}

//////////////////////////////////////////////////////////////////////////////

static INLINE int GenerateRotatedYPos(vdp2rotationparameter_struct *p, int x, int y)
{
   float Ysp = p->D * ((p->Xst + p->deltaXst * y) - p->Px) +
               p->E * ((p->Yst + p->deltaYst * y) - p->Py) +
               p->F * (p->Zst - p->Pz);

   return (int)(p->ky * (Ysp + p->dY * (float)x) + p->Yp);
}

//////////////////////////////////////////////////////////////////////////////

static INLINE int GenerateRotatedXPosFP(vdp2rotationparameterfp_struct *p, int x, fixed32 xmul, fixed32 ymul, fixed32 C)
{
   fixed32 Xsp = mulfixed(p->A, xmul) + mulfixed(p->B, ymul) + C;

   return touint(mulfixed(p->kx, (Xsp + mulfixed(p->dX, tofixed(x)))) + p->Xp);
}

//////////////////////////////////////////////////////////////////////////////

static INLINE int GenerateRotatedYPosFP(vdp2rotationparameterfp_struct *p, int x, fixed32 xmul, fixed32 ymul, fixed32 F)
{
   fixed32 Ysp = mulfixed(p->D, xmul) + mulfixed(p->E, ymul) + F;

   return touint(mulfixed(p->ky, (Ysp + mulfixed(p->dY, tofixed(x)))) + p->Yp);
}

//////////////////////////////////////////////////////////////////////////////

static INLINE void CalculateRotationValues(vdp2rotationparameter_struct *p)
{
   p->Xp=p->A * (p->Px - p->Cx) +
         p->B * (p->Py - p->Cy) +
         p->C * (p->Pz - p->Cz) +
         p->Cx + p->Mx;
   p->Yp=p->D * (p->Px - p->Cx) +
         p->E * (p->Py - p->Cy) +
         p->F * (p->Pz - p->Cz) +
         p->Cy + p->My;
   p->dX=p->A * p->deltaX +
         p->B * p->deltaY;
   p->dY=p->D * p->deltaX +
         p->E * p->deltaY;
}

//////////////////////////////////////////////////////////////////////////////

static INLINE void CalculateRotationValuesFP(vdp2rotationparameterfp_struct *p)
{
   p->Xp=mulfixed(p->A, (p->Px - p->Cx)) +
         mulfixed(p->B, (p->Py - p->Cy)) +
         mulfixed(p->C, (p->Pz - p->Cz)) +
         p->Cx + p->Mx;
   p->Yp=mulfixed(p->D, (p->Px - p->Cx)) +
         mulfixed(p->E, (p->Py - p->Cy)) +
         mulfixed(p->F, (p->Pz - p->Cz)) +
         p->Cy + p->My;
   p->dX=mulfixed(p->A, p->deltaX) +
         mulfixed(p->B, p->deltaY);
   p->dY=mulfixed(p->D, p->deltaX) +
         mulfixed(p->E, p->deltaY);
}

//////////////////////////////////////////////////////////////////////////////

static INLINE void CalcPlaneAddr(vdp2draw_struct *info, u32 tmp)
{
   int deca = info->planeh + info->planew - 2;
   int multi = info->planeh * info->planew;
     
   //if (Vdp2Regs->VRSIZE & 0x8000)
   //{
      if (info->patterndatasize == 1)
      {
         if (info->patternwh == 1)
            info->addr = ((tmp & 0x3F) >> deca) * (multi * 0x2000);
         else
            info->addr = (tmp >> deca) * (multi * 0x800);
      }
      else
      {
         if (info->patternwh == 1)
            info->addr = ((tmp & 0x1F) >> deca) * (multi * 0x4000);
         else
            info->addr = ((tmp & 0x7F) >> deca) * (multi * 0x1000);
      }
   /*}
   else
   {
      if (info->patterndatasize == 1)
      {
         if (info->patternwh == 1)
            info->addr = ((tmp & 0x1F) >> deca) * (multi * 0x2000);
         else
            info->addr = ((tmp & 0x7F) >> deca) * (multi * 0x800);
      }
      else
      {
         if (info->patternwh == 1)
            info->addr = ((tmp & 0xF) >> deca) * (multi * 0x4000);
         else
            info->addr = ((tmp & 0x3F) >> deca) * (multi * 0x1000);
      }
   }*/
}

//////////////////////////////////////////////////////////////////////////////

static INLINE void ReadBitmapSize(vdp2draw_struct *info, u16 bm, int mask)
{
   switch(bm & mask)
   {
      case 0: info->cellw = 512;
              info->cellh = 256;
              break;
      case 1: info->cellw = 512;
              info->cellh = 512;
              break;
      case 2: info->cellw = 1024;
              info->cellh = 256;
              break;
      case 3: info->cellw = 1024;
              info->cellh = 512;
              break;
   }
}

//////////////////////////////////////////////////////////////////////////////

static INLINE void ReadPlaneSize(vdp2draw_struct *info, u16 reg)
{
   switch(reg & 0x3)
   {
      case 0:
         info->planew = info->planeh = 1;
         break;
      case 1:
         info->planew = 2;
         info->planeh = 1;
         break;
      case 3:
         info->planew = info->planeh = 2;
         break;
      default: // Not sure what 0x2 does, though a few games seem to use it
         info->planew = info->planeh = 1;
         break;
   }
}

//////////////////////////////////////////////////////////////////////////////

static INLINE void ReadPatternData(vdp2draw_struct *info, u16 pnc, int chctlwh)
{
   if(pnc & 0x8000)
      info->patterndatasize = 1;
   else
      info->patterndatasize = 2;

   if(chctlwh)
      info->patternwh = 2;
   else
      info->patternwh = 1;

   info->pagewh = 64/info->patternwh;
   info->cellw = info->cellh = 8;
   info->supplementdata = pnc & 0x3FF;
   info->auxmode = (pnc & 0x4000) >> 14;
}

//////////////////////////////////////////////////////////////////////////////

static INLINE void ReadMosaicData(vdp2draw_struct *info, u16 mask)
{
   if (Vdp2Regs->MZCTL & mask)
   {  
      info->mosaicxmask = ((Vdp2Regs->MZCTL >> 8) & 0xF) + 1;
      info->mosaicymask = (Vdp2Regs->MZCTL >> 12) + 1;
   }
   else
   {
      info->mosaicxmask = 1;
      info->mosaicymask = 1;
   }
}

//////////////////////////////////////////////////////////////////////////////

static INLINE void ReadLineScrollData(vdp2draw_struct *info, u16 mask, u32 tbl)
{
   if (mask & 0xE)
   {
      info->islinescroll = (mask >> 1) & 0x7;
      info->linescrolltbl = (tbl & 0x7FFFE) << 1;
   }
   else
      info->islinescroll = 0;
}

//////////////////////////////////////////////////////////////////////////////

static INLINE void ReadWindowData(int wctl, clipping_struct *clip)
{
   if (wctl & 0x2)
   {
      clip[0].xstart = Vdp2Regs->WPSX0 >> 1; // fix me
      clip[0].ystart = Vdp2Regs->WPSY0;
      clip[0].xend = Vdp2Regs->WPEX0 >> 1; // fix me
      clip[0].yend = Vdp2Regs->WPEY0;
   }

   if (wctl & 0x8)
   {
      clip[1].xstart = Vdp2Regs->WPSX1 >> 1; // fix me
      clip[1].ystart = Vdp2Regs->WPSY1;
      clip[1].xend = Vdp2Regs->WPEX1 >> 1; // fix me
      clip[1].yend = Vdp2Regs->WPEY1;
   }

   if (wctl & 0x20)
   {
      // fix me
   }
}

//////////////////////////////////////////////////////////////////////////////

static INLINE void ReadLineWindowData(int *islinewindow, int wctl, u32 *linewnd0addr, u32 *linewnd1addr)
{
   islinewindow[0] = 0;

   if (wctl & 0x2 && Vdp2Regs->LWTA0.all & 0x80000000)
   {
      islinewindow[0] |= 0x1;
      linewnd0addr[0] = (Vdp2Regs->LWTA0.all & 0x7FFFE) << 1;
   }
   if (wctl & 0x8 && Vdp2Regs->LWTA1.all & 0x80000000)
   {
      islinewindow[0] |= 0x2;
      linewnd1addr[0] = (Vdp2Regs->LWTA1.all & 0x7FFFE) << 1;
   }
}

//////////////////////////////////////////////////////////////////////////////

static INLINE void ReadLineWindowClip(int islinewindow, clipping_struct *clip, u32 *linewnd0addr, u32 *linewnd1addr)
{
   if (islinewindow)
   {
      // Fetch new xstart and xend values from table
      if (islinewindow & 0x1)
      {
         // Window 0
         clip[0].xstart = (T1ReadWord(Vdp2Ram, linewnd0addr[0]) & 0x3FF) >> 1; // fix me
         linewnd0addr[0]+=2;
         clip[0].xend = (T1ReadWord(Vdp2Ram, linewnd0addr[0]) & 0x3FF) >> 1; // fix me
         linewnd0addr[0]+=2;
      }
      if (islinewindow & 0x2)
      {
         // Window 1
         clip[1].xstart = (T1ReadWord(Vdp2Ram, linewnd1addr[0]) & 0x3FF) >> 1; // fix me
         linewnd1addr[0]+=2;
         clip[1].xend = (T1ReadWord(Vdp2Ram, linewnd1addr[0]) & 0x3FF) >> 1; // fix me
         linewnd1addr[0]+=2;
      }
   }
}

//////////////////////////////////////////////////////////////////////////////

static INLINE int IsScreenRotated(vdp2rotationparameter_struct *parameter)
{
  return (parameter->deltaXst == 0.0 &&
          parameter->deltaYst == 1.0 &&
          parameter->deltaX == 1.0 &&
          parameter->deltaY == 0.0 &&
          parameter->A == 1.0 &&
          parameter->B == 0.0 &&
          parameter->C == 0.0 &&
          parameter->D == 0.0 &&
          parameter->E == 1.0 &&
          parameter->F == 0.0);
}

//////////////////////////////////////////////////////////////////////////////

static INLINE int IsScreenRotatedFP(vdp2rotationparameterfp_struct *parameter)
{
  return (parameter->deltaXst == tofixed(0.0) &&
          parameter->deltaYst == tofixed(1.0) &&
          parameter->deltaX == tofixed(1.0) &&
          parameter->deltaY == tofixed(0.0) &&
          parameter->A == tofixed(1.0) &&
          parameter->B == tofixed(0.0) &&
          parameter->C == tofixed(0.0) &&
          parameter->D == tofixed(0.0) &&
          parameter->E == tofixed(1.0) &&
          parameter->F == tofixed(0.0));
}

//////////////////////////////////////////////////////////////////////////////

static INLINE void Vdp2ReadCoefficient(vdp2rotationparameter_struct *parameter, u32 addr)
{
   switch (parameter->coefmode)
   {
      case 0: // coefficient for kx and ky
         parameter->kx = parameter->ky = Vdp2ReadCoefficientMode0_2(parameter, addr);
         break;
      case 1: // coefficient for kx
         parameter->kx = Vdp2ReadCoefficientMode0_2(parameter, addr);
         break;
      case 2: // coefficient for ky
         parameter->ky = Vdp2ReadCoefficientMode0_2(parameter, addr);
         break;
      case 3: // coefficient for Xp
      {
         s32 i;

         if (parameter->coefdatasize == 2)
         {
            i = T1ReadWord(Vdp2Ram, addr);
            parameter->msb = (i >> 15) & 0x1;
            parameter->Xp = (float) (signed) ((i & 0x7FFF) | (i & 0x4000 ? 0xFFFFC000 : 0x00000000)) / 4;
         }
         else
         {
            i = T1ReadLong(Vdp2Ram, addr);
            parameter->msb = (i >> 31) & 0x1;
            parameter->Xp = (float) (signed) ((i & 0x007FFFFF) | (i & 0x00800000 ? 0xFF800000 : 0x00000000)) / 256;
         }
         break;
      }
   }
}

//////////////////////////////////////////////////////////////////////////////

static INLINE void Vdp2ReadCoefficientFP(vdp2rotationparameterfp_struct *parameter, u32 addr)
{
   switch (parameter->coefmode)
   {
      case 0: // coefficient for kx and ky
         parameter->kx = parameter->ky = Vdp2ReadCoefficientMode0_2FP(parameter, addr);
         break;
      case 1: // coefficient for kx
         parameter->kx = Vdp2ReadCoefficientMode0_2FP(parameter, addr);
         break;
      case 2: // coefficient for ky
         parameter->ky = Vdp2ReadCoefficientMode0_2FP(parameter, addr);
         break;
      case 3: // coefficient for Xp
      {
         s32 i;

         if (parameter->coefdatasize == 2)
         {
            i = T1ReadWord(Vdp2Ram, addr);
            parameter->msb = (i >> 15) & 0x1;
            parameter->Xp = (signed) ((i & 0x7FFF) | (i & 0x4000 ? 0xFFFFC000 : 0x00000000)) * 16384;
         }
         else
         {
            i = T1ReadLong(Vdp2Ram, addr);
            parameter->msb = (i >> 31) & 0x1;
            parameter->Xp = (signed) ((i & 0x007FFFFF) | (i & 0x00800000 ? 0xFF800000 : 0x00000000)) * 256;
         }

         break;
      }
   }
}

//////////////////////////////////////////////////////////////////////////////

static INLINE void Vdp1ProcessSpritePixel(int type, u16 *pixel, int *shadow, int *priority, int *colorcalc)
{
   switch(type)
   {
      case 0x0:
      {
         // Type 0(2-bit priority, 3-bit color calculation, 11-bit color data)
         *priority = *pixel >> 14;
         *colorcalc = (*pixel >> 11) & 0x7;
         *pixel = *pixel & 0x7FF;
         break;
      }
      case 0x1:
      {
         // Type 1(3-bit priority, 2-bit color calculation, 11-bit color data)
         *priority = *pixel >> 13;
         *colorcalc = (*pixel >> 11) & 0x3;
         *pixel &= 0x7FF;
         break;
      }
      case 0x2:
      {
         // Type 2(1-bit shadow, 1-bit priority, 3-bit color calculation, 11-bit color data)
         *shadow = *pixel >> 15;
         *priority = (*pixel >> 14) & 0x1;
         *colorcalc = (*pixel >> 11) & 0x7;
         *pixel &= 0x7FF;
         break;
      }
      case 0x3:
      {
         // Type 3(1-bit shadow, 2-bit priority, 2-bit color calculation, 11-bit color data)
         *shadow = *pixel >> 15;
         *priority = (*pixel >> 13) & 0x3;
         *colorcalc = (*pixel >> 11) & 0x3;
         *pixel &= 0x7FF;
         break;
      }
      case 0x4:
      {
         // Type 4(1-bit shadow, 2-bit priority, 3-bit color calculation, 10-bit color data)
         *shadow = *pixel >> 15;
         *priority = (*pixel >> 13) & 0x3;
         *colorcalc = (*pixel >> 10) & 0x7;
         *pixel &= 0x3FF;
         break;
      }
      case 0x5:
      {
         // Type 5(1-bit shadow, 3-bit priority, 1-bit color calculation, 11-bit color data)
         *shadow = *pixel >> 15;
         *priority = (*pixel >> 12) & 0x7;
         *colorcalc = (*pixel >> 11) & 0x1;
         *pixel &= 0x7FF;
         break;
      }
      case 0x6:
      {
         // Type 6(1-bit shadow, 3-bit priority, 2-bit color calculation, 10-bit color data)
         *shadow = *pixel >> 15;
         *priority = (*pixel >> 12) & 0x7;
         *colorcalc = (*pixel >> 10) & 0x3;
         *pixel &= 0x3FF;
         break;
      }
      case 0x7:
      {
         // Type 7(1-bit shadow, 3-bit priority, 3-bit color calculation, 9-bit color data)
         *shadow = *pixel >> 15;
         *priority = (*pixel >> 12) & 0x7;
         *colorcalc = (*pixel >> 9) & 0x7;
         *pixel &= 0x1FF;
         break;
      }
      case 0x8:
      {
         // Type 8(1-bit priority, 7-bit color data)
         *priority = *pixel >> 7;
         *pixel &= 0x7F;
         break;
      }
      case 0x9:
      {
         // Type 9(1-bit priority, 1-bit color calculation, 6-bit color data)
         *priority = *pixel >> 7;
         *colorcalc = (*pixel >> 6) & 0x1;
         *pixel &= 0x3F;
         break;
      }
      case 0xA:
      {
         // Type A(2-bit priority, 6-bit color data)
         *priority = *pixel >> 6;
         *pixel &= 0x3F;
         break;
      }
      case 0xB:
      {
         // Type B(2-bit color calculation, 6-bit color data)
         *colorcalc = *pixel >> 6;
         *pixel &= 0x3F;
         break;
      }
      case 0xC:
      {
         // Type C(1-bit special priority, 8-bit color data - bit 7 is shared)
         *priority = *pixel >> 7;
         break;
      }
      case 0xD:
      {
         // Type D(1-bit special priority, 1-bit special color calculation, 8-bit color data - bits 6 and 7 are shared)
         *priority = *pixel >> 7;
         *colorcalc = (*pixel >> 6) & 0x1;
         break;
      }
      case 0xE:
      {
         // Type E(2-bit special priority, 8-bit color data - bits 6 and 7 are shared)
         *priority = *pixel >> 6;
         break;
      }
      case 0xF:
      {
         // Type F(2-bit special color calculation, 8-bit color data - bits 6 and 7 are shared)
         *colorcalc = *pixel >> 6;
         break;
      }
      default: break;
   }
}

//////////////////////////////////////////////////////////////////////////////

#endif
