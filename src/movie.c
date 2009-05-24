#include "scsp.h"
#include "movie.h"

int LagFrameFlag;
int FrameAdvanceVariable=0;
int currFrameCounter;
char MovieStatus[40];
int lagframecounter;

//////////////////////////////////////////////////////////////////////////////

void PauseOrUnpause(void) {

	if(FrameAdvanceVariable == RunNormal) {
		FrameAdvanceVariable=Paused;
		ScspMuteAudio();
	}
	else {
		FrameAdvanceVariable=RunNormal;	
		ScspUnMuteAudio();
	}
}

void MakeMovieStateName(const char *filename) {

//	if(Movie.Status == Recording || Movie.Status == Playback)
//		strcat ( filename, "movie");
}