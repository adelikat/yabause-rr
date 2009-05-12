#ifndef AVIOUT_H_INCLUDED
#define AVIOUT_H_INCLUDED

void DRV_AviSoundUpdate(void* soundData, int soundLen);
void DRV_AviVideoUpdate(const u16* buffer);
#endif