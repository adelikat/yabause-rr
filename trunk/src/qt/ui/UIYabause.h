/*  Copyright 2005 Guillaume Duhamel
	Copyright 2005-2006 Theo Berkau
	Copyright 2008 Filipe Azevedo <pasnox@gmail.com>

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
#ifndef UIYABAUSE_H
#define UIYABAUSE_H

#include "ui_UIYabause.h"
#include "../YabauseThread.h"

class YabauseGL;
class QTextEdit;
class QDockWidget;

class YabauseLocker
{
public:
	YabauseLocker( YabauseThread* yt/*, bool fr = false*/ )
	{
		Q_ASSERT( yt );
		mThread = yt;
		//mForceRun = fr;
		mRunning = mThread->emulationRunning();
		mPaused = mThread->emulationPaused();
		if ( mRunning && !mPaused )
			mThread->pauseEmulation();
	}
	~YabauseLocker()
	{
		if ( ( mRunning && !mPaused ) /*|| mForceRun*/ )
			mThread->runEmulation();
	}

protected:
	YabauseThread* mThread;
	bool mRunning;
	bool mPaused;
	//bool mForceRun;
};

class UIYabause : public QMainWindow, public Ui::UIYabause
{
	Q_OBJECT
	
public:
	UIYabause( QWidget* parent = 0 );

	void swapBuffers();
	virtual bool eventFilter( QObject* o, QEvent* e );

protected:
	YabauseGL* mYabauseGL;
	YabauseThread* mYabauseThread;
	QDockWidget* mLogDock;
	QTextEdit* teLog;
	bool mInit;

	virtual void showEvent( QShowEvent* event );
	virtual void closeEvent( QCloseEvent* event );
	virtual void keyPressEvent( QKeyEvent* event );
	virtual void keyReleaseEvent( QKeyEvent* event );

public slots:
	void appendLog( const char* msg );

protected slots:
	void sizeRequested( const QSize& size );
	void fullscreenRequested( bool fullscreen );
	void refreshStatesActions();
	// file menu
	void on_aFileSettings_triggered();
	void on_aFileOpenISO_triggered();
	void on_aFileOpenCDRom_triggered();
	void on_mFileSaveState_triggered( QAction* );
	void on_mFileLoadState_triggered( QAction* );
	void on_aFileSaveStateAs_triggered();
	void on_aFileLoadStateAs_triggered();
	void on_aFileScreenshot_triggered();
	void on_aFileQuit_triggered();
	// emulation menu
	void on_aEmulationRun_triggered();
	void on_aEmulationPause_triggered();
	void on_aEmulationReset_triggered();
	void on_aEmulationFrameSkipLimiter_toggled( bool toggled );
	// tools
	void on_aToolsBackupManager_triggered();
	void on_aToolsCheatsList_triggered();
	void on_aToolsTransfer_triggered();
	// view menu
	void on_aViewFPS_triggered();
	void on_aViewLayerVdp1_triggered();
	void on_aViewLayerNBG0_triggered();
	void on_aViewLayerNBG1_triggered();
	void on_aViewLayerNBG2_triggered();
	void on_aViewLayerNBG3_triggered();
	void on_aViewLayerRBG0_triggered();
	void on_aViewFullscreen_triggered( bool b );
	// help menu
	void on_aHelpEmuCompatibility_triggered();
	void on_aHelpAbout_triggered();
	// toolbar
	void on_aSound_triggered();
	void on_aVideoDriver_triggered();
	void on_cbSound_toggled( bool toggled );
	void on_sVolume_valueChanged( int value );
	void on_cbVideoDriver_currentIndexChanged( int id );
};

#endif // UIYABAUSE_H
