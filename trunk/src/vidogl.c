/*  Copyright 2003-2006 Guillaume Duhamel
    Copyright 2004 Lawrence Sebald
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

#ifdef HAVE_LIBGL

#include "vidogl.h"
#include "vidshared.h"
#include "debug.h"
#include "vdp2.h"
#include "yabause.h"
#include "ygl.h"
#include "yui.h"

#ifdef USEMICSHADERS
#ifdef WIN32
#include <windows.h>
#include <wingdi.h>
#elif HAVE_GLXGETPROCADDRESS
#include <GL/glx.h>
#endif
#endif

#if defined WORDS_BIGENDIAN
#define SAT2YAB1(alpha,temp)		(alpha | (temp & 0x7C00) << 1 | (temp & 0x3E0) << 14 | (temp & 0x1F) << 27)
#else
#define SAT2YAB1(alpha,temp)		(alpha << 24 | (temp & 0x1F) << 3 | (temp & 0x3E0) << 6 | (temp & 0x7C00) << 9)
#endif

#if defined WORDS_BIGENDIAN
#define SAT2YAB2(alpha,dot1,dot2)       ((dot2 & 0xFF << 24) | ((dot2 & 0xFF00) << 8) | ((dot1 & 0xFF) << 8) | alpha)
#else
#define SAT2YAB2(alpha,dot1,dot2)       (alpha << 24 | ((dot1 & 0xFF) << 16) | (dot2 & 0xFF00) | (dot2 & 0xFF))
#endif

#define COLOR_ADDt(b)		(b>0xFF?0xFF:(b<0?0:b))
#define COLOR_ADDb(b1,b2)	COLOR_ADDt((signed) (b1) + (b2))
#ifdef WORDS_BIGENDIAN
#define COLOR_ADD(l,r,g,b)	(COLOR_ADDb((l >> 24) & 0xFF, r) << 24) | \
				(COLOR_ADDb((l >> 16) & 0xFF, g) << 16) | \
				(COLOR_ADDb((l >> 8) & 0xFF, b) << 8) | \
				(l & 0xFF)
#else
#define COLOR_ADD(l,r,g,b)	COLOR_ADDb((l & 0xFF), r) | \
				(COLOR_ADDb((l >> 8 ) & 0xFF, g) << 8) | \
				(COLOR_ADDb((l >> 16 ) & 0xFF, b) << 16) | \
				(l & 0xFF000000)
#endif

int VIDOGLInit(void);
void VIDOGLDeInit(void);
void VIDOGLResize(unsigned int, unsigned int, int);
int VIDOGLIsFullscreen(void);
int VIDOGLVdp1Reset(void);
void VIDOGLVdp1DrawStart(void);
void VIDOGLVdp1DrawEnd(void);
void VIDOGLVdp1NormalSpriteDraw(void);
void VIDOGLVdp1ScaledSpriteDraw(void);
void VIDOGLVdp1DistortedSpriteDraw(void);
void VIDOGLVdp1PolygonDraw(void);
void VIDOGLVdp1PolylineDraw(void);
void VIDOGLVdp1LineDraw(void);
void VIDOGLVdp1UserClipping(void);
void VIDOGLVdp1SystemClipping(void);
void VIDOGLVdp1LocalCoordinate(void);
int VIDOGLVdp2Reset(void);
void VIDOGLVdp2DrawStart(void);
void VIDOGLVdp2DrawEnd(void);
void VIDOGLVdp2DrawScreens(void);
void VIDOGLVdp2SetResolution(u16 TVMD);
void FASTCALL VIDOGLVdp2SetPriorityNBG0(int priority);
void FASTCALL VIDOGLVdp2SetPriorityNBG1(int priority);
void FASTCALL VIDOGLVdp2SetPriorityNBG2(int priority);
void FASTCALL VIDOGLVdp2SetPriorityNBG3(int priority);
void FASTCALL VIDOGLVdp2SetPriorityRBG0(int priority);
void YglGetGlSize(int *width, int *height);

VideoInterface_struct VIDOGL = {
VIDCORE_OGL,
"OpenGL Video Interface",
VIDOGLInit,
VIDOGLDeInit,
VIDOGLResize,
VIDOGLIsFullscreen,
VIDOGLVdp1Reset,
VIDOGLVdp1DrawStart,
VIDOGLVdp1DrawEnd,
VIDOGLVdp1NormalSpriteDraw,
VIDOGLVdp1ScaledSpriteDraw,
VIDOGLVdp1DistortedSpriteDraw,
VIDOGLVdp1PolygonDraw,
VIDOGLVdp1PolylineDraw,
VIDOGLVdp1LineDraw,
VIDOGLVdp1UserClipping,
VIDOGLVdp1SystemClipping,
VIDOGLVdp1LocalCoordinate,
VIDOGLVdp2Reset,
VIDOGLVdp2DrawStart,
VIDOGLVdp2DrawEnd,
VIDOGLVdp2DrawScreens,
VIDOGLVdp2SetResolution,
VIDOGLVdp2SetPriorityNBG0,
VIDOGLVdp2SetPriorityNBG1,
VIDOGLVdp2SetPriorityNBG2,
VIDOGLVdp2SetPriorityNBG3,
VIDOGLVdp2SetPriorityRBG0,
YglOnScreenDebugMessage,
YglGetGlSize
};

static float vdp1wratio=1;
static float vdp1hratio=1;
static int vdp1cor=0;
static int vdp1cog=0;
static int vdp1cob=0;

static int vdp2width;
static int vdp2height;
static int nbg0priority=0;
static int nbg1priority=0;
static int nbg2priority=0;
static int nbg3priority=0;
static int rbg0priority=0;

static u32 Vdp2ColorRamGetColor(u32 colorindex, int alpha);

static int GlHeight=320;
static int GlWidth=224;

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL Vdp1ReadTexture(vdp1cmd_struct *cmd, YglSprite *sprite, YglTexture *texture)
{
   u32 charAddr = cmd->CMDSRCA * 8;
   u32 dot;
   u8 SPD = ((cmd->CMDPMOD & 0x40) != 0);
   u32 alpha = 0xFF;
   VDP1LOG("Making new sprite %08X\n", charAddr);

   switch(cmd->CMDPMOD & 0x7)
   {
      case 0:
         alpha = 0xFF;
         break;
      case 3:
         alpha = 0x80;
         break;
      default:
         VDP1LOG("unimplemented color calculation: %X\n", (cmd->CMDPMOD & 0x7));
         break;
   }

   switch((cmd->CMDPMOD >> 3) & 0x7)
   {
      case 0:
      {
         // 4 bpp Bank mode
         u32 colorBank = cmd->CMDCOLR;
         u32 colorOffset = (Vdp2Regs->CRAOFB & 0x70) << 4;
         u16 i;

         for(i = 0;i < sprite->h;i++)
         {
            u16 j;
            j = 0;
            while(j < sprite->w)
            {
               dot = T1ReadByte(Vdp1Ram, charAddr & 0x7FFFF);

               // Pixel 1
               if (((dot >> 4) == 0) && !SPD) *texture->textdata++ = COLOR_ADD(0, vdp1cor, vdp1cog, vdp1cob);
               else *texture->textdata++ = COLOR_ADD(Vdp2ColorRamGetColor(((dot >> 4) | colorBank) + colorOffset, alpha), vdp1cor, vdp1cog, vdp1cob);
               j += 1;

               // Pixel 2
               if (((dot & 0xF) == 0) && !SPD) *texture->textdata++ = COLOR_ADD(0, vdp1cor, vdp1cog, vdp1cob);
               else *texture->textdata++ = COLOR_ADD(Vdp2ColorRamGetColor(((dot & 0xF) | colorBank) + colorOffset, alpha), vdp1cor, vdp1cog, vdp1cob);
               j += 1;

               charAddr += 1;
            }
            texture->textdata += texture->w;
         }
         break;
      }
      case 1:
      {
         // 4 bpp LUT mode
         u32 temp;
         u32 colorLut = cmd->CMDCOLR * 8;
         u16 i;

         for(i = 0;i < sprite->h;i++)
         {
            u16 j;
            j = 0;
            while(j < sprite->w)
            {
               dot = T1ReadByte(Vdp1Ram, charAddr & 0x7FFFF);

               if (((dot >> 4) == 0) && !SPD)
                  *texture->textdata++ = COLOR_ADD(0, vdp1cor, vdp1cog, vdp1cob);
               else
               {
                  temp = T1ReadWord(Vdp1Ram, ((dot >> 4) * 2 + colorLut) & 0x7FFFF);
                  if (temp & 0x8000)
                     *texture->textdata++ = COLOR_ADD(SAT2YAB1(alpha, temp), vdp1cor, vdp1cog, vdp1cob);
                  else
                     *texture->textdata++ = COLOR_ADD(Vdp2ColorRamGetColor(temp, alpha), vdp1cor, vdp1cog, vdp1cob);
               }

               j += 1;

               if (((dot & 0xF) == 0) && !SPD)
                  *texture->textdata++ = COLOR_ADD(0, vdp1cor, vdp1cog, vdp1cob);
               else
               {
                  temp = T1ReadWord(Vdp1Ram, ((dot & 0xF) * 2 + colorLut) & 0x7FFFF);
                  if (temp & 0x8000)
                     *texture->textdata++ = COLOR_ADD(SAT2YAB1(alpha, temp), vdp1cor, vdp1cog, vdp1cob);
                  else
                     *texture->textdata++ = COLOR_ADD(Vdp2ColorRamGetColor(temp, alpha), vdp1cor, vdp1cog, vdp1cob);
               }

               j += 1;

               charAddr += 1;
            }
            texture->textdata += texture->w;
         }
         break;
      }
      case 2:
      {
         // 8 bpp(64 color) Bank mode
         u32 colorBank = cmd->CMDCOLR;
         u32 colorOffset = (Vdp2Regs->CRAOFB & 0x70) << 4;

         u16 i, j;

         for(i = 0;i < sprite->h;i++)
         {
            for(j = 0;j < sprite->w;j++)
            {
               dot = T1ReadByte(Vdp1Ram, charAddr & 0x7FFFF) & 0x3F;
               charAddr++;

               if ((dot == 0) && !SPD) *texture->textdata++ = COLOR_ADD(0, vdp1cor, vdp1cog, vdp1cob);
               else *texture->textdata++ = COLOR_ADD(Vdp2ColorRamGetColor((dot | colorBank) + colorOffset, alpha), vdp1cor, vdp1cog, vdp1cob);
            }
            texture->textdata += texture->w;
         }
         break;
      }
      case 3:
      {
         // 8 bpp(128 color) Bank mode
         u32 colorBank = cmd->CMDCOLR;
         u32 colorOffset = (Vdp2Regs->CRAOFB & 0x70) << 4;
         u16 i, j;

         for(i = 0;i < sprite->h;i++)
         {
            for(j = 0;j < sprite->w;j++)
            {
               dot = T1ReadByte(Vdp1Ram, charAddr & 0x7FFFF) & 0x7F;
               charAddr++;

               if ((dot == 0) && !SPD) *texture->textdata++ = COLOR_ADD(0, vdp1cor, vdp1cog, vdp1cob);
               else *texture->textdata++ = COLOR_ADD(Vdp2ColorRamGetColor((dot | colorBank) + colorOffset, alpha), vdp1cor, vdp1cog, vdp1cob);
            }
            texture->textdata += texture->w;
         }
         break;
      }
      case 4:
      {
         // 8 bpp(256 color) Bank mode
         u32 colorBank = cmd->CMDCOLR;
         u32 colorOffset = (Vdp2Regs->CRAOFB & 0x70) << 4;
         u16 i, j;

         for(i = 0;i < sprite->h;i++)
         {
            for(j = 0;j < sprite->w;j++)
            {
               dot = T1ReadByte(Vdp1Ram, charAddr & 0x7FFFF);
               charAddr++;

               if ((dot == 0) && !SPD) *texture->textdata++ = COLOR_ADD(0, vdp1cor, vdp1cog, vdp1cob);
               else *texture->textdata++ = COLOR_ADD(Vdp2ColorRamGetColor((dot | colorBank) + colorOffset, alpha), vdp1cor, vdp1cog, vdp1cob);
            }
            texture->textdata += texture->w;
         }
         break;
      }
      case 5:
      {
         // 16 bpp Bank mode
         u16 i, j;

         for(i = 0;i < sprite->h;i++)
         {
            for(j = 0;j < sprite->w;j++)
            {
               dot = T1ReadWord(Vdp1Ram, charAddr & 0x7FFFF);
               charAddr += 2;

               //if (!(dot & 0x8000) && (Vdp2Regs->SPCTL & 0x20)) printf("mixed mode\n");
               if (!(dot & 0x8000) && !SPD) *texture->textdata++ = COLOR_ADD(0, vdp1cor, vdp1cog, vdp1cob);
               else *texture->textdata++ = COLOR_ADD(SAT2YAB1(alpha, dot), vdp1cor, vdp1cog, vdp1cob);
            }
            texture->textdata += texture->w;
         }
         break;
      }
      default:
         VDP1LOG("Unimplemented sprite color mode: %X\n", (cmd->CMDPMOD >> 3) & 0x7);
         break;
   }
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL Vdp1ReadPriority(vdp1cmd_struct *cmd, YglSprite *sprite)
{
   u8 SPCLMD = Vdp2Regs->SPCTL;
   u8 sprite_register;
   u8 *sprprilist = (u8 *)&Vdp2Regs->PRISA;
   u16 lutPri;
   u16 *reg_src = &cmd->CMDCOLR;
   int not_lut = 1;

   // is the sprite is RGB or LUT (in fact, LUT can use bank color, we just hope it won't...)
   if ((SPCLMD & 0x20) && (cmd->CMDCOLR & 0x8000))
   {
      // RGB data, use register 0
      sprite->priority = Vdp2Regs->PRISA & 0x7;
      return;
   }

   if (((cmd->CMDPMOD >> 3) & 0x7) == 1) {
      u32 charAddr, dot, colorLut;

      sprite->priority = Vdp2Regs->PRISA & 0x7;

      charAddr = cmd->CMDSRCA * 8;
      dot = T1ReadByte(Vdp1Ram, charAddr & 0x7FFFF);
      colorLut = cmd->CMDCOLR * 8;
      lutPri = T1ReadWord(Vdp1Ram, (dot >> 4) * 2 + colorLut);
      if (!(lutPri & 0x8000)) {
         not_lut = 0;
         reg_src = &lutPri;
      } else
         return;
   }

   {
      u8 sprite_type = SPCLMD & 0xF;
      switch(sprite_type)
      {
         case 0:
            sprite_register = (*reg_src & 0xC000) >> 14;
#ifdef WORDS_BIGENDIAN
            sprite->priority = sprprilist[sprite_register^1] & 0x7;
#else
            sprite->priority = sprprilist[sprite_register] & 0x7;
#endif
            if (not_lut) cmd->CMDCOLR &= 0x7FF;
            break;
         case 1:
            sprite_register = (*reg_src & 0xE000) >> 13;
#ifdef WORDS_BIGENDIAN
            sprite->priority = sprprilist[sprite_register^1] & 0x7;
#else
            sprite->priority = sprprilist[sprite_register] & 0x7;
#endif
            if (not_lut) cmd->CMDCOLR &= 0x7FF;
            break;
         case 2:
            sprite_register = (*reg_src >> 14) & 0x1;
#ifdef WORDS_BIGENDIAN
            sprite->priority = sprprilist[sprite_register^1] & 0x7;
#else
            sprite->priority = sprprilist[sprite_register] & 0x7;
#endif
            if (not_lut) cmd->CMDCOLR &= 0x7FF;
            break;
         case 3:
            sprite_register = (*reg_src & 0x6000) >> 13;
#ifdef WORDS_BIGENDIAN
            sprite->priority = sprprilist[sprite_register^1] & 0x7;
#else
            sprite->priority = sprprilist[sprite_register] & 0x7;
#endif
            if (not_lut) cmd->CMDCOLR &= 0x7FF;
            break;
         case 4:
            sprite_register = (*reg_src & 0x6000) >> 13;
#ifdef WORDS_BIGENDIAN
            sprite->priority = sprprilist[sprite_register^1] & 0x7;
#else
            sprite->priority = sprprilist[sprite_register] & 0x7;
#endif
            if (not_lut) cmd->CMDCOLR &= 0x3FF;
            break;
         case 5:
            sprite_register = (*reg_src & 0x7000) >> 12;
#ifdef WORDS_BIGENDIAN
            sprite->priority = sprprilist[sprite_register^1] & 0x7;
#else
            sprite->priority = sprprilist[sprite_register] & 0x7;
#endif
            if (not_lut) cmd->CMDCOLR &= 0x7FF;
            break;
         case 6:
            sprite_register = (*reg_src & 0x7000) >> 12;
#ifdef WORDS_BIGENDIAN
            sprite->priority = sprprilist[sprite_register^1] & 0x7;
#else
            sprite->priority = sprprilist[sprite_register] & 0x7;
#endif
            if (not_lut) cmd->CMDCOLR &= 0x3FF;
            break;
         case 7:
            sprite_register = (*reg_src & 0x7000) >> 12;
#ifdef WORDS_BIGENDIAN
            sprite->priority = sprprilist[sprite_register^1] & 0x7;
#else
            sprite->priority = sprprilist[sprite_register] & 0x7;
#endif
            if (not_lut) cmd->CMDCOLR &= 0x1FF;
            break;
         case 8:
            sprite_register = (*reg_src & 0x80) >> 7;
#ifdef WORDS_BIGENDIAN
            sprite->priority = sprprilist[sprite_register^1] & 0x7;
#else
            sprite->priority = sprprilist[sprite_register] & 0x7;
#endif
            if (not_lut) cmd->CMDCOLR &= 0x7F;
            break;
         case 9:
            sprite_register = (*reg_src & 0x80) >> 7;
#ifdef WORDS_BIGENDIAN
            sprite->priority = sprprilist[sprite_register^1] & 0x7;
#else
            sprite->priority = sprprilist[sprite_register] & 0x7;
#endif
            if (not_lut) cmd->CMDCOLR &= 0x3F;
            break;
         case 10:
            sprite_register = (*reg_src & 0xC0) >> 6;
#ifdef WORDS_BIGENDIAN
            sprite->priority = sprprilist[sprite_register^1] & 0x7;
#else
            sprite->priority = sprprilist[sprite_register] & 0x7;
#endif
            if (not_lut) cmd->CMDCOLR &= 0x3F;
            break;
         case 11:
            sprite_register = 0;
#ifdef WORDS_BIGENDIAN
            sprite->priority = sprprilist[sprite_register^1] & 0x7;
#else
            sprite->priority = sprprilist[sprite_register] & 0x7;
#endif
            if (not_lut) cmd->CMDCOLR &= 0x3F;
            break;
         case 12:
            sprite_register = (*reg_src & 0x80) >> 7;
#ifdef WORDS_BIGENDIAN
            sprite->priority = sprprilist[sprite_register^1] & 0x7;
#else
            sprite->priority = sprprilist[sprite_register] & 0x7;
#endif
            if (not_lut) cmd->CMDCOLR &= 0xFF;
            break;
         case 13:
            sprite_register = (*reg_src & 0x80) >> 7;
#ifdef WORDS_BIGENDIAN
            sprite->priority = sprprilist[sprite_register^1] & 0x7;
#else
            sprite->priority = sprprilist[sprite_register] & 0x7;
#endif
            if (not_lut) cmd->CMDCOLR &= 0xFF;
            break;
         case 14:
            sprite_register = (*reg_src & 0xC0) >> 6;
#ifdef WORDS_BIGENDIAN
            sprite->priority = sprprilist[sprite_register^1] & 0x7;
#else
            sprite->priority = sprprilist[sprite_register] & 0x7;
#endif
            if (not_lut) cmd->CMDCOLR &= 0xFF;
            break;
         case 15:
            sprite_register = 0;
#ifdef WORDS_BIGENDIAN
            sprite->priority = sprprilist[sprite_register^1] & 0x7;
#else
            sprite->priority = sprprilist[sprite_register] & 0x7;
#endif
            if (not_lut) cmd->CMDCOLR &= 0xFF;
            break;
         default:
            VDP1LOG("sprite type %d not implemented\n", sprite_type);

            // if we don't know what to do with a sprite, we put it on top
            sprite->priority = 7;
            break;
      }
   }
}

//////////////////////////////////////////////////////////////////////////////

static void Vdp1SetTextureRatio(int vdp2widthratio, int vdp2heightratio)
{
   float vdp1w=1;
   float vdp1h=1;

   // may need some tweaking

   // Figure out which vdp1 screen mode to use
   switch (Vdp1Regs->TVMR & 7)
   {
      case 0:
      case 2:
      case 3:
          vdp1w=1;
          break;
      case 1:
          vdp1w=2;
          break;
      default:
          vdp1w=1;
          vdp1h=1;
          break;
   }

   // Is double-interlace enabled?
   if (Vdp1Regs->FBCR & 0x8)
      vdp1h=2;

   vdp1wratio = (float)vdp2widthratio / vdp1w;
   vdp1hratio = (float)vdp2heightratio / vdp1h;
}

//////////////////////////////////////////////////////////////////////////////

static u32 Vdp2ColorRamGetColor(u32 colorindex, int alpha)
{
   switch(Vdp2Internal.ColorMode)
   {
      case 0:
      case 1:
      {
         u32 tmp;
         colorindex <<= 1;
         tmp = T2ReadWord(Vdp2ColorRam, colorindex & 0xFFF);
         return SAT2YAB1(alpha, tmp);
      }
      case 2:
      {
         u32 tmp1, tmp2;
         colorindex <<= 2;
         colorindex &= 0xFFF;
         tmp1 = T2ReadWord(Vdp2ColorRam, colorindex);
         tmp2 = T2ReadWord(Vdp2ColorRam, colorindex+2);
         return SAT2YAB2(alpha, tmp1, tmp2);
      }
      default: break;
   }

   return 0;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL Vdp2DrawCell(vdp2draw_struct *info, YglTexture *texture)
{
   u32 color;
   int i, j;

   switch(info->colornumber)
   {
      case 0: // 4 BPP
         for(i = 0;i < info->cellh;i++)
         {
            for(j = 0;j < info->cellw;j+=4)
            {
               u16 dot = T1ReadWord(Vdp2Ram, info->charaddr & 0x7FFFF);

               info->charaddr += 2;
               if (!(dot & 0xF000) && info->transparencyenable) color = 0x00000000;
               else color = Vdp2ColorRamGetColor(info->coloroffset + ((info->paladdr << 4) | ((dot & 0xF000) >> 12)), info->alpha);
               *texture->textdata++ = info->PostPixelFetchCalc(info, color);
               if (!(dot & 0xF00) && info->transparencyenable) color = 0x00000000;
               else color = Vdp2ColorRamGetColor(info->coloroffset + ((info->paladdr << 4) | ((dot & 0xF00) >> 8)), info->alpha);
               *texture->textdata++ = info->PostPixelFetchCalc(info, color);
               if (!(dot & 0xF0) && info->transparencyenable) color = 0x00000000;
               else color = Vdp2ColorRamGetColor(info->coloroffset + ((info->paladdr << 4) | ((dot & 0xF0) >> 4)), info->alpha);
               *texture->textdata++ = info->PostPixelFetchCalc(info, color);
               if (!(dot & 0xF) && info->transparencyenable) color = 0x00000000;
               else color = Vdp2ColorRamGetColor(info->coloroffset + ((info->paladdr << 4) | (dot & 0xF)), info->alpha);
               *texture->textdata++ = info->PostPixelFetchCalc(info, color);
            }
            texture->textdata += texture->w;
         }
         break;
      case 1: // 8 BPP
         for(i = 0;i < info->cellh;i++)
         {
            for(j = 0;j < info->cellw;j+=2)
            {
               u16 dot = T1ReadWord(Vdp2Ram, info->charaddr & 0x7FFFF);

               info->charaddr += 2;
               if (!(dot & 0xFF00) && info->transparencyenable) color = 0x00000000;
               else color = Vdp2ColorRamGetColor(info->coloroffset + ((info->paladdr << 4) | ((dot & 0xFF00) >> 8)), info->alpha);
               *texture->textdata++ = info->PostPixelFetchCalc(info, color);
               if (!(dot & 0xFF) && info->transparencyenable) color = 0x00000000;
               else color = Vdp2ColorRamGetColor(info->coloroffset + ((info->paladdr << 4) | (dot & 0xFF)), info->alpha);
               *texture->textdata++ = info->PostPixelFetchCalc(info, color);
            }
            texture->textdata += texture->w;
         }
         break;
    case 2: // 16 BPP(palette)
      for(i = 0;i < info->cellh;i++)
      {
        for(j = 0;j < info->cellw;j++)
        {
          u16 dot = T1ReadWord(Vdp2Ram, info->charaddr & 0x7FFFF);
          if ((dot == 0) && info->transparencyenable) color = 0x00000000;
          else color = Vdp2ColorRamGetColor(info->coloroffset + dot, info->alpha);
          info->charaddr += 2;
          *texture->textdata++ = info->PostPixelFetchCalc(info, color);
	}
        texture->textdata += texture->w;
      }
      break;
    case 3: // 16 BPP(RGB)
      for(i = 0;i < info->cellh;i++)
      {
        for(j = 0;j < info->cellw;j++)
        {
          u16 dot = T1ReadWord(Vdp2Ram, info->charaddr & 0x7FFFF);
          info->charaddr += 2;
          if (!(dot & 0x8000) && info->transparencyenable) color = 0x00000000;
	  else color = SAT2YAB1(0xFF, dot);
          *texture->textdata++ = info->PostPixelFetchCalc(info, color);
	}
        texture->textdata += texture->w;
      }
      break;
    case 4: // 32 BPP
      for(i = 0;i < info->cellh;i++)
      {
        for(j = 0;j < info->cellw;j++)
        {
          u16 dot1, dot2;
          dot1 = T1ReadWord(Vdp2Ram, info->charaddr & 0x7FFFF);
          info->charaddr += 2;
          dot2 = T1ReadWord(Vdp2Ram, info->charaddr & 0x7FFFF);
          info->charaddr += 2;
          if (!(dot1 & 0x8000) && info->transparencyenable) color = 0x00000000;
          else color = SAT2YAB2(info->alpha, dot1, dot2);
          *texture->textdata++ = info->PostPixelFetchCalc(info, color);
	}
        texture->textdata += texture->w;
      }
      break;
  }
}

//////////////////////////////////////////////////////////////////////////////

static void Vdp2DrawPattern(vdp2draw_struct *info, YglTexture *texture)
{
   u32 cacheaddr = ((u32) (info->alpha >> 3) << 27) | (info->paladdr << 20) | info->charaddr;
   int * c;
   YglSprite tile;

   tile.w = tile.h = info->patternpixelwh;
   tile.flip = info->flipfunction;

   if (info->specialprimode == 1)
      tile.priority = (info->priority & 0xFFFFFFFE) | info->specialfunction;
   else
      tile.priority = info->priority;
   tile.vertices[0] = info->x * info->coordincx;
   tile.vertices[1] = info->y * info->coordincy;
   tile.vertices[2] = (info->x + tile.w) * info->coordincx;
   tile.vertices[3] = info->y * info->coordincy;
   tile.vertices[4] = (info->x + tile.w) * info->coordincx;
   tile.vertices[5] = (info->y + tile.h) * info->coordincy;
   tile.vertices[6] = info->x * info->coordincx;
   tile.vertices[7] = (info->y + tile.h) * info->coordincy;

   if ((c = YglIsCached(cacheaddr)) != NULL)
   {
      YglCachedQuad(&tile, c);

      info->x += tile.w;
      info->y += tile.h;
      return;
   }

   c = YglQuad(&tile, texture);
   YglCache(cacheaddr, c);

   switch(info->patternwh)
   {
      case 1:
         Vdp2DrawCell(info, texture);
         break;
      case 2:
         texture->w += 8;
         Vdp2DrawCell(info, texture);
         texture->textdata -= (texture->w + 8) * 8 - 8;
         Vdp2DrawCell(info, texture);
         texture->textdata -= 8;
         Vdp2DrawCell(info, texture);
         texture->textdata -= (texture->w + 8) * 8 - 8;
         Vdp2DrawCell(info, texture);
         break;
   }
   info->x += tile.w;
   info->y += tile.h;
}

//////////////////////////////////////////////////////////////////////////////

static void Vdp2PatternAddr(vdp2draw_struct *info)
{
   switch(info->patterndatasize)
   {
      case 1:
      {
         u16 tmp = T1ReadWord(Vdp2Ram, info->addr);

         info->addr += 2;
         info->specialfunction = (info->supplementdata >> 9) & 0x1;

         switch(info->colornumber)
         {
            case 0: // in 16 colors
               info->paladdr = ((tmp & 0xF000) >> 12) | ((info->supplementdata & 0xE0) >> 1);
               break;
            default: // not in 16 colors
               info->paladdr = (tmp & 0x7000) >> 8;
               break;
         }

         switch(info->auxmode)
         {
            case 0:
               info->flipfunction = (tmp & 0xC00) >> 10;

               switch(info->patternwh)
               {
                  case 1:
                     info->charaddr = (tmp & 0x3FF) | ((info->supplementdata & 0x1F) << 10);
                     break;
                  case 2:
                     info->charaddr = ((tmp & 0x3FF) << 2) | (info->supplementdata & 0x3) | ((info->supplementdata & 0x1C) << 10);
                     break;
               }
               break;
            case 1:
               info->flipfunction = 0;

               switch(info->patternwh)
               {
                  case 1:
                     info->charaddr = (tmp & 0xFFF) | ((info->supplementdata & 0x1C) << 10);
                     break;
                  case 2:
                     info->charaddr = ((tmp & 0xFFF) << 2) | (info->supplementdata & 0x3) | ((info->supplementdata & 0x10) << 10);
                     break;
               }
               break;
         }

         break;
      }
      case 2: {
         u16 tmp1 = T1ReadWord(Vdp2Ram, info->addr);
         u16 tmp2 = T1ReadWord(Vdp2Ram, info->addr+2);
         info->addr += 4;
         info->charaddr = tmp2 & 0x7FFF;
         info->flipfunction = (tmp1 & 0xC000) >> 14;
         info->paladdr = (tmp1 & 0x7F);
         info->specialfunction = (tmp1 & 0x2000) >> 13;
         break;
      }
   }

   if (!(Vdp2Regs->VRSIZE & 0x8000))
      info->charaddr &= 0x3FFF;

   info->charaddr *= 0x20; // thanks Runik
}

//////////////////////////////////////////////////////////////////////////////

static void Vdp2DrawPage(vdp2draw_struct *info, YglTexture *texture)
{
   int X, Y;
   int i, j;

   X = info->x;
   for(i = 0;i < info->pagewh;i++)
   {
      Y = info->y;
      info->x = X;
      for(j = 0;j < info->pagewh;j++)
      {
         info->y = Y;
         if ((info->x >= -info->patternpixelwh) &&
             (info->y >= -info->patternpixelwh) &&
             (info->x <= info->draww) &&
             (info->y <= info->drawh))
         {
            Vdp2PatternAddr(info);
            Vdp2DrawPattern(info, texture);
         }
         else
         {
            info->addr += info->patterndatasize * 2;
            info->x += info->patternpixelwh;
            info->y += info->patternpixelwh;
         }
      }
   }
}

//////////////////////////////////////////////////////////////////////////////

static void Vdp2DrawPlane(vdp2draw_struct *info, YglTexture *texture)
{
   int X, Y;
   int i, j;

   X = info->x;
   for(i = 0;i < info->planeh;i++)
   {
      Y = info->y;
      info->x = X;
      for(j = 0;j < info->planew;j++)
      {
         info->y = Y;
         Vdp2DrawPage(info, texture);
      }
   }
}

//////////////////////////////////////////////////////////////////////////////

static void Vdp2DrawMap(vdp2draw_struct *info, YglTexture *texture)
{
   int i, j;
   int X, Y;

   X = info->x;

   info->patternpixelwh = 8 * info->patternwh;
   info->draww = (int)((float)vdp2width / info->coordincx);
   info->drawh = (int)((float)vdp2height / info->coordincy);

   for(i = 0;i < info->mapwh;i++)
   {
      Y = info->y;
      info->x = X;
      for(j = 0;j < info->mapwh;j++)
      {
         info->y = Y;
         info->PlaneAddr(info, info->mapwh * i + j);
          Vdp2DrawPlane(info, texture);
      }
   }
}

//////////////////////////////////////////////////////////////////////////////

static u32 FASTCALL DoNothing(UNUSED void *info, u32 pixel)
{
   return pixel;
}

//////////////////////////////////////////////////////////////////////////////

static u32 FASTCALL DoColorOffset(void *info, u32 pixel)
{
    return COLOR_ADD(pixel, ((vdp2draw_struct *)info)->cor,
                     ((vdp2draw_struct *)info)->cog,
                     ((vdp2draw_struct *)info)->cob);
}

//////////////////////////////////////////////////////////////////////////////

static INLINE void ReadVdp2ColorOffset(vdp2draw_struct *info, int mask)
{
   if (Vdp2Regs->CLOFEN & mask)
   {
      // color offset enable
      if (Vdp2Regs->CLOFSL & mask)
      {
         // color offset B
         info->cor = Vdp2Regs->COBR & 0xFF;
         if (Vdp2Regs->COBR & 0x100)
            info->cor |= 0xFFFFFF00;

         info->cog = Vdp2Regs->COBG & 0xFF;
         if (Vdp2Regs->COBG & 0x100)
            info->cog |= 0xFFFFFF00;

         info->cob = Vdp2Regs->COBB & 0xFF;
         if (Vdp2Regs->COBB & 0x100)
            info->cob |= 0xFFFFFF00;
      }
      else
      {
         // color offset A
         info->cor = Vdp2Regs->COAR & 0xFF;
         if (Vdp2Regs->COAR & 0x100)
            info->cor |= 0xFFFFFF00;

         info->cog = Vdp2Regs->COAG & 0xFF;
         if (Vdp2Regs->COAG & 0x100)
            info->cog |= 0xFFFFFF00;

         info->cob = Vdp2Regs->COAB & 0xFF;
         if (Vdp2Regs->COAB & 0x100)
            info->cob |= 0xFFFFFF00;
      }

      info->PostPixelFetchCalc = &DoColorOffset;
   }
   else // color offset disable
      info->PostPixelFetchCalc = &DoNothing;
}

//////////////////////////////////////////////////////////////////////////////

static INLINE u32 Vdp2RotationFetchPixel(vdp2draw_struct *info, int x, int y, int cellw)
{
   u32 dot;

   switch(info->colornumber)
   {
      case 0: // 4 BPP
         dot = T1ReadByte(Vdp2Ram, ((info->charaddr + ((y * cellw) + x) / 2) & 0x7FFFF));
         if (!(x & 0x1)) dot >>= 4;
         if (!(dot & 0xF) && info->transparencyenable) return 0x00000000;
         else return Vdp2ColorRamGetColor(info->coloroffset + ((info->paladdr << 4) | (dot & 0xF)), info->alpha);
      case 1: // 8 BPP
         dot = T1ReadByte(Vdp2Ram, ((info->charaddr + (y * cellw) + x) & 0x7FFFF));
         if (!(dot & 0xFF) && info->transparencyenable) return 0x00000000;
         else return Vdp2ColorRamGetColor(info->coloroffset + ((info->paladdr << 4) | (dot & 0xFF)), info->alpha);
      case 2: // 16 BPP(palette)
         dot = T1ReadWord(Vdp2Ram, ((info->charaddr + ((y * cellw) + x) * 2) & 0x7FFFF));
         if ((dot == 0) && info->transparencyenable) return 0x00000000;
         else return Vdp2ColorRamGetColor(info->coloroffset + dot, info->alpha);
      case 3: // 16 BPP(RGB)
         dot = T1ReadWord(Vdp2Ram, ((info->charaddr + ((y * cellw) + x) * 2) & 0x7FFFF));
         if (!(dot & 0x8000) && info->transparencyenable) return 0x00000000;
         else return SAT2YAB1(0xFF, dot);
      case 4: // 32 BPP
         dot = T1ReadLong(Vdp2Ram, ((info->charaddr + ((y * cellw) + x) * 4) & 0x7FFFF));
         if (!(dot & 0x80000000) && info->transparencyenable) return 0x00000000;
         else return SAT2YAB2(info->alpha, (dot >> 16), dot);
      default:
         return 0;
   }
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL Vdp2DrawRotation(vdp2draw_struct *info, vdp2rotationparameter_struct *parameter, YglTexture *texture)
{
   if (!parameter->coefenab)
   {
      // Since coefficients aren't being used, we can simplify the drawing process
      if (IsScreenRotated(parameter))
      {
         // No rotation
         // FIXME - This should really be fixed point math
         info->x = (int)-(parameter->kx * (parameter->Xst - parameter->Px) + parameter->Px + parameter->Mx);
         info->y = (int)-(parameter->ky * (parameter->Yst - parameter->Py) + parameter->Py + parameter->My);
         info->coordincx = 1.0 / parameter->kx;
         info->coordincy = 1.0 / parameter->ky;
      }
      else
      {
         // Do simple rotation
         CalculateRotationValues(parameter);

         if (info->isbitmap)
         {
            // Bitmap
            info->vertices[0] = GenerateRotatedXPos(parameter, 0, 0);
            info->vertices[1] = GenerateRotatedYPos(parameter, 0, 0);
            info->vertices[2] = GenerateRotatedXPos(parameter, info->cellw-1, 0);
            info->vertices[3] = GenerateRotatedYPos(parameter, info->cellw-1, 0);
            info->vertices[4] = GenerateRotatedXPos(parameter, info->cellw-1, info->cellh-1);
            info->vertices[5] = GenerateRotatedYPos(parameter, info->cellw-1, info->cellh-1);
            info->vertices[6] = GenerateRotatedXPos(parameter, 0, info->cellh-1);
            info->vertices[7] = GenerateRotatedYPos(parameter, 0, info->cellh-1);

            YglQuad((YglSprite *)info, texture);
            Vdp2DrawCell(info, texture);
            return;
         }
         else
         {
            Vdp2DrawMap(info, texture);
            return;
         }
      }
   }
   else
   {
      // Rotation using Coefficient Tables(now this stuff just gets wacky. It
      // has to be done in software, no exceptions)
      int i, j;
      int x, y;
      int cellw, cellh;
      int pagepixelwh;
      int planepixelwidth;
      int planepixelheight;
      int screenwidth;
      int screenheight;
      int oldcellx=-1, oldcelly=-1;

      CalculateRotationValues(parameter);

      info->vertices[0] = 0;
      info->vertices[1] = 0;
      info->vertices[2] = vdp2width;
      info->vertices[3] = 0;
      info->vertices[4] = vdp2width;
      info->vertices[5] = vdp2height;
      info->vertices[6] = 0;
      info->vertices[7] = vdp2height;

      cellw = info->cellw;
      cellh = info->cellh;
      info->cellw = vdp2width;
      info->cellh = vdp2height;
      info->flipfunction = 0;
      YglQuad((YglSprite *)info, texture);

      if (!info->isbitmap)
      {
         pagepixelwh=64*8;
         planepixelwidth=info->planew*pagepixelwh;
         planepixelheight=info->planeh*pagepixelwh;
         screenwidth=4*planepixelwidth;
         screenheight=4*planepixelheight;
         oldcellx=-1;
         oldcelly=-1;
      }
      else
      {
         pagepixelwh=0;
         planepixelwidth=0;
         planepixelheight=0;
         screenwidth=0;
         screenheight=0;
         oldcellx=0;
         oldcelly=0;
      }

      for (j = 0; j < vdp2height; j++)
      {
         if (parameter->deltaKAx == 0)
         {
            Vdp2ReadCoefficient(parameter,
                                parameter->coeftbladdr +
                                (u32)(((float)j)*parameter->deltaKAst *
                                ((float)parameter->coefdatasize)));
         }


         for (i = 0; i < vdp2width; i++)
         {
            u32 color;

            if (parameter->deltaKAx != 0)
            {
               Vdp2ReadCoefficient(parameter,
                                   parameter->coeftbladdr+
                                   (u32)((((float)j*parameter->deltaKAst) +
                                   ((float)i*parameter->deltaKAx)) *
                                   ((float)parameter->coefdatasize)));
            }

            if (parameter->msb)
            {
               *texture->textdata++ = info->PostPixelFetchCalc(info, 0x00000000);
               continue;
            }

            x = GenerateRotatedXPos(parameter, i, j);
            y = GenerateRotatedYPos(parameter, i, j);

            // Convert coordinates into graphics
            if (info->isbitmap)
            {
               x &= cellw-1;
               y &= cellh-1;

               // Fetch Pixel
               color = Vdp2RotationFetchPixel(info, x, y, cellw);
            }
            else
            {
               // Tile
               int planenum;
               int pagesize=info->pagewh*info->pagewh;

               x &= screenwidth-1;
               y &= screenheight-1;

               if ((x / (8 * info->patternwh)) != oldcellx ||
                   (y / (8 * info->patternwh)) != oldcelly)
               {
                  oldcellx = x / (8 * info->patternwh);
                  oldcelly = y / (8 * info->patternwh);

                  // Calculate which plane we're dealing with
                  planenum = ((int)(y / planepixelheight)*4) + (int)(x / planepixelwidth);
                  x = (x % planepixelwidth);
                  y = (y % planepixelheight);

                  // Fetch and decode pattern name data
                  info->PlaneAddr(info, planenum); // needs reworking

                  // Figure out which page it's on(if plane size is not 1x1)
                  info->addr += ((y / pagepixelwh * pagesize * info->planew) +
                                (x / pagepixelwh * pagesize) +
                                ((y % pagepixelwh) / (8 * info->patternwh) * info->pagewh) +
                                ((x % pagepixelwh) / (8 * info->patternwh))) * info->patterndatasize * 2;

                  Vdp2PatternAddr(info); // Heh, this could be optimized
               }

               // Figure out which pixel in the tile we want
               if (info->patternwh == 1)
               {
                  x &= 8-1;
                  y &= 8-1;

                  // vertical flip
                  if (info->flipfunction & 0x2)
                     y = 8 - 1 - y;

                  // horizontal flip
                  if (info->flipfunction & 0x1)
                     x = 8 - 1 - x;
               }
               else
               {
                  if (info->flipfunction)
                  {
                     y &= 16 - 1;
                     if (info->flipfunction & 0x2)
                     {
                        if (!(y & 8))
                           y = 8 - 1 - y + 16;
                        else
                           y = 16 - 1 - y;
                     }
                     else if (y & 8)
                        y += 8;

                     if (info->flipfunction & 0x1)
                     {
                        if (!(x & 8))
                           y += 8;

                        x &= 8-1;
                        x = 8 - 1 - x;
                     }
                     else if (x & 8)
                     {
                        y += 8;
                        x &= 8-1;
                     }
                     else
                        x &= 8-1;
                  }
                  else
                  {
                     y &= 16 - 1;
                     if (y & 8)
                        y += 8;
                     if (x & 8)
                        y += 8;
                     x &= 8-1;
                  }
               }

               // Fetch pixel
               color = Vdp2RotationFetchPixel(info, x, y, 8);
            }

            *texture->textdata++ = info->PostPixelFetchCalc(info, color);
         }
         texture->textdata += texture->w;
      }
      return;
   }

   if (info->isbitmap)
   {
      info->vertices[0] = info->x * info->coordincx;
      info->vertices[1] = info->y * info->coordincy;
      info->vertices[2] = (info->x + info->cellw) * info->coordincx;
      info->vertices[3] = info->y * info->coordincy;
      info->vertices[4] = (info->x + info->cellw) * info->coordincx;
      info->vertices[5] = (info->y + info->cellh) * info->coordincy;
      info->vertices[6] = info->x * info->coordincx;
      info->vertices[7] = (info->y + info->cellh) * info->coordincy;

      YglQuad((YglSprite *)info, texture);
      Vdp2DrawCell(info, texture);
   }
   else
      Vdp2DrawMap(info, texture);
}

//////////////////////////////////////////////////////////////////////////////

static void SetSaturnResolution(int width, int height)
{
   YglChangeResolution(width, height);

   vdp2width=width;
   vdp2height=height;
}

//////////////////////////////////////////////////////////////////////////////

#ifdef USEMICSHADERS
#ifndef GLchar
#define GLchar GLbyte
#endif

#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER 0x8B30
#endif
#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER 0x8B31
#endif

GLuint (STDCALL *pfglCreateProgram)(void);
GLuint (STDCALL *pfglCreateShader)(GLenum);
void (STDCALL *pfglShaderSource)(GLuint,GLsizei,const GLchar **,const GLint *);
void (STDCALL *pfglCompileShader)(GLuint);
void (STDCALL *pfglAttachShader)(GLuint,GLuint);
void (STDCALL *pfglLinkProgram)(GLuint);
void (STDCALL *pfglUseProgram)(GLuint);
GLint (STDCALL *pfglGetUniformLocation)(GLuint,const GLchar *);
void (STDCALL *pfglUniform1i)(GLint,GLint);
void (STDCALL *pfglGetShaderInfoLog)(GLuint,GLsizei,GLsizei *,GLchar *);

GLuint shaderProgram;
GLuint saturnMeshGouraudFragmentShader;
int useShaders=0;

// RGBA pattern that assures that no mesh effect or gouraud shading is applied
const unsigned char noMeshGouraud[16] = {0x80,0x80,0x80,0xFF,0x80,0x80,0x80,0xFF,0x80,0x80,0x80,0xFF,0x80,0x80,0x80,0xFF};

// This shader implements the mesh processing and gouraud shading of the VDP1.
// Mesh processing is only applied when gl_Color.a==0 (set by VIDOGLVdp1PolygonDraw).
const GLchar saturnMeshGouraudFragmentShaderCode[] = \
"uniform sampler2D mytexture;\n" \
"void main() {\n" \
"  vec4 baseColor = texture2D(mytexture, gl_TexCoord[0].st);\n" \
"  float red,green,blue;\n" \
"  int xlsb = mod(floor(gl_FragCoord.x),2);\n" \
"  int ylsb = mod(floor(gl_FragCoord.y),2);\n" \
"  red = clamp(baseColor.r + (gl_Color.r - 0.5), 0.0, 1.0);\n" \
"  green = clamp(baseColor.g + (gl_Color.g - 0.5), 0.0, 1.0);\n" \
"  blue = clamp(baseColor.b + (gl_Color.b - 0.5), 0.0, 1.0);\n" \
"  if (gl_Color.a == 0.0) {\n" \
"    if ((xlsb+ylsb) != 1) {\n" \
"      gl_FragColor = vec4(red, green, blue, baseColor.a);\n" \
"    } else {\n" \
"      gl_FragColor = vec4(red, green, blue, 0.0);\n" \
"    }\n" \
"  } else {\n" \
"    gl_FragColor = vec4(red, green, blue, baseColor.a);\n" \
"  }\n" \
"}\n";
const GLchar *saturnMeshGouraudFragmentShaderSource[] = {saturnMeshGouraudFragmentShaderCode, NULL};

#ifdef HAVE_GLXGETPROCADDRESS
void STDCALL * (*yglGetProcAddress)(const char *szProcName) = glXGetProcAddress;
#elif WIN32
void STDCALL * (*yglGetProcAddress)(const char *szProcName) = wglGetProcAddress;
#endif
#endif

int VIDOGLInit(void)
{
#ifdef USEMICSHADERS
   GLint mytexture;
   char shaderInfoLog[256];
#endif

   if (YglInit(1024, 1024, 8) != 0)
      return -1;

   SetSaturnResolution(320, 224);

   vdp1wratio = 1;
   vdp1hratio = 1;

#ifdef USEMICSHADERS
   // Set up fragment shader
   useShaders = 0;
   pfglCreateProgram = yglGetProcAddress("glCreateProgram");
   pfglCreateShader = yglGetProcAddress("glCreateShader");
   pfglCompileShader = yglGetProcAddress("glCompileShader");
   pfglAttachShader = yglGetProcAddress("glAttachShader");
   pfglLinkProgram = yglGetProcAddress("glLinkProgram");
   pfglUseProgram = yglGetProcAddress("glUseProgram");
   pfglShaderSource = yglGetProcAddress("glShaderSource");
   pfglGetUniformLocation = yglGetProcAddress("glGetUniformLocation");
   pfglUniform1i = yglGetProcAddress("glUniform1i");
   pfglGetShaderInfoLog = yglGetProcAddress("glGetShaderInfoLog");

   if (pfglCreateProgram && pfglCreateShader && pfglCompileShader && pfglShaderSource &&
       pfglAttachShader && pfglLinkProgram && pfglUseProgram && pfglGetUniformLocation &&
       pfglUniform1i)
   {
      shaderProgram = pfglCreateProgram();
      if (shaderProgram)
      {
	     saturnMeshGouraudFragmentShader = pfglCreateShader(GL_FRAGMENT_SHADER);
		 if (saturnMeshGouraudFragmentShader)
		 {
		    useShaders = 1;
		    pfglShaderSource(saturnMeshGouraudFragmentShader, 1, saturnMeshGouraudFragmentShaderSource, NULL);
		    pfglCompileShader(saturnMeshGouraudFragmentShader);
            pfglGetShaderInfoLog(saturnMeshGouraudFragmentShader,255,NULL,shaderInfoLog);
		    pfglAttachShader(shaderProgram, saturnMeshGouraudFragmentShader);
		    pfglLinkProgram(shaderProgram);
		    mytexture = pfglGetUniformLocation(shaderProgram, "mytexture");
		    pfglUniform1i(mytexture, 0);
		 }
	  }
   }


   /*FILE *fp;
   fp = fopen("yashader.txt", "wb");
   fprintf(fp, "%p %p %p %p %p %p %p useShaders=%d\r\n",pfglCreateProgram,pfglCreateShader,pfglCompileShader,
   pfglAttachShader,pfglLinkProgram,pfglUseProgram,pfglShaderSource,useShaders);
   fputs(shaderInfoLog,fp);
   fclose(fp);*/
#endif

   return 0;
}

//////////////////////////////////////////////////////////////////////////////

void VIDOGLDeInit(void)
{
   YglDeInit();
}

//////////////////////////////////////////////////////////////////////////////

int _VIDOGLIsFullscreen;

void VIDOGLResize(unsigned int w, unsigned int h, int on)
{
   glDeleteTextures(1, &_Ygl->texture);

   _VIDOGLIsFullscreen = on;

   if (on)
      YuiSetVideoMode(w, h, 32, 1);
   else
      YuiSetVideoMode(w, h, 32, 0);

   YglGLInit(1024, 1024);
   glViewport(0, 0, w, h);

   SetSaturnResolution(vdp2width, vdp2height);

   GlHeight=h;
   GlWidth=w;
}

//////////////////////////////////////////////////////////////////////////////

int VIDOGLIsFullscreen(void) {
   return _VIDOGLIsFullscreen;
}

//////////////////////////////////////////////////////////////////////////////

int VIDOGLVdp1Reset(void)
{
   return 0;
}

//////////////////////////////////////////////////////////////////////////////

void VIDOGLVdp1DrawStart(void)
{
   YglCacheReset();

   if (Vdp2Regs->CLOFEN & 0x40)
   {
      // color offset enable
      if (Vdp2Regs->CLOFSL & 0x40)
      {
         // color offset B
         vdp1cor = Vdp2Regs->COBR & 0xFF;
         if (Vdp2Regs->COBR & 0x100)
            vdp1cor |= 0xFFFFFF00;

         vdp1cog = Vdp2Regs->COBG & 0xFF;
         if (Vdp2Regs->COBG & 0x100)
            vdp1cog |= 0xFFFFFF00;

         vdp1cob = Vdp2Regs->COBB & 0xFF;
         if (Vdp2Regs->COBB & 0x100)
            vdp1cob |= 0xFFFFFF00;
      }
      else
      {
         // color offset A
         vdp1cor = Vdp2Regs->COAR & 0xFF;
         if (Vdp2Regs->COAR & 0x100)
            vdp1cor |= 0xFFFFFF00;

         vdp1cog = Vdp2Regs->COAG & 0xFF;
         if (Vdp2Regs->COAG & 0x100)
            vdp1cog |= 0xFFFFFF00;

         vdp1cob = Vdp2Regs->COAB & 0xFF;
         if (Vdp2Regs->COAB & 0x100)
            vdp1cob |= 0xFFFFFF00;
      }
   }
   else // color offset disable
      vdp1cor = vdp1cog = vdp1cob = 0;
}

//////////////////////////////////////////////////////////////////////////////

void VIDOGLVdp1DrawEnd(void)
{
}

//////////////////////////////////////////////////////////////////////////////

void VIDOGLVdp1NormalSpriteDraw(void)
{
   vdp1cmd_struct cmd;
   YglSprite sprite;
   YglTexture texture;
   int * c;
   u32 tmp;
   s16 x, y;
   u16 CMDPMOD;
#ifdef USEMICSHADERS
   YglColor colors;
   u16 color2;
   u8 mesh;
   int i;
#endif

   Vdp1ReadCommand(&cmd, Vdp1Regs->addr);

   x = cmd.CMDXA + Vdp1Regs->localX;
   y = cmd.CMDYA + Vdp1Regs->localY;
   sprite.w = ((cmd.CMDSIZE >> 8) & 0x3F) * 8;
   sprite.h = cmd.CMDSIZE & 0xFF;

   sprite.flip = (cmd.CMDCTRL & 0x30) >> 4;

   sprite.vertices[0] = (int)((float)x * vdp1wratio);
   sprite.vertices[1] = (int)((float)y * vdp1hratio);
   sprite.vertices[2] = (int)((float)(x + sprite.w) * vdp1wratio);
   sprite.vertices[3] = (int)((float)y * vdp1hratio);
   sprite.vertices[4] = (int)((float)(x + sprite.w) * vdp1wratio);
   sprite.vertices[5] = (int)((float)(y + sprite.h) * vdp1hratio);
   sprite.vertices[6] = (int)((float)x * vdp1wratio);
   sprite.vertices[7] = (int)((float)(y + sprite.h) * vdp1hratio);

   tmp = cmd.CMDSRCA;
   tmp <<= 16;
   tmp |= cmd.CMDCOLR;

   Vdp1ReadPriority(&cmd, &sprite);

   CMDPMOD = T1ReadWord(Vdp1Ram, Vdp1Regs->addr + 0x4);

#ifdef USEMICSHADERS
   mesh = 0xFF;
   if (CMDPMOD & 0x100)
      mesh = 0x00;

   // Check if the Gouraud shading bit is set and the color mode is RGB
   if ((CMDPMOD & 4) && ((CMDPMOD & 0x38) == 0x28))
   {
      for (i=0; i<4; i++)
	  {
	     color2 = T1ReadWord(Vdp1Ram, (T1ReadWord(Vdp1Ram, Vdp1Regs->addr + 0x1C) << 3) + (i << 1));
	     colors.rgba[(i << 2) + 0] = (color2 & 0x001F) << 3;
             colors.rgba[(i << 2) + 1] = (color2 & 0x03E0) >> 2;
             colors.rgba[(i << 2) + 2] = (color2 & 0x7C00) >> 7;
	     colors.rgba[(i << 2) + 3] = mesh;
	  }
   }
   else // No Gouraud shading, use same color for all 4 vertices
   {
	  for (i=0; i<4; i++)
	  {
	     colors.rgba[(i << 2) + 0] = 0x80;
             colors.rgba[(i << 2) + 1] = 0x80;
             colors.rgba[(i << 2) + 2] = 0x80;
	     colors.rgba[(i << 2) + 3] = mesh;
	  }
   }
#endif

   if (sprite.w > 0 && sprite.h > 1)
   {
      if ((c = YglIsCached(tmp)) != NULL)
      {
#ifdef USEMICSHADERS
         YglCachedQuad2(&sprite, c, &colors);
#else
         YglCachedQuad(&sprite, c);
#endif
         return;
      }

#ifdef USEMICSHADERS
      c = YglQuad2(&sprite, &texture, &colors);
#else
      c = YglQuad(&sprite, &texture);
#endif
      YglCache(tmp, c);

      Vdp1ReadTexture(&cmd, &sprite, &texture);
   }
}

//////////////////////////////////////////////////////////////////////////////

void VIDOGLVdp1ScaledSpriteDraw(void)
{
   vdp1cmd_struct cmd;
   YglSprite sprite;
   YglTexture texture;
   int * c;
   u32 tmp;
   s16 rw=0, rh=0;
   s16 x, y;
   u16 CMDPMOD;
#ifdef USEMICSHADERS
   YglColor colors;
   u16 color2;
   u8 mesh;
   int i;
#endif

   Vdp1ReadCommand(&cmd, Vdp1Regs->addr);

   x = cmd.CMDXA + Vdp1Regs->localX;
   y = cmd.CMDYA + Vdp1Regs->localY;
   sprite.w = ((cmd.CMDSIZE >> 8) & 0x3F) * 8;
   sprite.h = cmd.CMDSIZE & 0xFF;
   sprite.flip = (cmd.CMDCTRL & 0x30) >> 4;

   // Setup Zoom Point
   switch ((cmd.CMDCTRL & 0xF00) >> 8)
   {
      case 0x0: // Only two coordinates
         rw = cmd.CMDXC - x + Vdp1Regs->localX + 1;
         rh = cmd.CMDYC - y + Vdp1Regs->localY + 1;
         break;
      case 0x5: // Upper-left
         rw = cmd.CMDXB + 1;
         rh = cmd.CMDYB + 1;
         break;
      case 0x6: // Upper-Center
         rw = cmd.CMDXB;
         rh = cmd.CMDYB;
         x = x - rw/2;
         rw++;
         rh++;
         break;
      case 0x7: // Upper-Right
         rw = cmd.CMDXB;
         rh = cmd.CMDYB;
         x = x - rw;
         rw++;
         rh++;
         break;
      case 0x9: // Center-left
         rw = cmd.CMDXB;
         rh = cmd.CMDYB;
         y = y - rh/2;
         rw++;
         rh++;
         break;
      case 0xA: // Center-center
         rw = cmd.CMDXB;
         rh = cmd.CMDYB;
         x = x - rw/2;
         y = y - rh/2;
         rw++;
         rh++;
         break;
      case 0xB: // Center-right
         rw = cmd.CMDXB;
         rh = cmd.CMDYB;
         x = x - rw;
         y = y - rh/2;
         rw++;
         rh++;
         break;
      case 0xD: // Lower-left
         rw = cmd.CMDXB;
         rh = cmd.CMDYB;
         y = y - rh;
         rw++;
         rh++;
         break;
      case 0xE: // Lower-center
         rw = cmd.CMDXB;
         rh = cmd.CMDYB;
         x = x - rw/2;
         y = y - rh;
         rw++;
         rh++;
         break;
      case 0xF: // Lower-right
         rw = cmd.CMDXB;
         rh = cmd.CMDYB;
         x = x - rw;
         y = y - rh;
         rw++;
         rh++;
         break;
      default: break;
   }

   sprite.vertices[0] = (int)((float)x * vdp1wratio);
   sprite.vertices[1] = (int)((float)y * vdp1hratio);
   sprite.vertices[2] = (int)((float)(x + rw) * vdp1wratio);
   sprite.vertices[3] = (int)((float)y * vdp1hratio);
   sprite.vertices[4] = (int)((float)(x + rw) * vdp1wratio);
   sprite.vertices[5] = (int)((float)(y + rh) * vdp1hratio);
   sprite.vertices[6] = (int)((float)x * vdp1wratio);
   sprite.vertices[7] = (int)((float)(y + rh) * vdp1hratio);

   tmp = cmd.CMDSRCA;
   tmp <<= 16;
   tmp |= cmd.CMDCOLR;

   CMDPMOD = T1ReadWord(Vdp1Ram, Vdp1Regs->addr + 0x4);

#ifdef USEMICSHADERS
   mesh = 0xFF;
   if (CMDPMOD & 0x100)
      mesh = 0x00;

   // Check if the Gouraud shading bit is set and the color mode is RGB
   if ((CMDPMOD & 4) && ((CMDPMOD & 0x38) == 0x28))
   {
      for (i=0; i<4; i++)
	  {
	     color2 = T1ReadWord(Vdp1Ram, (T1ReadWord(Vdp1Ram, Vdp1Regs->addr + 0x1C) << 3) + (i << 1));
	     colors.rgba[(i << 2) + 0] = (color2 & 0x001F) << 3;
		 colors.rgba[(i << 2) + 1] = (color2 & 0x03E0) >> 2;
		 colors.rgba[(i << 2) + 2] = (color2 & 0x7C00) >> 7;
	     colors.rgba[(i << 2) + 3] = mesh;
	  }
   }
   else // No Gouraud shading, use same color for all 4 vertices
   {
	  for (i=0; i<4; i++)
	  {
	     colors.rgba[(i << 2) + 0] = 0x80;
		 colors.rgba[(i << 2) + 1] = 0x80;
		 colors.rgba[(i << 2) + 2] = 0x80;
	     colors.rgba[(i << 2) + 3] = mesh;
	  }
   }
#endif

   Vdp1ReadPriority(&cmd, &sprite);

   if (sprite.w > 0 && sprite.h > 1)
   {
      if ((c = YglIsCached(tmp)) != NULL)
      {
#ifdef USEMICSHADERS
         YglCachedQuad2(&sprite, c, &colors);
#else
         YglCachedQuad(&sprite, c);
#endif
         return;
      }

#ifdef USEMICSHADERS
      c = YglQuad2(&sprite, &texture, &colors);
#else
      c = YglQuad(&sprite, &texture);
#endif
      YglCache(tmp, c);

      Vdp1ReadTexture(&cmd, &sprite, &texture);
   }
}

//////////////////////////////////////////////////////////////////////////////

void VIDOGLVdp1DistortedSpriteDraw(void)
{
   vdp1cmd_struct cmd;
   YglSprite sprite;
   YglTexture texture;
   int * c;
   u32 tmp;
   u16 CMDPMOD;
#ifdef USEMICSHADERS
   YglColor colors;
   u16 color2;
   u8 mesh;
   int i;
#endif

   Vdp1ReadCommand(&cmd, Vdp1Regs->addr);

   sprite.w = ((cmd.CMDSIZE >> 8) & 0x3F) * 8;
   sprite.h = cmd.CMDSIZE & 0xFF;

   sprite.flip = (cmd.CMDCTRL & 0x30) >> 4;

   sprite.vertices[0] = (s32)((float)(cmd.CMDXA + Vdp1Regs->localX) * vdp1wratio);
   sprite.vertices[1] = (s32)((float)(cmd.CMDYA + Vdp1Regs->localY) * vdp1hratio);
   sprite.vertices[2] = (s32)((float)((cmd.CMDXB + 1) + Vdp1Regs->localX) * vdp1wratio);
   sprite.vertices[3] = (s32)((float)(cmd.CMDYB + Vdp1Regs->localY) * vdp1hratio);
   sprite.vertices[4] = (s32)((float)((cmd.CMDXC + 1) + Vdp1Regs->localX) * vdp1wratio);
   sprite.vertices[5] = (s32)((float)((cmd.CMDYC + 1) + Vdp1Regs->localY) * vdp1hratio);
   sprite.vertices[6] = (s32)((float)(cmd.CMDXD + Vdp1Regs->localX) * vdp1wratio);
   sprite.vertices[7] = (s32)((float)((cmd.CMDYD + 1) + Vdp1Regs->localY) * vdp1hratio);

   tmp = cmd.CMDSRCA;

   tmp <<= 16;
   tmp |= cmd.CMDCOLR;

   CMDPMOD = T1ReadWord(Vdp1Ram, Vdp1Regs->addr + 0x4);

#ifdef USEMICSHADERS
   mesh = 0xFF;
   if (CMDPMOD & 0x100)
      mesh = 0x00;

   // Check if the Gouraud shading bit is set and the color mode is RGB
   if ((CMDPMOD & 4) && ((CMDPMOD & 0x38) == 0x28))
   {
      for (i=0; i<4; i++)
	  {
	     color2 = T1ReadWord(Vdp1Ram, (T1ReadWord(Vdp1Ram, Vdp1Regs->addr + 0x1C) << 3) + (i << 1));
	     colors.rgba[(i << 2) + 0] = (color2 & 0x001F) << 3;
		 colors.rgba[(i << 2) + 1] = (color2 & 0x03E0) >> 2;
		 colors.rgba[(i << 2) + 2] = (color2 & 0x7C00) >> 7;
	     colors.rgba[(i << 2) + 3] = mesh;
	  }
   }
   else // No Gouraud shading, use same color for all 4 vertices
   {
	  for (i=0; i<4; i++)
	  {
	     colors.rgba[(i << 2) + 0] = 0x80;
		 colors.rgba[(i << 2) + 1] = 0x80;
		 colors.rgba[(i << 2) + 2] = 0x80;
	     colors.rgba[(i << 2) + 3] = mesh;
	  }
   }
#endif

   Vdp1ReadPriority(&cmd, &sprite);

   if (sprite.w > 0 && sprite.h > 1)
   {
      if ((c = YglIsCached(tmp)) != NULL)
      {
#ifdef USEMICSHADERS
         YglCachedQuad2(&sprite, c, &colors);
#else
         YglCachedQuad(&sprite, c);
#endif
         return;
      }

#ifdef USEMICSHADERS
      c = YglQuad2(&sprite, &texture, &colors);
#else
      c = YglQuad(&sprite, &texture);
#endif
      YglCache(tmp, c);

      Vdp1ReadTexture(&cmd, &sprite, &texture);
   }
}

//////////////////////////////////////////////////////////////////////////////

void VIDOGLVdp1PolygonDraw(void)
{
   s16 X[4];
   s16 Y[4];
   u16 color;
   u16 CMDPMOD;
   u8 alpha;
   YglSprite polygon;
   YglTexture texture;
#ifdef USEMICSHADERS
   YglColor colors;
   u8 mesh;
   u16 color2;
   int i;
#endif

   X[0] = Vdp1Regs->localX + T1ReadWord(Vdp1Ram, Vdp1Regs->addr + 0xC);
   Y[0] = Vdp1Regs->localY + T1ReadWord(Vdp1Ram, Vdp1Regs->addr + 0xE);
   X[1] = Vdp1Regs->localX + T1ReadWord(Vdp1Ram, Vdp1Regs->addr + 0x10);
   Y[1] = Vdp1Regs->localY + T1ReadWord(Vdp1Ram, Vdp1Regs->addr + 0x12);
   X[2] = Vdp1Regs->localX + T1ReadWord(Vdp1Ram, Vdp1Regs->addr + 0x14);
   Y[2] = Vdp1Regs->localY + T1ReadWord(Vdp1Ram, Vdp1Regs->addr + 0x16);
   X[3] = Vdp1Regs->localX + T1ReadWord(Vdp1Ram, Vdp1Regs->addr + 0x18);
   Y[3] = Vdp1Regs->localY + T1ReadWord(Vdp1Ram, Vdp1Regs->addr + 0x1A);

   color = T1ReadWord(Vdp1Ram, Vdp1Regs->addr + 0x6);
   CMDPMOD = T1ReadWord(Vdp1Ram, Vdp1Regs->addr + 0x4);

   alpha = 0xFF;

   if ((CMDPMOD & 0x7) == 0x3)
      alpha = 0x80;

   if (color == 0)
      alpha = 0;

#ifdef USEMICSHADERS
   mesh = 0xFF;
   if (CMDPMOD & 0x100)
      mesh = 0x00;

   // Check if the Gouraud shading bit is set and the color mode is RGB
   if ((CMDPMOD & 4) && ((CMDPMOD & 0x38) == 0x28))
   {
      for (i=0; i<4; i++)
	  {
	     color2 = T1ReadWord(Vdp1Ram, (T1ReadWord(Vdp1Ram, Vdp1Regs->addr + 0x1C) << 3) + (i << 1));
	     colors.rgba[(i << 2) + 0] = (color2 & 0x001F) << 3;
		 colors.rgba[(i << 2) + 1] = (color2 & 0x03E0) >> 2;
		 colors.rgba[(i << 2) + 2] = (color2 & 0x7C00) >> 7;
	     colors.rgba[(i << 2) + 3] = mesh;
	  }
   }
   else // No Gouraud shading, use same color for all 4 vertices
   {
	  for (i=0; i<4; i++)
	  {
	     colors.rgba[(i << 2) + 0] = 0x80;
		 colors.rgba[(i << 2) + 1] = 0x80;
		 colors.rgba[(i << 2) + 2] = 0x80;
	     colors.rgba[(i << 2) + 3] = mesh;
	  }
   }
#endif

   if (color & 0x8000)
      polygon.priority = Vdp2Regs->PRISA & 0x7;
   else
   {
      int shadow, priority, colorcalc;

      priority = 0;  // Avoid compiler warning
      Vdp1ProcessSpritePixel(Vdp2Regs->SPCTL & 0xF, &color, &shadow, &priority, &colorcalc);
#ifdef WORDS_BIGENDIAN
      polygon.priority = ((u8 *)&Vdp2Regs->PRISA)[priority^1] & 0x7;
#else
      polygon.priority = ((u8 *)&Vdp2Regs->PRISA)[priority] & 0x7;
#endif
   }

   polygon.vertices[0] = (int)((float)X[0] * vdp1wratio);
   polygon.vertices[1] = (int)((float)Y[0] * vdp1hratio);
   polygon.vertices[2] = (int)((float)X[1] * vdp1wratio);
   polygon.vertices[3] = (int)((float)Y[1] * vdp1hratio);
   polygon.vertices[4] = (int)((float)X[2] * vdp1wratio);
   polygon.vertices[5] = (int)((float)Y[2] * vdp1hratio);
   polygon.vertices[6] = (int)((float)X[3] * vdp1wratio);
   polygon.vertices[7] = (int)((float)Y[3] * vdp1hratio);

   polygon.w = 1;
   polygon.h = 1;
   polygon.flip = 0;

#ifdef USEMICSHADERS
   YglQuad2(&polygon, &texture, &colors);
#else
   YglQuad(&polygon, &texture);
#endif

   if (color & 0x8000)
      *texture.textdata = COLOR_ADD(SAT2YAB1(alpha,color), vdp1cor, vdp1cog, vdp1cob);
   else
      *texture.textdata = COLOR_ADD(Vdp2ColorRamGetColor(color, alpha), vdp1cor, vdp1cog, vdp1cob);
}

//////////////////////////////////////////////////////////////////////////////

void VIDOGLVdp1PolylineDraw(void)
{
   s16 X[4];
   s16 Y[4];
   u16 color;
   u16 CMDPMOD;
   u8 alpha;
   YglSprite polygon;
   YglTexture texture;
   int * c;

   X[0] = Vdp1Regs->localX + (T1ReadWord(Vdp1Ram, Vdp1Regs->addr + 0x0C) );
   Y[0] = Vdp1Regs->localY + (T1ReadWord(Vdp1Ram, Vdp1Regs->addr + 0x0E) );
   X[1] = Vdp1Regs->localX + (T1ReadWord(Vdp1Ram, Vdp1Regs->addr + 0x10) );
   Y[1] = Vdp1Regs->localY + (T1ReadWord(Vdp1Ram, Vdp1Regs->addr + 0x12) );
   X[2] = Vdp1Regs->localX + (T1ReadWord(Vdp1Ram, Vdp1Regs->addr + 0x14) );
   Y[2] = Vdp1Regs->localY + (T1ReadWord(Vdp1Ram, Vdp1Regs->addr + 0x16) );
   X[3] = Vdp1Regs->localX + (T1ReadWord(Vdp1Ram, Vdp1Regs->addr + 0x18) );
   Y[3] = Vdp1Regs->localY + (T1ReadWord(Vdp1Ram, Vdp1Regs->addr + 0x1A) );

   color = T1ReadWord(Vdp1Ram, Vdp1Regs->addr + 0x6);
   CMDPMOD = T1ReadWord(Vdp1Ram, Vdp1Regs->addr + 0x4);

   alpha = 0xFF;
   if ((CMDPMOD & 0x7) == 0x3)
      alpha = 0x80;

   polygon.priority = Vdp2Regs->PRISA & 0x7;

   // A bit of kludge, but eventually we'll have to redo the YGL anyways.
   polygon.vertices[0] = (int)((float)X[0] * vdp1wratio);
   polygon.vertices[1] = (int)((float)Y[0] * vdp1hratio);
   polygon.vertices[2] = (int)((float)X[0] * vdp1wratio)+1;
   polygon.vertices[3] = (int)((float)Y[0] * vdp1hratio)+1;
   polygon.vertices[4] = (int)((float)X[1] * vdp1wratio);
   polygon.vertices[5] = (int)((float)Y[1] * vdp1hratio);
   polygon.vertices[6] = (int)((float)X[1] * vdp1wratio)+1;
   polygon.vertices[7] = (int)((float)Y[1] * vdp1hratio)+1;

   polygon.w = 1;
   polygon.h = 1;
   polygon.flip = 0;

   c = YglQuad(&polygon, &texture);

   polygon.vertices[0] = polygon.vertices[4];
   polygon.vertices[1] = polygon.vertices[5];
   polygon.vertices[2] = polygon.vertices[6];
   polygon.vertices[3] = polygon.vertices[7];
   polygon.vertices[4] = (int)((float)X[2] * vdp1wratio);
   polygon.vertices[5] = (int)((float)Y[2] * vdp1hratio);
   polygon.vertices[6] = (int)((float)X[2] * vdp1wratio)+1;
   polygon.vertices[7] = (int)((float)Y[2] * vdp1hratio)+1;
   YglCachedQuad(&polygon, c);

   polygon.vertices[0] = polygon.vertices[4];
   polygon.vertices[1] = polygon.vertices[5];
   polygon.vertices[2] = polygon.vertices[6];
   polygon.vertices[3] = polygon.vertices[7];
   polygon.vertices[4] = (int)((float)X[3] * vdp1wratio);
   polygon.vertices[5] = (int)((float)Y[3] * vdp1hratio);
   polygon.vertices[6] = (int)((float)X[3] * vdp1wratio)+1;
   polygon.vertices[7] = (int)((float)Y[3] * vdp1hratio)+1;
   YglCachedQuad(&polygon, c);

   polygon.vertices[0] = (int)((float)X[0] * vdp1wratio);
   polygon.vertices[1] = (int)((float)Y[0] * vdp1hratio);
   polygon.vertices[2] = (int)((float)X[0] * vdp1wratio)+1;
   polygon.vertices[3] = (int)((float)Y[0] * vdp1hratio)+1;
   YglCachedQuad(&polygon, c);

   if (color & 0x8000)
      *texture.textdata = COLOR_ADD(SAT2YAB1(alpha,color), vdp1cor, vdp1cog, vdp1cob);
   else
      *texture.textdata = COLOR_ADD(Vdp2ColorRamGetColor(color, alpha), vdp1cor, vdp1cog, vdp1cob);
}

//////////////////////////////////////////////////////////////////////////////

void VIDOGLVdp1LineDraw(void)
{
   s16 X[2];
   s16 Y[2];
   u16 color;
   u16 CMDPMOD;
   u8 alpha;
   YglSprite polygon;
   YglTexture texture;

   X[0] = Vdp1Regs->localX + (T1ReadWord(Vdp1Ram, Vdp1Regs->addr + 0x0C));
   Y[0] = Vdp1Regs->localY + (T1ReadWord(Vdp1Ram, Vdp1Regs->addr + 0x0E));
   X[1] = Vdp1Regs->localX + (T1ReadWord(Vdp1Ram, Vdp1Regs->addr + 0x10));
   Y[1] = Vdp1Regs->localY + (T1ReadWord(Vdp1Ram, Vdp1Regs->addr + 0x12));

   color = T1ReadWord(Vdp1Ram, Vdp1Regs->addr + 0x6);
   CMDPMOD = T1ReadWord(Vdp1Ram, Vdp1Regs->addr + 0x4);

   alpha = 0xFF;

   if ((CMDPMOD & 0x7) == 0x3)
      alpha = 0x80;

   polygon.priority = Vdp2Regs->PRISA & 0x7;

   polygon.vertices[0] = (int)((float)X[0] * vdp1wratio);
   polygon.vertices[1] = (int)((float)Y[0] * vdp1hratio);
   polygon.vertices[2] = (int)((float)X[0] * vdp1wratio)+1;
   polygon.vertices[3] = (int)((float)Y[0] * vdp1hratio)+1;
   polygon.vertices[4] = (int)((float)X[1] * vdp1wratio);
   polygon.vertices[5] = (int)((float)Y[1] * vdp1hratio);
   polygon.vertices[6] = (int)((float)X[1] * vdp1wratio)+1;
   polygon.vertices[7] = (int)((float)Y[1] * vdp1hratio)+1;

   polygon.w = 1;
   polygon.h = 1;
   polygon.flip = 0;

   YglQuad(&polygon, &texture);

   if (color & 0x8000)
      *texture.textdata = COLOR_ADD(SAT2YAB1(alpha,color), vdp1cor, vdp1cog, vdp1cob);
   else
      *texture.textdata = COLOR_ADD(Vdp2ColorRamGetColor(color, alpha), vdp1cor, vdp1cog, vdp1cob);
}

//////////////////////////////////////////////////////////////////////////////

void VIDOGLVdp1UserClipping(void)
{
   Vdp1Regs->userclipX1 = T1ReadWord(Vdp1Ram, Vdp1Regs->addr + 0xC);
   Vdp1Regs->userclipY1 = T1ReadWord(Vdp1Ram, Vdp1Regs->addr + 0xE);
   Vdp1Regs->userclipX2 = T1ReadWord(Vdp1Ram, Vdp1Regs->addr + 0x14);
   Vdp1Regs->userclipY2 = T1ReadWord(Vdp1Ram, Vdp1Regs->addr + 0x16);
}

//////////////////////////////////////////////////////////////////////////////

void VIDOGLVdp1SystemClipping(void)
{
   Vdp1Regs->systemclipX1 = 0;
   Vdp1Regs->systemclipY1 = 0;
   Vdp1Regs->systemclipX2 = T1ReadWord(Vdp1Ram, Vdp1Regs->addr + 0x14);
   Vdp1Regs->systemclipY2 = T1ReadWord(Vdp1Ram, Vdp1Regs->addr + 0x16);
}

//////////////////////////////////////////////////////////////////////////////

void VIDOGLVdp1LocalCoordinate(void)
{
   Vdp1Regs->localX = T1ReadWord(Vdp1Ram, Vdp1Regs->addr + 0xC);
   Vdp1Regs->localY = T1ReadWord(Vdp1Ram, Vdp1Regs->addr + 0xE);
}

//////////////////////////////////////////////////////////////////////////////

int VIDOGLVdp2Reset(void)
{
   return 0;
}

//////////////////////////////////////////////////////////////////////////////

void VIDOGLVdp2DrawStart(void)
{
   YglReset();
   YglCacheReset();
}

//////////////////////////////////////////////////////////////////////////////

void VIDOGLVdp2DrawEnd(void)
{
   YglRender();
}

//////////////////////////////////////////////////////////////////////////////

static void Vdp2DrawBackScreen(void)
{
   u32 scrAddr;
   int dot, y;

   if (Vdp2Regs->VRSIZE & 0x8000)
      scrAddr = (((Vdp2Regs->BKTAU & 0x7) << 16) | Vdp2Regs->BKTAL) * 2;
   else
      scrAddr = (((Vdp2Regs->BKTAU & 0x3) << 16) | Vdp2Regs->BKTAL) * 2;

   if (Vdp2Regs->BKTAU & 0x8000)
   {
      glBegin(GL_LINES);

      for(y = 0; y < vdp2height; y++)
      {
         dot = T1ReadWord(Vdp2Ram, scrAddr);
         scrAddr += 2;

         glColor3ub((dot & 0x1F) << 3, (dot & 0x3E0) >> 2, (dot & 0x7C00) >> 7);

         glVertex2f(0, y);
         glVertex2f(vdp2width, y);
      }
      glEnd();
      glColor3ub(0xFF, 0xFF, 0xFF);
   }
   else
   {
      dot = T1ReadWord(Vdp2Ram, scrAddr);

      glColor3ub((dot & 0x1F) << 3, (dot & 0x3E0) >> 2, (dot & 0x7C00) >> 7);

      glBegin(GL_QUADS);
      glVertex2i(0, 0);
      glVertex2i(vdp2width, 0);
      glVertex2i(vdp2width, vdp2height);
      glVertex2i(0, vdp2height);
      glEnd();
      glColor3ub(0xFF, 0xFF, 0xFF);
   }
}

//////////////////////////////////////////////////////////////////////////////

static void Vdp2DrawLineColorScreen(void)
{
}

//////////////////////////////////////////////////////////////////////////////

static void Vdp2DrawNBG0(void)
{
   vdp2draw_struct info;
   YglTexture texture;
   int *tmp;
   int i, i2;
   u32 linescrolladdr;
   vdp2rotationparameter_struct parameter;

   if (Vdp2Regs->BGON & 0x20)
   {
      // RBG1 mode
      info.enable = Vdp2Regs->BGON & 0x20;

      // Read in Parameter B
      Vdp2ReadRotationTable(1, &parameter);

      if((info.isbitmap = Vdp2Regs->CHCTLA & 0x2) != 0)
      {
         // Bitmap Mode

         ReadBitmapSize(&info, Vdp2Regs->CHCTLA >> 2, 0x3);

         info.charaddr = (Vdp2Regs->MPOFR & 0x70) * 0x2000;
         info.paladdr = (Vdp2Regs->BMPNA & 0x7) << 4;
         info.flipfunction = 0;
         info.specialfunction = 0;
      }
      else
      {
         // Tile Mode
         info.mapwh = 4;
         ReadPlaneSize(&info, Vdp2Regs->PLSZ >> 12);
         ReadPatternData(&info, Vdp2Regs->PNCN0, Vdp2Regs->CHCTLA & 0x1);
      }

      info.rotatenum = 1;
      info.PlaneAddr = (FAST_FUNC_PTR(void,)(void *, int))&Vdp2ParameterBPlaneAddr;
      parameter.coefenab = Vdp2Regs->KTCTL & 0x100;
   }
   else if (Vdp2Regs->BGON & 0x1)
   {
      // NBG0 mode
      info.enable = Vdp2Regs->BGON & 0x1;

      if((info.isbitmap = Vdp2Regs->CHCTLA & 0x2) != 0)
      {
         // Bitmap Mode

         ReadBitmapSize(&info, Vdp2Regs->CHCTLA >> 2, 0x3);

         info.x = - ((Vdp2Regs->SCXIN0 & 0x7FF) % info.cellw);
         info.y = - ((Vdp2Regs->SCYIN0 & 0x7FF) % info.cellh);

         info.charaddr = (Vdp2Regs->MPOFN & 0x7) * 0x20000;
         info.paladdr = (Vdp2Regs->BMPNA & 0x7) << 4;
         info.flipfunction = 0;
         info.specialfunction = 0;
      }
      else
      {
         // Tile Mode
         info.mapwh = 2;

         ReadPlaneSize(&info, Vdp2Regs->PLSZ);

         info.x = - ((Vdp2Regs->SCXIN0 & 0x7FF) % (512 * info.planew));
         info.y = - ((Vdp2Regs->SCYIN0 & 0x7FF) % (512 * info.planeh));
         ReadPatternData(&info, Vdp2Regs->PNCN0, Vdp2Regs->CHCTLA & 0x1);
      }

      info.coordincx = (float) 65536 / (Vdp2Regs->ZMXN0.all & 0x7FF00);
      info.coordincy = (float) 65536 / (Vdp2Regs->ZMYN0.all & 0x7FF00);
      info.PlaneAddr = (FAST_FUNC_PTR(void,)(void *, int))&Vdp2NBG0PlaneAddr;
   }
   else
      // Not enabled
      return;

   info.transparencyenable = !(Vdp2Regs->BGON & 0x100);
   info.specialprimode = Vdp2Regs->SFPRMD & 0x3;

   info.colornumber = (Vdp2Regs->CHCTLA & 0x70) >> 4;

   if (Vdp2Regs->CCCTL & 0x1)
      info.alpha = ((~Vdp2Regs->CCRNA & 0x1F) << 3) + 0x7;
   else
      info.alpha = 0xFF;

   info.coloroffset = (Vdp2Regs->CRAOFA & 0x7) << 8;
   ReadVdp2ColorOffset(&info, 0x1);
   info.priority = nbg0priority;

   if (!(info.enable & Vdp2External.disptoggle) || (info.priority == 0))
      return;

   if (info.enable == 1)
   {
      // NBG0 draw
      if (info.isbitmap)
      {
         // Let's check to see if game has set up bitmap to a size smaller than
         // 512x256 using line scroll and vertical cell scroll
         if ((Vdp2Regs->SCRCTL & 0x7) == 0x7)
         {
            linescrolladdr = (Vdp2Regs->LSTA0.all & 0x7FFFE) << 1;

            // Let's first figure out if we have to adjust the vertical offset
            for(i = 0; i < (info.cellh-1); i++)
            {
               if (T1ReadWord(Vdp2Ram, linescrolladdr+((i+1) * 8)) != 0x0000)
               {
                  info.y+=i;

                  // Now let's figure out the height of the bitmap
                  for (i2 = i+1; i2 < info.cellh; i2++)
                  {
                     if (T1ReadLong(Vdp2Ram,linescrolladdr+((i2+1)*8) + 4) == 0)
                        break;
                  }

                  info.cellh = i2-i+1;

                  break;
               }
            }

            // Now let's fetch line 1's line scroll horizontal value, that should
            // be the same as the bitmap width
            info.cellw = T1ReadWord(Vdp2Ram, linescrolladdr+((i+1)*8));
         }

         info.vertices[0] = info.x * info.coordincx;
         info.vertices[1] = info.y * info.coordincy;
         info.vertices[2] = (info.x + info.cellw) * info.coordincx;
         info.vertices[3] = info.y * info.coordincy;
         info.vertices[4] = (info.x + info.cellw) * info.coordincx;
         info.vertices[5] = (info.y + info.cellh) * info.coordincy;
         info.vertices[6] = info.x * info.coordincx;
         info.vertices[7] = (info.y + info.cellh) * info.coordincy;

         tmp = YglQuad((YglSprite *)&info, &texture);
         Vdp2DrawCell(&info, &texture);

         // Handle Scroll Wrapping(Let's see if we even need do to it to begin
         // with)
         if (info.x < (vdp2width - info.cellw))
         {
            info.vertices[0] = (info.x+info.cellw) * info.coordincx;
            info.vertices[2] = (info.x + (info.cellw<<1)) * info.coordincx;
            info.vertices[4] = (info.x + (info.cellw<<1)) * info.coordincx;
            info.vertices[6] = (info.x+info.cellw) * info.coordincx;

            YglCachedQuad((YglSprite *)&info, tmp);

            if (info.y < (vdp2height - info.cellh))
            {
               info.vertices[1] = (info.y+info.cellh) * info.coordincy;
               info.vertices[3] = (info.y + (info.cellh<<1)) * info.coordincy;
               info.vertices[5] = (info.y + (info.cellh<<1)) * info.coordincy;
               info.vertices[7] = (info.y+info.cellh) * info.coordincy;

               YglCachedQuad((YglSprite *)&info, tmp);
            }
         }
         else if (info.y < (vdp2height - info.cellh))
         {
            info.vertices[1] = (info.y+info.cellh) * info.coordincy;
            info.vertices[3] = (info.y + (info.cellh<<1)) * info.coordincy;
            info.vertices[5] = (info.y + (info.cellh<<1)) * info.coordincy;
            info.vertices[7] = (info.y+info.cellh) * info.coordincy;

            YglCachedQuad((YglSprite *)&info, tmp);
         }
      }
      else
      {
         Vdp2DrawMap(&info, &texture);
      }
   }
   else
   {
      // RBG1 draw
      Vdp2DrawRotation(&info, &parameter, &texture);
   }
}

//////////////////////////////////////////////////////////////////////////////

static void Vdp2DrawNBG1(void)
{
   vdp2draw_struct info;
   YglTexture texture;
   int *tmp;

   info.enable = Vdp2Regs->BGON & 0x2;
   info.transparencyenable = !(Vdp2Regs->BGON & 0x200);
   info.specialprimode = (Vdp2Regs->SFPRMD >> 2) & 0x3;

   info.colornumber = (Vdp2Regs->CHCTLA & 0x3000) >> 12;

   if((info.isbitmap = Vdp2Regs->CHCTLA & 0x200) != 0)
   {
      ReadBitmapSize(&info, Vdp2Regs->CHCTLA >> 10, 0x3);

      info.x = - ((Vdp2Regs->SCXIN1 & 0x7FF) % info.cellw);
      info.y = - ((Vdp2Regs->SCYIN1 & 0x7FF) % info.cellh);

      info.charaddr = ((Vdp2Regs->MPOFN & 0x70) >> 4) * 0x20000;
      info.paladdr = (Vdp2Regs->BMPNA & 0x700) >> 4;
      info.flipfunction = 0;
      info.specialfunction = 0;
   }
   else
   {
      info.mapwh = 2;

      ReadPlaneSize(&info, Vdp2Regs->PLSZ >> 2);

      info.x = - ((Vdp2Regs->SCXIN1 & 0x7FF) % (512 * info.planew));
      info.y = - ((Vdp2Regs->SCYIN1 & 0x7FF) % (512 * info.planeh));

      ReadPatternData(&info, Vdp2Regs->PNCN1, Vdp2Regs->CHCTLA & 0x100);
   }

   if (Vdp2Regs->CCCTL & 0x2)
      info.alpha = ((~Vdp2Regs->CCRNA & 0x1F00) >> 5) + 0x7;
   else
      info.alpha = 0xFF;

   info.coloroffset = (Vdp2Regs->CRAOFA & 0x70) << 4;
   ReadVdp2ColorOffset(&info, 0x2);
   info.coordincx = (float) 65536 / (Vdp2Regs->ZMXN1.all & 0x7FF00);
   info.coordincy = (float) 65536 / (Vdp2Regs->ZMXN1.all & 0x7FF00);

   info.priority = nbg1priority;
   info.PlaneAddr = (FAST_FUNC_PTR(void,)(void *, int))&Vdp2NBG1PlaneAddr;

   if (!(info.enable & Vdp2External.disptoggle) || (info.priority == 0))
      return;

   if (info.isbitmap)
   {
      info.vertices[0] = info.x * info.coordincx;
      info.vertices[1] = info.y * info.coordincy;
      info.vertices[2] = (info.x + info.cellw) * info.coordincx;
      info.vertices[3] = info.y * info.coordincy;
      info.vertices[4] = (info.x + info.cellw) * info.coordincx;
      info.vertices[5] = (info.y + info.cellh) * info.coordincy;
      info.vertices[6] = info.x * info.coordincx;
      info.vertices[7] = (info.y + info.cellh) * info.coordincy;

      tmp = YglQuad((YglSprite *)&info, &texture);
      Vdp2DrawCell(&info, &texture);

      // Handle Scroll Wrapping(Let's see if we even need do to it to begin
      // with)
      if (info.x < (vdp2width - info.cellw))
      {
         info.vertices[0] = (info.x+info.cellw) * info.coordincx;
         info.vertices[2] = (info.x + (info.cellw<<1)) * info.coordincx;
         info.vertices[4] = (info.x + (info.cellw<<1)) * info.coordincx;
         info.vertices[6] = (info.x+info.cellw) * info.coordincx;

         YglCachedQuad((YglSprite *)&info, tmp);

         if (info.y < (vdp2height - info.cellh))
         {
            info.vertices[1] = (info.y+info.cellh) * info.coordincy;
            info.vertices[3] = (info.y + (info.cellh<<1)) * info.coordincy;
            info.vertices[5] = (info.y + (info.cellh<<1)) * info.coordincy;
            info.vertices[7] = (info.y+info.cellh) * info.coordincy;

            YglCachedQuad((YglSprite *)&info, tmp);
         }
      }
      else if (info.y < (vdp2height - info.cellh))
      {
         info.vertices[1] = (info.y+info.cellh) * info.coordincy;
         info.vertices[3] = (info.y + (info.cellh<<1)) * info.coordincy;
         info.vertices[5] = (info.y + (info.cellh<<1)) * info.coordincy;
         info.vertices[7] = (info.y+info.cellh) * info.coordincy;

         YglCachedQuad((YglSprite *)&info, tmp);
      }
   }
   else
      Vdp2DrawMap(&info, &texture);
}

//////////////////////////////////////////////////////////////////////////////

static void Vdp2DrawNBG2(void)
{
   vdp2draw_struct info;
   YglTexture texture;

   info.enable = Vdp2Regs->BGON & 0x4;
   info.transparencyenable = !(Vdp2Regs->BGON & 0x400);
   info.specialprimode = (Vdp2Regs->SFPRMD >> 4) & 0x3;

   info.colornumber = (Vdp2Regs->CHCTLB & 0x2) >> 1;
   info.mapwh = 2;

   ReadPlaneSize(&info, Vdp2Regs->PLSZ >> 4);
   info.x = - ((Vdp2Regs->SCXN2 & 0x7FF) % (512 * info.planew));
   info.y = - ((Vdp2Regs->SCYN2 & 0x7FF) % (512 * info.planeh));
   ReadPatternData(&info, Vdp2Regs->PNCN2, Vdp2Regs->CHCTLB & 0x1);

   if (Vdp2Regs->CCCTL & 0x4)
      info.alpha = ((~Vdp2Regs->CCRNB & 0x1F) << 3) + 0x7;
   else
      info.alpha = 0xFF;

   info.coloroffset = Vdp2Regs->CRAOFA & 0x700;
   ReadVdp2ColorOffset(&info, 0x4);
   info.coordincx = info.coordincy = 1;

   info.priority = nbg2priority;
   info.PlaneAddr = (FAST_FUNC_PTR(void,)(void *, int))&Vdp2NBG2PlaneAddr;

   if (!(info.enable & Vdp2External.disptoggle) || (info.priority == 0))
      return;

   Vdp2DrawMap(&info, &texture);
}

//////////////////////////////////////////////////////////////////////////////

static void Vdp2DrawNBG3(void)
{
   vdp2draw_struct info;
   YglTexture texture;

   info.enable = Vdp2Regs->BGON & 0x8;
   info.transparencyenable = !(Vdp2Regs->BGON & 0x800);
   info.specialprimode = (Vdp2Regs->SFPRMD >> 6) & 0x3;

   info.colornumber = (Vdp2Regs->CHCTLB & 0x20) >> 5;

   info.mapwh = 2;

   ReadPlaneSize(&info, Vdp2Regs->PLSZ >> 6);
   info.x = - ((Vdp2Regs->SCXN3 & 0x7FF) % (512 * info.planew));
   info.y = - ((Vdp2Regs->SCYN3 & 0x7FF) % (512 * info.planeh));
   ReadPatternData(&info, Vdp2Regs->PNCN3, Vdp2Regs->CHCTLB & 0x10);

   if (Vdp2Regs->CCCTL & 0x8)
      info.alpha = ((~Vdp2Regs->CCRNB & 0x1F00) >> 5) + 0x7;
   else
      info.alpha = 0xFF;

   info.coloroffset = (Vdp2Regs->CRAOFA & 0x7000) >> 4;
   ReadVdp2ColorOffset(&info, 0x8);
   info.coordincx = info.coordincy = 1;

   info.priority = nbg3priority;
   info.PlaneAddr = (FAST_FUNC_PTR(void,)(void *, int))&Vdp2NBG3PlaneAddr;

   if (!(info.enable & Vdp2External.disptoggle) || (info.priority == 0))
      return;

   Vdp2DrawMap(&info, &texture);
}

//////////////////////////////////////////////////////////////////////////////

static void Vdp2DrawRBG0(void)
{
   vdp2draw_struct info;
   YglTexture texture;
   vdp2rotationparameter_struct parameter;

   info.enable = Vdp2Regs->BGON & 0x10;
   info.priority = rbg0priority;
   if (!(info.enable & Vdp2External.disptoggle) || (info.priority == 0))
      return;
   info.transparencyenable = !(Vdp2Regs->BGON & 0x1000);
   info.specialprimode = (Vdp2Regs->SFPRMD >> 8) & 0x3;

   info.colornumber = (Vdp2Regs->CHCTLB & 0x7000) >> 12;

   // Figure out which Rotation Parameter we're using
   switch (Vdp2Regs->RPMD & 0x3)
   {
      case 0:
         // Parameter A
         info.rotatenum = 0;
         info.PlaneAddr = (FAST_FUNC_PTR(void,)(void *, int))&Vdp2ParameterAPlaneAddr;
         break;
      case 1:
         // Parameter B
         info.rotatenum = 1;
         info.PlaneAddr = (FAST_FUNC_PTR(void,)(void *, int))&Vdp2ParameterBPlaneAddr;
         break;
      case 2:
         // Parameter A+B switched via coefficients
         // FIX ME(need to figure out which Parameter is being used)
      case 3:
      default:
         // Parameter A+B switched via rotation parameter window
         // FIX ME(need to figure out which Parameter is being used)
         VDP2LOG("Rotation Parameter Mode %d not supported!\n", Vdp2Regs->RPMD & 0x3);
         info.rotatenum = 0;
         info.PlaneAddr = (FAST_FUNC_PTR(void,)(void *, int))&Vdp2ParameterAPlaneAddr;
         break;
   }

   Vdp2ReadRotationTable(info.rotatenum, &parameter);

   if((info.isbitmap = Vdp2Regs->CHCTLB & 0x200) != 0)
   {
      // Bitmap Mode
      ReadBitmapSize(&info, Vdp2Regs->CHCTLB >> 10, 0x1);

      if (info.rotatenum == 0)
         // Parameter A
         info.charaddr = (Vdp2Regs->MPOFR & 0x7) * 0x20000;
      else
         // Parameter B
         info.charaddr = (Vdp2Regs->MPOFR & 0x70) * 0x2000;

      info.paladdr = (Vdp2Regs->BMPNB & 0x7) << 4;
      info.flipfunction = 0;
      info.specialfunction = 0;
   }
   else
   {
      // Tile Mode
      info.mapwh = 4;

      if (info.rotatenum == 0)
         // Parameter A
         ReadPlaneSize(&info, Vdp2Regs->PLSZ >> 8);
      else
         // Parameter B
         ReadPlaneSize(&info, Vdp2Regs->PLSZ >> 12);

      ReadPatternData(&info, Vdp2Regs->PNCR, Vdp2Regs->CHCTLB & 0x100);
   }

   if (Vdp2Regs->CCCTL & 0x10)
      info.alpha = ((~Vdp2Regs->CCRR & 0x1F) << 3) + 0x7;
   else
      info.alpha = 0xFF;

   info.coloroffset = (Vdp2Regs->CRAOFB & 0x7) << 8;

   ReadVdp2ColorOffset(&info, 0x10);
   info.coordincx = info.coordincy = 1;
   Vdp2DrawRotation(&info, &parameter, &texture);
}

//////////////////////////////////////////////////////////////////////////////

void VIDOGLVdp2DrawScreens(void)
{
   Vdp2DrawBackScreen();
   Vdp2DrawLineColorScreen();
   Vdp2DrawNBG3();
   Vdp2DrawNBG2();
   Vdp2DrawNBG1();
   Vdp2DrawNBG0();
   Vdp2DrawRBG0();
}

//////////////////////////////////////////////////////////////////////////////

void VIDOGLVdp2SetResolution(u16 TVMD)
{
   int width=0, height=0;
   int wratio=1, hratio=1;

   // Horizontal Resolution
   switch (TVMD & 0x7)
   {
      case 0:
         width = 320;
         wratio = 1;
         break;
      case 1:
         width = 352;
         wratio = 1;
         break;
      case 2:
         width = 640;
         wratio = 2;
         break;
      case 3:
         width = 704;
         wratio = 2;
         break;
      case 4:
         width = 320;
         wratio = 1;
         break;
      case 5:
         width = 352;
         wratio = 1;
         break;
      case 6:
         width = 640;
         wratio = 2;
         break;
      case 7:
         width = 704;
         wratio = 2;
         break;
   }

   // Vertical Resolution
   switch ((TVMD >> 4) & 0x3)
   {
      case 0:
         height = 224;
         break;
      case 1: height = 240;
                 break;
      case 2: height = 256;
                 break;
      default: break;
   }

   hratio = 1;

   // Check for interlace
   switch ((TVMD >> 6) & 0x3)
   {
      case 3: // Double-density Interlace
         height *= 2;
         hratio = 2;
         break;
      case 2: // Single-density Interlace
      case 0: // Non-interlace
      default: break;
   }

   SetSaturnResolution(width, height);
   Vdp1SetTextureRatio(wratio, hratio);
}

//////////////////////////////////////////////////////////////////////////////

void FASTCALL VIDOGLVdp2SetPriorityNBG0(int priority)
{
   nbg0priority = priority;
}

//////////////////////////////////////////////////////////////////////////////

void FASTCALL VIDOGLVdp2SetPriorityNBG1(int priority)
{
   nbg1priority = priority;
}

//////////////////////////////////////////////////////////////////////////////

void FASTCALL VIDOGLVdp2SetPriorityNBG2(int priority)
{
   nbg2priority = priority;
}

//////////////////////////////////////////////////////////////////////////////

void FASTCALL VIDOGLVdp2SetPriorityNBG3(int priority)
{
   nbg3priority = priority;
}

//////////////////////////////////////////////////////////////////////////////

void FASTCALL VIDOGLVdp2SetPriorityRBG0(int priority)
{
   rbg0priority = priority;
}

//////////////////////////////////////////////////////////////////////////////

void YglGetGlSize(int *width, int *height)
{
   *width = GlWidth;
   *height = GlHeight;
}

#endif
