/*  
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

#ifndef MOVIE_H
#define MOVIE_H

#define RunNormal   0
#define Paused      1
#define NeedAdvance 2

void PauseOrUnpause(void);
void MakeMovieStateName(const char *filename);
char* GetMovieLengthStr();

extern int FrameAdvanceVariable;
extern int LagFrameCounter;
extern int LagFrameFlag;

extern char MovieStatus[40];
extern char InputDisplayString[40];
extern char MovieStatus[40];

#ifdef __cplusplus
extern "C" {
#endif
extern int currFrameCounter;
#ifdef __cplusplus
}
#endif

#endif