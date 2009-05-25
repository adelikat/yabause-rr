/*  Copyright 2004-2005 Theo Berkau
    Copyright 2005 Guillaume Duhamel

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

#ifndef CS0_H
#define CS0_H

#include "memory.h"

#define CART_NONE               0
#define CART_PAR                1
#define CART_BACKUPRAM4MBIT     2
#define CART_BACKUPRAM8MBIT     3
#define CART_BACKUPRAM16MBIT    4
#define CART_BACKUPRAM32MBIT    5
#define CART_DRAM8MBIT          6
#define CART_DRAM32MBIT         7
#define CART_NETLINK            8
#define CART_ROM16MBIT          9
#define CART_JAPMODEM          10

typedef struct
{
   int carttype;
   int cartid;
   const char *filename;

   FAST_FUNC_PTR(u8,Cs0ReadByte)(u32 addr);
   FAST_FUNC_PTR(u16,Cs0ReadWord)(u32 addr);
   FAST_FUNC_PTR(u32,Cs0ReadLong)(u32 addr);
   FAST_FUNC_PTR(void,Cs0WriteByte)(u32 addr, u8 val);
   FAST_FUNC_PTR(void,Cs0WriteWord)(u32 addr, u16 val);
   FAST_FUNC_PTR(void,Cs0WriteLong)(u32 addr, u32 val);

   FAST_FUNC_PTR(u8,Cs1ReadByte)(u32 addr);
   FAST_FUNC_PTR(u16,Cs1ReadWord)(u32 addr);
   FAST_FUNC_PTR(u32,Cs1ReadLong)(u32 addr);
   FAST_FUNC_PTR(void,Cs1WriteByte)(u32 addr, u8 val);
   FAST_FUNC_PTR(void,Cs1WriteWord)(u32 addr, u16 val);
   FAST_FUNC_PTR(void,Cs1WriteLong)(u32 addr, u32 val);

   FAST_FUNC_PTR(u8,Cs2ReadByte)(u32 addr);
   FAST_FUNC_PTR(u16,Cs2ReadWord)(u32 addr);
   FAST_FUNC_PTR(u32,Cs2ReadLong)(u32 addr);
   FAST_FUNC_PTR(void,Cs2WriteByte)(u32 addr, u8 val);
   FAST_FUNC_PTR(void,Cs2WriteWord)(u32 addr, u16 val);
   FAST_FUNC_PTR(void,Cs2WriteLong)(u32 addr, u32 val);

   void *rom;
   void *bupram;
   void *dram;
} cartridge_struct;

extern cartridge_struct *CartridgeArea;

int CartInit(const char *filename, int);
void CartDeInit(void);

int CartSaveState(FILE *fp);
int CartLoadState(FILE *fp, int version, int size);

#endif
