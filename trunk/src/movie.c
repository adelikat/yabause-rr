#include "peripheral.h"
#include "scsp.h"
#include "movie.h"
#include "cs2.h"

int RecordingFileOpened;
int PlaybackFileOpened;

struct MovieStruct Movie;

char MovieStatus[40];
int movieLoaded = 0;	//Boolean value, 1 if a movie is playing or recording

//Counting
int framecounter;
int lagframecounter;
int LagFrameFlag;
int FrameAdvanceVariable=0;

int headersize=512;

//////////////////////////////////////////////////////////////////////////////

void ReadHeader(FILE* fp) {

	int x;

	fseek(fp, 0, SEEK_SET);

	fseek(fp, 172, SEEK_SET);
	fread(&Movie.Rerecords, sizeof(Movie.Rerecords), 1, fp);

//	x = fgetc(fp);

//	if(x=1)
	//	Movie.startsfromsavestate=1;

	fseek(fp, headersize, SEEK_SET);
}

//////////////////////////////////////////////////////////////////////////////

void WriteHeader(FILE* fp) {

	int x;

	fseek(fp, 0, SEEK_SET);

	fwrite("YMV", sizeof("YMV"), 1, fp);
	fwrite(VERSION, sizeof(VERSION), 1, fp);
	fwrite(cdip->cdinfo, sizeof(cdip->cdinfo), 1, fp);
	fwrite(cdip->itemnum, sizeof(cdip->itemnum), 1, fp);
	fwrite(cdip->version, sizeof(cdip->version), 1, fp);
	fwrite(cdip->date, sizeof(cdip->date), 1, fp);
	fwrite(cdip->gamename, sizeof(cdip->gamename), 1, fp);
	fwrite(cdip->region, sizeof(cdip->region), 1, fp);
	fwrite(&Movie.Rerecords, sizeof(Movie.Rerecords), 1, fp);

//	fputc(Movie.startsfromsavestate, fp);

	fseek(fp, headersize, SEEK_SET);
}

//////////////////////////////////////////////////////////////////////////////

void ClearInput(void) {

	//do something....
}

//////////////////////////////////////////////////////////////////////////////

const char* Buttons[8] = {"B", "C", "A", "S", "U", "D", "R", "L"};
const char* Spaces[8]  = {" ", " ", " ", " ", " ", " ", " ", " "};
const char* Buttons2[8] = {"", "", "", "L", "Z", "Y", "X", "R"};
const char* Spaces2[8]  = {"", "", "", " ", " ", " ", " ", " "};

char str[40];
char InputDisplayString[40];

void SetInputDisplayCharacters(void) {

	int x;

	strcpy(str, "");

	for (x = 0; x < 8; x++) {

		if(PORTDATA1.data[2] & (1 << x)) {
			strcat(str, Spaces[x]);	
		}
		else
			strcat(str, Buttons[x]);

	}

	for (x = 0; x < 8; x++) {

		if(PORTDATA1.data[3] & (1 << x)) {
			strcat(str, Spaces2[x]);	
		}
		else
			strcat(str, Buttons2[x]);

	}

	strcpy(InputDisplayString, str);
}

//////////////////////////////////////////////////////////////////////////////

void IncrementLagAndFrameCounter(void)
{
	if(LagFrameFlag == 1)
		lagframecounter++;

	framecounter++;
}

//////////////////////////////////////////////////////////////////////////////

int framelength=8;

void DoMovie(void) {

	int x;

	IncrementLagAndFrameCounter();
	LagFrameFlag=1;
	SetInputDisplayCharacters();

	//Read/Write Controller Data
	if(Movie.Status == Recording) {
		for (x = 0; x < 8; x++) {
			fwrite(&PORTDATA1.data[x], 1, 1, Movie.fp);
		}
	}

	if(Movie.Status == Playback) {
		for (x = 0; x < 8; x++) {
			fread(&PORTDATA1.data[x], 1, 1, Movie.fp);
		}

		//if we get to the end of the movie
		if((ftell(Movie.fp)/framelength) >= Movie.Frames) {
			fclose(Movie.fp);
			PlaybackFileOpened=0;
			Movie.Status = Stopped;
			ClearInput();
			strcpy(MovieStatus, "Playback Stopped");
		}
	}

	//Stop Recording/Playback
	if(Movie.Status != Recording && RecordingFileOpened) {
		fclose(Movie.fp);
		RecordingFileOpened=0;
		Movie.Status = Stopped;
		strcpy(MovieStatus, "Recording Stopped");
	}

	if(Movie.Status != Playback && PlaybackFileOpened && Movie.ReadOnly != 0) {
		fclose(Movie.fp);
		PlaybackFileOpened=0;
		Movie.Status = Stopped;
		strcpy(MovieStatus, "Playback Stopped");
	}
}

//////////////////////////////////////////////////////////////////////////////

void MovieLoadState(const char * filename) {


	if (Movie.ReadOnly == 1 && Movie.Status == Playback)  {
		//Movie.Status = Playback;
		fseek (Movie.fp,headersize+(framecounter * framelength),SEEK_SET);
	}

	if(Movie.Status == Recording) {
		fseek (Movie.fp,headersize+(framecounter * framelength),SEEK_SET);
		Movie.Rerecords++;
	}

	if(Movie.Status == Playback && Movie.ReadOnly == 0) {
		Movie.Status = Recording;
		RecordingFileOpened=1;
		strcpy(MovieStatus, "Recording Resumed");
		TruncateMovie(Movie);
		fseek (Movie.fp,headersize+(framecounter * framelength),SEEK_SET);
		Movie.Rerecords++;
	}
}

//////////////////////////////////////////////////////////////////////////////

void TruncateMovie(struct MovieStruct Movie) {

	//when we resume recording, shorten the movie so that there isn't 
	//potential garbage data at the end

	struct MovieBufferStruct tempbuffer;
/*//TODO
	fseek(Movie.fp,0,SEEK_SET);
	tempbuffer=ReadMovieIntoABuffer(Movie.fp);
	fclose(Movie.fp);

	//clear the file and write it again
	Movie.fp=fopen(Movie.filename,"wb");
	fwrite(tempbuffer.data,framelength,framecounter,Movie.fp);
	fclose(Movie.fp);

	Movie.fp=fopen(Movie.filename,"r+b");
*/
}

//////////////////////////////////////////////////////////////////////////////

int MovieGetSize(FILE* fp) {
	int size;
	int fpos;

	fpos = ftell(fp);//save current pos

	fseek (fp,0,SEEK_END);
	size=ftell(fp);

	Movie.Frames=(size-headersize)/ framelength;

	fseek(fp, fpos, SEEK_SET); //reset back to correct pos
	return(size);
}

//////////////////////////////////////////////////////////////////////////////

void MovieToggleReadOnly(void) {

	if(Movie.Status == Playback) {

		if(Movie.ReadOnly == 1) 
		{
			Movie.ReadOnly=0;
			DisplayMessage("Movie is now read+write.");
		}
		else 
		{
			Movie.ReadOnly=1;
			DisplayMessage("Movie is now read only.");
		}
	}
}

//////////////////////////////////////////////////////////////////////////////

void StopMovie(void) {

	if(Movie.Status == Recording && RecordingFileOpened) {
		WriteHeader(Movie.fp);
		fclose(Movie.fp);
		RecordingFileOpened=0;
		Movie.Status = Stopped;
		ClearInput();
		strcpy(MovieStatus, "Recording Stopped");
	}

	if(Movie.Status == Playback && PlaybackFileOpened && Movie.ReadOnly != 0) {
		fclose(Movie.fp);
		PlaybackFileOpened=0;
		Movie.Status = Stopped;
		ClearInput();
		strcpy(MovieStatus, "Playback Stopped");
	}
}

//////////////////////////////////////////////////////////////////////////////

int SaveMovie(const char *filename) {

	char* str=malloc(1024);

	if(Movie.Status == Playback)
		StopMovie();

	if ((Movie.fp = fopen(filename, "w+b")) == NULL)
		return -1;

	strcpy(str, filename);
	Movie.filename=str;
	RecordingFileOpened=1;
	framecounter=0;
	Movie.Status=Recording;
	strcpy(MovieStatus, "Recording Started");
	BupFormat(0);
	Movie.Rerecords=0;
	WriteHeader(Movie.fp);
	YabauseReset();
	return 0;
}

//////////////////////////////////////////////////////////////////////////////

int PlayMovie(const char *filename) {

	char* str=malloc(1024);

	if(Movie.Status == Recording)
		StopMovie();


	if ((Movie.fp = fopen(filename, "r+b")) == NULL)
		return -1;

	strcpy(str, filename);
	Movie.filename=str;
	PlaybackFileOpened=1;
	framecounter=0;
	Movie.ReadOnly = 1;
	Movie.Status=Playback;
	Movie.Size = MovieGetSize(Movie.fp);
	strcpy(MovieStatus, "Playback Started");
	BupFormat(0);
	ReadHeader(Movie.fp);
	YabauseReset();
	return 0;
}

//////////////////////////////////////////////////////////////////////////////

void SaveMovieInState(FILE* fp, IOCheck_struct check) {

	struct MovieBufferStruct tempbuffer;

	fseek(fp, 0, SEEK_END);

	if(Movie.Status == Recording || Movie.Status == Playback) {
		tempbuffer=ReadMovieIntoABuffer(Movie.fp);

		fwrite(&tempbuffer.size, 4, 1, fp);
		fwrite(tempbuffer.data, tempbuffer.size, 1, fp);
	}
}

//////////////////////////////////////////////////////////////////////////////

void MovieReadState(FILE* fp, const char * filename) {

	ReadMovieInState(fp);
	MovieLoadState(filename);//file pointer and truncation

}

void ReadMovieInState(FILE* fp) {

	struct MovieBufferStruct tempbuffer;
	int fpos;

	//overwrite the main movie on disk if we are recording or read+write playback
	if(Movie.Status == Recording || (Movie.Status == Playback && Movie.ReadOnly == 0)) {

		fpos=ftell(fp);//where we are in the savestate
		fread(&tempbuffer.size, 4, 1, fp);//size
		if ((tempbuffer.data = (u8 *)malloc(tempbuffer.size)) == NULL)
		{
			return;
		}
	//	tempbuffer.data = (char*) malloc (sizeof(char)*tempbuffer.size);
		fread(tempbuffer.data, 1, tempbuffer.size, fp);//movie
		fseek(fp, fpos, SEEK_SET);//reset savestate position

		rewind(Movie.fp);
		fwrite(tempbuffer.data, 1, tempbuffer.size, Movie.fp);
		rewind(Movie.fp);
	}
}

//////////////////////////////////////////////////////////////////////////////

struct MovieBufferStruct ReadMovieIntoABuffer(FILE* fp) {

	int fpos;
	struct MovieBufferStruct tempbuffer;

	fpos = ftell(fp);//save current pos

	fseek (fp,0,SEEK_END);
	tempbuffer.size=ftell(fp);  //get size
	rewind(fp);

	tempbuffer.data = (char*) malloc (sizeof(char)*tempbuffer.size);
	fread (tempbuffer.data, 1, tempbuffer.size, fp);

	fseek(fp, fpos, SEEK_SET); //reset back to correct pos
	return(tempbuffer);
}

//////////////////////////////////////////////////////////////////////////////

void MakeMovieStateName(const char *filename) {

	if(Movie.Status == Recording || Movie.Status == Playback)
		strcat ( filename, "movie");

}

//////////////////////////////////////////////////////////////////////////////

//debugging only
void TestWrite(struct MovieBufferStruct tempbuffer) {

	FILE* tempbuffertest;

	tempbuffertest=fopen("rmiab.txt", "wb");
	fwrite (tempbuffer.data, 1, tempbuffer.size, tempbuffertest);
	fclose(tempbuffertest);
}

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

//////////////////////////////////////////////////////////////////////////////

int IsMovieLoaded()
{
	if (RecordingFileOpened || PlaybackFileOpened)
		return 1;
	else
		return 0;
}

//////////////////////////////////////////////////////////////////////////////