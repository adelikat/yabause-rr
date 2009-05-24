/* movie.cpp
 *
 * Copyright (C) 2006-2009 DeSmuME team
 *
 * This file is part of DeSmuME
 *
 * DeSmuME is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * DeSmuME is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with DeSmuME; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <assert.h>
//#include <limits.h>
#include <fstream>
//#include "utils/guid.h"
#include "utils/xstring.h"
#include "movie.hpp"
extern "C" {
#include "../peripheral.h"
#include "../yabause.h"
#include "../movie.h"
#include "../cs2.h"
#include "../vdp2.h"
#include "../bios.h"
}


//#include "NDSSystem.h"
#include "readwrite.h"

//#include "debug.h"
//#include "rtc.h"
//#include "common.h"

using namespace std;
bool freshMovie = false;	  //True when a movie loads, false when movie is altered.  Used to determine if a movie has been altered since opening
bool autoMovieBackup = true;

#define FCEU_PrintError LOG

#define MOVIE_VERSION 1

//----movie engine main state

EMOVIEMODE movieMode = MOVIEMODE_INACTIVE;

//this should not be set unless we are in MOVIEMODE_RECORD!
fstream* osRecordingMovie = 0;


u32 cur_input_display = 0;
int pauseframe = -1;
bool movie_readonly = true;

char curMovieFilename[512] = {0};
MovieData currMovieData;
int currRerecordCount;

//--------------

const char* Buttons[8] = {"B", "C", "A", "S", "U", "D", "L", "R"};
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

void MovieData::clearRecordRange(int start, int len)
{
	for(int i=0;i<len;i++)
		records[i+start].clear();
}

void MovieData::insertEmpty(int at, int frames)
{
	if(at == -1) 
	{
		int currcount = records.size();
		records.resize(records.size()+frames);
		clearRecordRange(currcount,frames);
	}
	else
	{
		records.insert(records.begin()+at,frames,MovieRecord());
		clearRecordRange(at,frames);
	}
}



void MovieRecord::clear()
{ 
	pad = 0;
	commands = 0;
	touch.padding = 0;
}


//const char MovieRecord::mnemonics[13] = {'R','L','D','U','T','S','B','A','Y','X','W','E','G'};
const char MovieRecord::mnemonics[13] = {'L','R','U','D','S','A','B','C','X','Y','Z','W','E'};
void MovieRecord::dumpPad(std::ostream* os, u16 pad)
{
	//these are mnemonics for each joystick bit.
	//since we usually use the regular joypad, these will be more helpful.
	//but any character other than ' ' or '.' should count as a set bit
	//maybe other input types will need to be encoded another way..
	for(int bit=0;bit<13;bit++)
	{
		int bitmask = (1<<(12-bit));
		char mnemonic = mnemonics[bit];
		//if the bit is set write the mnemonic
		if(pad & bitmask)
			os->put(mnemonic);
		else //otherwise write an unset bit
			os->put('.');
	}
}


void MovieRecord::parsePad(std::istream* is, u16& pad)
{
	char buf[13];
	is->read(buf,13);
	pad = 0;
	for(int i=0;i<13;i++)
	{
		pad <<= 1;
		pad |= ((buf[i]=='.'||buf[i]==' ')?0:1);
	}
}


void MovieRecord::parse(MovieData* md, std::istream* is)
{
	//by the time we get in here, the initial pipe has already been extracted

	//extract the commands
	commands = u32DecFromIstream(is);
	
	is->get(); //eat the pipe

	parsePad(is, pad);
//	touch.x = u32DecFromIstream(is);
//	touch.y = u32DecFromIstream(is);
//	touch.touch = u32DecFromIstream(is);
		
	is->get(); //eat the pipe

	//should be left at a newline
}


void MovieRecord::dump(MovieData* md, std::ostream* os, int index)
{
	//dump the misc commands
	//*os << '|' << setw(1) << (int)commands;
	os->put('|');
	putdec<u8,1,true>(os,commands);

	os->put('|');
	dumpPad(os, pad);
//	putdec<u8,3,true>(os,touch.x); os->put(' ');
//	putdec<u8,3,true>(os,touch.y); os->put(' ');
//	putdec<u8,1,true>(os,touch.touch);
	os->put('|');
	
	//each frame is on a new line
	os->put('\n');
}

MovieData::MovieData()
	: version(MOVIE_VERSION)
	, emuVersion(DESMUME_VERSION_NUMERIC)
	, rerecordCount(1)
	, binaryFlag(false)
	//, greenZoneCount(0)
{
//	memset(&romChecksum,0,sizeof(MD5DATA));
}

void MovieData::truncateAt(int frame)
{
	records.resize(frame);
}


void MovieData::installValue(std::string& key, std::string& val)
{
	//todo - use another config system, or drive this from a little data structure. because this is gross
	if(key == "version")
		installInt(val,version);
	else if(key == "emuVersion")
		installInt(val,emuVersion);
	else if(key == "rerecordCount")
		installInt(val,rerecordCount);
	else if(key == "romFilename")
		romFilename = val;
//	else if(key == "romChecksum")
//		StringToBytes(val,&romChecksum,MD5DATA::size);
//	else if(key == "guid")
//		guid = Desmume_Guid::fromString(val);
//	else if(key == "comment")
//		comments.push_back(mbstowcs(val));
	else if(key == "binary")
		installBool(val,binaryFlag);
	else if(key == "savestate")
	{
		int len = Base64StringToBytesLength(val);
		if(len == -1) len = HexStringToBytesLength(val); // wasn't base64, try hex
		if(len >= 1)
		{
			savestate.resize(len);
			StringToBytes(val,&savestate[0],len); // decodes either base64 or hex
		}
	}
}


int MovieData::dump(std::ostream *os, bool binary)
{
	int start = os->tellp();
	*os << "version " << version << endl;
	*os << "emuVersion " << emuVersion << endl;
	*os << "rerecordCount " << rerecordCount << endl;
	*os << "cdGameName " << cdip->gamename << endl;
	*os << "cdInfo " << cdip->cdinfo << endl;
	*os << "cdItemNum " << cdip->itemnum << endl;
	*os << "cdVersion " << cdip->version << endl;
	*os << "cdDate " << cdip->date << endl;
	*os << "cdRegion " << cdip->region << endl;
	*os << "emulateBios " << yabsys.emulatebios << endl;
	*os << "isPal " << yabsys.IsPal << endl;
#ifdef WIN32
	*os << "sh2CoreType " << int(sh2coretype) << endl;
	*os << "sndCoreType " << int(sndcoretype) << endl;
	*os << "vidCoreType " << int(vidcoretype) << endl;
	*os << "cartType " << carttype << endl;

	char* temp=(char*)malloc(1024);
	strcpy(temp, cdrompath);

	if (strrchr(temp, '/'))
        temp = 1 + strrchr(temp, '/');

	if (strrchr(temp, '\\'))
        temp = 1 + strrchr(temp, '\\');

	*os << "cdRomPath " << temp << endl;

/*
	extern char biosfilename[MAX_PATH];
extern char cdrompath[MAX_PATH];
extern char backupramfilename[MAX_PATH];
extern char mpegromfilename[MAX_PATH];
extern char cartfilename[MAX_PATH];*/
#endif

//	fwrite("YMV", sizeof("YMV"), 1, fp);
//	fwrite(VERSION, sizeof(VERSION), 1, fp);

	////
//	*os << "romChecksum " << BytesToString(romChecksum.data,MD5DATA::size) << endl;
//	*os << "guid " << guid.toString() << endl;

//	for(u32 i=0;i<comments.size();i++)
//		*os << "comment " << wcstombs(comments[i]) << endl;
	
	if(binary)
		*os << "binary 1" << endl;
		
	if(savestate.size() != 0)
		*os << "savestate " << BytesToString(&savestate[0],savestate.size()) << endl;
	if(binary)
	{
		//put one | to start the binary dump
		os->put('|');
		for(int i=0;i<(int)records.size();i++)
			records[i].dumpBinary(this,os,i);
	}
	else
		for(int i=0;i<(int)records.size();i++)
			records[i].dump(this,os,i);

	int end = os->tellp();
	return end-start;
}

//yuck... another custom text parser.
static bool LoadFM2(MovieData& movieData, std::istream* fp, int size, bool stopAfterHeader)
{
	//TODO - start with something different. like 'desmume movie version 1"
	std::ios::pos_type curr = fp->tellg();

	//movie must start with "version 1"
	char buf[9];
	curr = fp->tellg();
	fp->read(buf,9);
	fp->seekg(curr);
	if(fp->fail()) return false;
	if(memcmp(buf,"version 1",9)) 
		return false;

	std::string key,value;
	enum {
		NEWLINE, KEY, SEPARATOR, VALUE, RECORD, COMMENT
	} state = NEWLINE;
	bool bail = false;
	for(;;)
	{
		bool iswhitespace, isrecchar, isnewline;
		int c;
		if(size--<=0) goto bail;
		c = fp->get();
		if(c == -1)
			goto bail;
		iswhitespace = (c==' '||c=='\t');
		isrecchar = (c=='|');
		isnewline = (c==10||c==13);
		if(isrecchar && movieData.binaryFlag && !stopAfterHeader)
		{
			LoadFM2_binarychunk(movieData, fp, size);
			return true;
		}
		switch(state)
		{
		case NEWLINE:
			if(isnewline) goto done;
			if(iswhitespace) goto done;
			if(isrecchar) 
				goto dorecord;
			//must be a key
			key = "";
			value = "";
			goto dokey;
			break;
		case RECORD:
			{
				dorecord:
				if (stopAfterHeader) return true;
				int currcount = movieData.records.size();
				movieData.records.resize(currcount+1);
				int preparse = fp->tellg();
				movieData.records[currcount].parse(&movieData, fp);
				int postparse = fp->tellg();
				size -= (postparse-preparse);
				state = NEWLINE;
				break;
			}

		case KEY:
			dokey: //dookie
			state = KEY;
			if(iswhitespace) goto doseparator;
			if(isnewline) goto commit;
			key += c;
			break;
		case SEPARATOR:
			doseparator:
			state = SEPARATOR;
			if(isnewline) goto commit;
			if(!iswhitespace) goto dovalue;
			break;
		case VALUE:
			dovalue:
			state = VALUE;
			if(isnewline) goto commit;
			value += c;
			break;
		case COMMENT:
		default:
			break;
		}
		goto done;

		bail:
		bail = true;
		if(state == VALUE) goto commit;
		goto done;
		commit:
		movieData.installValue(key,value);
		state = NEWLINE;
		done: ;
		if(bail) break;
	}

	return true;
}


static void closeRecordingMovie()
{
	if(osRecordingMovie)
	{
		delete osRecordingMovie;
		osRecordingMovie = 0;
	}
}

/// Stop movie playback.
static void StopPlayback()
{
	DisplayMessage("Movie playback stopped.");
	strcpy(MovieStatus, "");
	movieMode = MOVIEMODE_INACTIVE;
}


/// Stop movie recording
static void StopRecording()
{
	DisplayMessage("Movie recording stopped.");
	strcpy(MovieStatus, "");
	movieMode = MOVIEMODE_INACTIVE;
	
	closeRecordingMovie();
}



void FCEUI_StopMovie()
{
	if(movieMode == MOVIEMODE_PLAY)
		StopPlayback();
	else if(movieMode == MOVIEMODE_RECORD)
		StopRecording();

	curMovieFilename[0] = 0;
	freshMovie = false;
}


//begin playing an existing movie
void FCEUI_LoadMovie(const char *fname, bool _read_only, bool tasedit, int _pauseframe)
{
	//if(!tasedit && !FCEU_IsValidUI(FCEUI_PLAYMOVIE))
	//	return;

	assert(fname);

	//mbg 6/10/08 - we used to call StopMovie here, but that cleared curMovieFilename and gave us crashes...
	if(movieMode == MOVIEMODE_PLAY)
		StopPlayback();
	else if(movieMode == MOVIEMODE_RECORD)
		StopRecording();
	//--------------

	currMovieData = MovieData();
	
	strcpy(curMovieFilename, fname);
	//FCEUFILE *fp = FCEU_fopen(fname,0,"rb",0);
	//if (!fp) return;
	//if(fp->isArchive() && !_read_only) {
	//	FCEU_PrintError("Cannot open a movie in read+write from an archive.");
	//	return;
	//}

	//LoadFM2(currMovieData, fp->stream, INT_MAX, false);

	
	fstream fs (fname);
	LoadFM2(currMovieData, &fs, INT_MAX, false);
	fs.close();

	//TODO
	//fully reload the game to reinitialize everything before playing any movie
	//poweron(true);

//	extern bool _HACK_DONT_STOPMOVIE;
//	_HACK_DONT_STOPMOVIE = true;
//	NDS_Reset();
	YabauseReset();
//	_HACK_DONT_STOPMOVIE = false;
	////WE NEED TO LOAD A SAVESTATE
	//if(currMovieData.savestate.size() != 0)
	//{
	//	bool success = MovieData::loadSavestateFrom(&currMovieData.savestate);
	//	if(!success) return;
	//}

	currFrameCounter = 0;
	pauseframe = _pauseframe;
	movie_readonly = _read_only;
	movieMode = MOVIEMODE_PLAY;
	currRerecordCount = currMovieData.rerecordCount;
//	InitMovieTime();
//	MovieSRAM();
	BupFormat(0);
	freshMovie = true;
//	ClearAutoHold();

	if(movie_readonly)
		DisplayMessage("Replay started Read-Only.");
	//	driver->USR_InfoMessage("Replay started Read-Only.");
	else
		DisplayMessage("Replay started Read+Write.");
	//	driver->USR_InfoMessage("Replay started Read+Write.");
}

static void openRecordingMovie(const char* fname)
{
	//osRecordingMovie = FCEUD_UTF8_fstream(fname, "wb");
	osRecordingMovie = new fstream(fname,std::ios_base::out);
	/*if(!osRecordingMovie)
		FCEU_PrintError("Error opening movie output file: %s",fname);*/
	strcpy(curMovieFilename, fname);
}


//begin recording a new movie
//TODO - BUG - the record-from-another-savestate doesnt work.
 void FCEUI_SaveMovie(const char *fname, std::wstring author)
{
	//if(!FCEU_IsValidUI(FCEUI_RECORDMOVIE))
	//	return;

	assert(fname);

	FCEUI_StopMovie();

	openRecordingMovie(fname);

	currFrameCounter = 0;
	//LagCounterReset();

	currMovieData = MovieData();
//	currMovieData.guid.newGuid();

	if(author != L"") currMovieData.comments.push_back(L"author " + author);
	//currMovieData.romChecksum = GameInfo->MD5;
	currMovieData.romFilename = cdip->gamename;//GetRomName();
	
//	extern bool _HACK_DONT_STOPMOVIE;
//	_HACK_DONT_STOPMOVIE = true;
//	NDS_Reset();
	YabauseReset();
//	_HACK_DONT_STOPMOVIE = false;

	//todo ?
	//poweron(true);
	//else
	//	MovieData::dumpSavestateTo(&currMovieData.savestate,Z_BEST_COMPRESSION);

	//we are going to go ahead and dump the header. from now on we will only be appending frames
	currMovieData.dump(osRecordingMovie, false);

	movieMode = MOVIEMODE_RECORD;
	movie_readonly = false;
	currRerecordCount = 0;
//	InitMovieTime();
//	MovieSRAM();
	BupFormat(0);

	DisplayMessage("Movie recording started.");
//	driver->USR_InfoMessage("Movie recording started.");
}

 void NDS_setTouchFromMovie(void) {

/*	 if(movieMode == MOVIEMODE_PLAY)
	 {

		 MovieRecord* mr = &currMovieData.records[currFrameCounter];
		 nds.touchX=mr->touch.x << 4;
		 nds.touchY=mr->touch.y << 4;

		 if(mr->touch.touch) {
			 nds.isTouch=mr->touch.touch;
			 MMU.ARM7_REG[0x136] &= 0xBF;
		 }
		 else {
			 nds.touchX=0;
			 nds.touchY=0;
			 nds.isTouch=0;

			 MMU.ARM7_REG[0x136] |= 0x40;
		 }
		 osd->addFixed(mr->touch.x, mr->touch.y, "%s", "X");
	 }*/
 }

 void NDS_setPadFromMovie(u16 pad)
 {
	if(pad & (1 << 11)) PORTDATA1.data[2] &= 0x7F; //r
	else PORTDATA1.data[2] |= ~0x7F; 

	if(pad & (1 << 12)) PORTDATA1.data[2] &= 0xBF; //l
	else PORTDATA1.data[2] |= ~0xBF; 

	if(pad & (1 << 10)) PORTDATA1.data[2] &= 0xDF; //d
	else PORTDATA1.data[2] |= ~0xDF; 

	if(pad & (1 << 9)) PORTDATA1.data[2] &= 0xEF; //up
	else PORTDATA1.data[2] |= ~0xEF; 

	if(pad & (1 << 8)) PORTDATA1.data[2] &= 0xF7; //start
	else PORTDATA1.data[2] |= ~0xF7; 

	if(pad & (1 << 7)) PORTDATA1.data[2] &= 0xFB; //a
	else PORTDATA1.data[2] |= ~0xFB; 

	if(pad & (1 << 6)) PORTDATA1.data[2] &= 0xFE; //b
	else PORTDATA1.data[2] |= ~0xFE; 

	if(pad & (1 << 5)) PORTDATA1.data[2] &= 0xFD; //c
	else PORTDATA1.data[2] |= ~0xFD;

	if(pad & (1 << 4)) PORTDATA1.data[3] &= 0xBF; //x
	else PORTDATA1.data[3] |= ~0xBF;
	
	if(pad & (1 << 3)) PORTDATA1.data[3] &= 0xDF; //y
	else PORTDATA1.data[3] |= ~0xDF;

	if(pad & (1 << 2)) PORTDATA1.data[3] &= 0xEF; //z
	else PORTDATA1.data[3] |= ~0xEF;

	if(pad & (1 << 1)) PORTDATA1.data[3] &= 0xF7; //l
	else PORTDATA1.data[3] |= ~0xF7;

	if(pad & (1 << 0)) PORTDATA1.data[3] &= 0x7F; //r
	else PORTDATA1.data[3] |= ~0x7F;
}

 
 //the main interaction point between the emulator and the movie system.
 //either dumps the current joystick state or loads one state from the movie
extern "C" void FCEUMOV_AddInputState()
 {
	 u16 pad;
	 //todo - for tasedit, either dump or load depending on whether input recording is enabled
	 //or something like that
	 //(input recording is just like standard read+write movie recording with input taken from gamepad)
	 //otherwise, it will come from the tasedit data.

	 if(movieMode == MOVIEMODE_PLAY)
	 {
		 //stop when we run out of frames
		 if(currFrameCounter == (int)currMovieData.records.size())
		 {
			 StopPlayback();
		 }
		 else
		 {

			 strcpy(MovieStatus, "Playback");

			 MovieRecord* mr = &currMovieData.records[currFrameCounter];

			 //reset if necessary
			 if(mr->command_reset())
			 {}
			 //ResetNES();

			 NDS_setPadFromMovie(mr->pad);
			 NDS_setTouchFromMovie();
		 }

		 //if we are on the last frame, then pause the emulator if the player requested it
		 if(currFrameCounter == (int)currMovieData.records.size()-1)
		 {
			 /*if(FCEUD_PauseAfterPlayback())
			 {
			 FCEUI_ToggleEmulationPause();
			 }*/
		 }

		 //pause the movie at a specified frame 
		 //if(FCEUMOV_ShouldPause() && FCEUI_EmulationPaused()==0)
		 //{
		 //	FCEUI_ToggleEmulationPause();
		 //	FCEU_DispMessage("Paused at specified movie frame");
		 //}
//		 osd->addFixed(180, 176, "%s", "Playback");

	 }
	 else if(movieMode == MOVIEMODE_RECORD)
	 {
		 MovieRecord mr;

		 mr.commands = 0;

		 strcpy(MovieStatus, "Recording");

		 ///////////////////////

		 #define FIX(b) (b?1:0)
	int r = FIX(~PORTDATA1.data[2] & (1 << 6));//const char* Buttons[8] = {"B", "C", "A", "S", "U", "D", "R", "L"};
	int l = FIX(~PORTDATA1.data[2] & (1 << 7));//const char* Buttons2[8] = {"", "", "", "L", "Z", "Y", "X", "R"};
	int d = FIX(~PORTDATA1.data[2] & (1 << 5));
	int u = FIX(~PORTDATA1.data[2] & (1 << 4));
	int s = FIX(~PORTDATA1.data[2] & (1 << 3));
	int a = FIX(~PORTDATA1.data[2] & (1 << 2));
	int b = FIX(~PORTDATA1.data[2] & (1 << 0));
	int c = FIX(~PORTDATA1.data[2] & (1 << 1));
	int x = FIX(~PORTDATA1.data[3] & (1 << 6));
	int y = FIX(~PORTDATA1.data[3] & (1 << 5));
	int z = FIX(~PORTDATA1.data[3] & (1 << 4));
	int w = FIX(~PORTDATA1.data[3] & (1 << 3));
	int e = FIX(~PORTDATA1.data[3] & (1 << 7));


	pad =
		(FIX(r)<<12)|
		(FIX(l)<<11)|
		(FIX(d)<<10)|
		(FIX(u)<<9)|
		(FIX(s)<<8)|
		(FIX(a)<<7)|
		(FIX(b)<<6)|
		(FIX(c)<<5)|
		(FIX(x)<<4)|
		(FIX(y)<<3)|
		(FIX(z)<<2)|
		(FIX(w)<<1)|
		(FIX(e)<<0);
	//////////////////////

		 mr.pad = pad;
/*		 if(nds.isTouch) {
			 mr.touch.x = nds.touchX >> 4;
			 mr.touch.y = nds.touchY >> 4;
			 mr.touch.touch = 1;
		 } else {
			 mr.touch.x = 0;
			 mr.touch.y = 0;
			 mr.touch.touch = 0;
		 }*/

		 mr.dump(&currMovieData, osRecordingMovie,currMovieData.records.size());
		 currMovieData.records.push_back(mr);
//		 osd->addFixed(180, 176, "%s", "Recording");
	 }

	 currFrameCounter++;

	 SetInputDisplayCharacters();

	 /*extern u8 joy[4];
	 memcpy(&cur_input_display,joy,4);*/
 }


//TODO 
static void FCEUMOV_AddCommand(int cmd)
{
	// do nothing if not recording a movie
	if(movieMode != MOVIEMODE_RECORD)
		return;
	
	//printf("%d\n",cmd);

	//MBG TODO BIG TODO TODO TODO
	//DoEncode((cmd>>3)&0x3,cmd&0x7,1);
}

//little endian 4-byte cookies
static const int kMOVI = 0x49564F4D;
static const int kNOMO = 0x4F4D4F4E;

void mov_savestate(std::ostream *os)//std::ostream* os
{
	//we are supposed to dump the movie data into the savestate
	//if(movieMode == MOVIEMODE_RECORD || movieMode == MOVIEMODE_PLAY)
	//	return currMovieData.dump(os, true);
	//else return 0;
	if(movieMode == MOVIEMODE_RECORD || movieMode == MOVIEMODE_PLAY)
	{
		write32le(kMOVI,os);
		currMovieData.dump(os, true);
	}
	else
	{
		write32le(kNOMO,os);
	}
}

static bool load_successful;

bool mov_loadstate(std::istream* is, int size)//std::istream* is
{
	load_successful = false;

	int cookie;
	if(read32le(&cookie,is) != 1) return false;
//	if(cookie == kNOMO)
//		return true;
	else if(cookie != kMOVI)
		return false;

	size -= 4;

	if (!movie_readonly && autoMovieBackup && freshMovie) //If auto-backup is on, movie has not been altered this session and the movie is in read+write mode
	{
		FCEUI_MakeBackupMovie(false);	//Backup the movie before the contents get altered, but do not display messages						  
	}

	//a little rule: cant load states in read+write mode with a movie from an archive.
	//so we are going to switch it to readonly mode in that case
//	if(!movie_readonly 
//		//*&& FCEU_isFileInArchive(curMovieFilename)*/
//		) {
//		FCEU_PrintError("Cannot loadstate in Read+Write with movie from archive. Movie is now Read-Only.");
//		movie_readonly = true;
//	}

	MovieData tempMovieData = MovieData();
	std::ios::pos_type curr = is->tellg();
	if(!LoadFM2(tempMovieData, is, size, false)) {
		
	//	is->seekg((u32)curr+size);
	/*	extern bool FCEU_state_loading_old_format;
		if(FCEU_state_loading_old_format) {
			if(movieMode == MOVIEMODE_PLAY || movieMode == MOVIEMODE_RECORD) {
				FCEUI_StopMovie();			
				FCEU_PrintError("You have tried to use an old savestate while playing a movie. This is unsupported (since the old savestate has old-format movie data in it which can't be converted on the fly)");
			}
		}*/
		return false;
	}

	//complex TAS logic for when a savestate is loaded:
	//----------------
	//if we are playing or recording and toggled read-only:
	//  then, the movie we are playing must match the guid of the one stored in the savestate or else error.
	//  the savestate is assumed to be in the same timeline as the current movie.
	//  if the current movie is not long enough to get to the savestate's frame#, then it is an error. 
	//  the movie contained in the savestate will be discarded.
	//  the emulator will be put into play mode.
	//if we are playing or recording and toggled read+write
	//  then, the movie we are playing must match the guid of the one stored in the savestate or else error.
	//  the movie contained in the savestate will be loaded into memory
	//  the frames in the movie after the savestate frame will be discarded
	//  the in-memory movie will have its rerecord count incremented
	//  the in-memory movie will be dumped to disk as fcm.
	//  the emulator will be put into record mode.
	//if we are doing neither:
	//  then, we must discard this movie and just load the savestate


	if(movieMode == MOVIEMODE_PLAY || movieMode == MOVIEMODE_RECORD)
	{
		//handle moviefile mismatch
/*		if(tempMovieData.guid != currMovieData.guid)
		{
			//mbg 8/18/08 - this code  can be used to turn the error message into an OK/CANCEL
			#ifdef WIN32
				//std::string msg = "There is a mismatch between savestate's movie and current movie.\ncurrent: " + currMovieData.guid.toString() + "\nsavestate: " + tempMovieData.guid.toString() + "\n\nThis means that you have loaded a savestate belonging to a different movie than the one you are playing now.\n\nContinue loading this savestate anyway?";
				//extern HWND pwindow;
				//int result = MessageBox(pwindow,msg.c_str(),"Error loading savestate",MB_OKCANCEL);
				//if(result == IDCANCEL)
				//	return false;
			#else
				FCEU_PrintError("Mismatch between savestate's movie and current movie.\ncurrent: %s\nsavestate: %s\n",currMovieData.guid.toString().c_str(),tempMovieData.guid.toString().c_str());
				return false;
			#endif
		}*/

		closeRecordingMovie();

		if(movie_readonly)
		{
			//-------------------------------------------------------------
			//this code would reload the movie from disk. allegedly it is helpful to hexers, but
			//it is way too slow with dsm format. so it is only here as a reminder, and in case someone
			//wants to play with it
			//-------------------------------------------------------------
			//{
			//	fstream fs (curMovieFilename);
			//	if(!LoadFM2(tempMovieData, &fs, INT_MAX, false))
			//	{
			//		FCEU_PrintError("Failed to reload DSM after loading savestate");
			//	}
			//	fs.close();
			//	currMovieData = tempMovieData;
			//}
			//-------------------------------------------------------------

			//if the frame counter is longer than our current movie, then error
			if(currFrameCounter > (int)currMovieData.records.size())
			{
//				FCEU_PrintError("Savestate is from a frame (%d) after the final frame in the movie (%d). This is not permitted.", currFrameCounter, currMovieData.records.size()-1);
				return false;
			}

			movieMode = MOVIEMODE_PLAY;
		}
		else
		{
			//truncate before we copy, just to save some time
			tempMovieData.truncateAt(currFrameCounter);
			currMovieData = tempMovieData;
			
		//	#ifdef _S9XLUA_H
		//	if(!FCEU_LuaRerecordCountSkip())
				currRerecordCount++;
		//	#endif
			
			currMovieData.rerecordCount = currRerecordCount;

			openRecordingMovie(curMovieFilename);
			//printf("DUMPING MOVIE: %d FRAMES\n",currMovieData.records.size());
			currMovieData.dump(osRecordingMovie, false);
			movieMode = MOVIEMODE_RECORD;
		}
	}
	
	load_successful = true;
	freshMovie = false;

	//// Maximus: Show the last input combination entered from the
	//// movie within the state
	//if(current!=0) // <- mz: only if playing or recording a movie
	//	memcpy(&cur_input_display, joop, 4);

	return true;
}

extern "C" void SaveStateMovie(char* filename) {

	strcat (filename, "ymvmovie");
	filebuf fb;
	fb.open (filename, ios::out | ios::binary);//ios::out
	ostream os(&fb);
	mov_savestate(&os);
	fb.close();
}

extern "C" void LoadStateMovie(char* filename) {

	strcat (filename, "ymvmovie");

    FILE * fp = fopen( filename, "r" );
    fseek( fp, 0, SEEK_END );
    int size = ftell(fp);
    fclose(fp);

	filebuf fb;
	fb.open (filename, ios::in | ios::binary);//ios::in
	istream is(&fb);
	mov_loadstate(&is, size);
	fb.close();
}

static void FCEUMOV_PreLoad(void)
{
	load_successful=0;
}

static bool FCEUMOV_PostLoad(void)
{
	if(movieMode == MOVIEMODE_INACTIVE)
		return true;
	else
		return load_successful;
}


bool FCEUI_MovieGetInfo(std::istream* fp, MOVIE_INFO& info, bool skipFrameCount)
{
	//MovieData md;
	//if(!LoadFM2(md, fp, INT_MAX, skipFrameCount))
	//	return false;
	//
	//info.movie_version = md.version;
	//info.poweron = md.savestate.size()==0;
	//info.pal = md.palFlag;
	//info.nosynchack = true;
	//info.num_frames = md.records.size();
	//info.md5_of_rom_used = md.romChecksum;
	//info.emu_version_used = md.emuVersion;
	//info.name_of_rom_used = md.romFilename;
	//info.rerecord_count = md.rerecordCount;
	//info.comments = md.comments;

	return true;
}

bool MovieRecord::parseBinary(MovieData* md, std::istream* is)
{
	commands=is->get();
	is->read((char *) &pad, sizeof pad);
//	is->read((char *) &touch.x, sizeof touch.x);
//	is->read((char *) &touch.y, sizeof touch.y);
//	is->read((char *) &touch.touch, sizeof touch.touch);
	return true;
}


void MovieRecord::dumpBinary(MovieData* md, std::ostream* os, int index)
{
	os->put(md->records[index].commands);
	os->write((char *) &md->records[index].pad, sizeof md->records[index].pad);
//	os->write((char *) &md->records[index].touch.x, sizeof md->records[index].touch.x);
//	os->write((char *) &md->records[index].touch.y, sizeof md->records[index].touch.y);
//	os->write((char *) &md->records[index].touch.touch, sizeof md->records[index].touch.touch);
}

void LoadFM2_binarychunk(MovieData& movieData, std::istream* fp, int size)
{
	int recordsize = 1; //1 for the command

	recordsize = 3;

	assert(size%3==0);

	//find out how much remains in the file
	int curr = fp->tellg();
	fp->seekg(0,std::ios::end);
	int end = fp->tellg();
	int flen = end-curr;
	fp->seekg(curr,std::ios::beg);

#undef min
	//the amount todo is the min of the limiting size we received and the remaining contents of the file
	int todo = std::min(size, flen);

	int numRecords = todo/recordsize;
	//printf("LOADED MOVIE: %d records; currFrameCounter: %d\n",numRecords,currFrameCounter);
	movieData.records.resize(numRecords);
	for(int i=0;i<numRecords;i++)
	{
		movieData.records[i].parseBinary(&movieData,fp);
	}
}

#include <sstream>

static bool CheckFileExists(const char* filename)
{
	//This function simply checks to see if the given filename exists
	string checkFilename; 

	if (filename)
		checkFilename = filename;
			
	//Check if this filename exists
	fstream test;
	test.open(checkFilename.c_str(),fstream::in);
		
	if (test.fail())
	{
		test.close();
		return false; 
	}
	else
	{
		test.close();
		return true; 
	}
}

void FCEUI_MakeBackupMovie(bool dispMessage)
{
	//This function generates backup movie files
	string currentFn;					//Current movie fillename
	string backupFn;					//Target backup filename
	string tempFn;						//temp used in back filename creation
	stringstream stream;
	int x;								//Temp variable for string manip
	bool exist = false;					//Used to test if filename exists
	bool overflow = false;				//Used for special situation when backup numbering exceeds limit

	currentFn = curMovieFilename;		//Get current moviefilename
	backupFn = curMovieFilename;		//Make backup filename the same as current moviefilename
	x = backupFn.find_last_of(".");		 //Find file extension
	backupFn = backupFn.substr(0,x);	//Remove extension
	tempFn = backupFn;					//Store the filename at this point
	for (unsigned int backNum=0;backNum<999;backNum++) //999 = arbituary limit to backup files
	{
		stream.str("");					 //Clear stream
		if (backNum > 99)
			stream << "-" << backNum;	 //assign backNum to stream
		 else if (backNum <= 99 && backNum >= 10)
			stream << "-0" << backNum;      //Make it 010, etc if two digits
		else
			stream << "-00" << backNum;	 //Make it 001, etc if single digit
		backupFn.append(stream.str());	 //add number to bak filename
		backupFn.append(".bak");		 //add extension

		exist = CheckFileExists(backupFn.c_str());	//Check if file exists
		
		if (!exist) 
			break;						//Yeah yeah, I should use a do loop or something
		else
		{
			backupFn = tempFn;			//Before we loop again, reset the filename
			
			if (backNum == 999)			//If 999 exists, we have overflowed, let's handle that
			{
				backupFn.append("-001.bak"); //We are going to simply overwrite 001.bak
				overflow = true;		//Flag that we have exceeded limit
				break;					//Just in case
			}
		}
	}

	MovieData md = currMovieData;								//Get current movie data
	std::fstream* outf = new fstream(backupFn.c_str(),std::ios_base::out); //FCEUD_UTF8_fstream(backupFn, "wb");	//open/create file
	md.dump(outf,false);										//dump movie data
	delete outf;												//clean up, delete file object
	
	//TODO, decide if fstream successfully opened the file and print error message if it doesn't

	if (dispMessage)	//If we should inform the user 
	{
//		if (overflow)
//			FCEUI_DispMessage("Backup overflow, overwriting %s",backupFn.c_str()); //Inform user of overflow
//		else
//			FCEUI_DispMessage("%s created",backupFn.c_str()); //Inform user of backup filename
	}
}

void ToggleReadOnly() {

	movie_readonly ^=1;

	if(movie_readonly==1) DisplayMessage("Movie is now read only.");
	else DisplayMessage("Movie is now read+write.");

}

extern "C" int MovieIsActive() {

	if(movieMode == MOVIEMODE_INACTIVE)
		return false;
	
	return true;

}
