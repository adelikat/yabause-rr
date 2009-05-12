#ifndef AVIOUT_H_INCLUDED
#define AVIOUT_H_INCLUDED

bool DRV_AviBegin(const char* fname, HWND HWnd);
void DRV_AviEnd();
void DRV_AviSoundUpdate(void* soundData, int soundLen);
bool DRV_AviIsRecording();
void DRV_AviVideoUpdate(const u16* buffer);
#endif