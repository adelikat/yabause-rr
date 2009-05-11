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
#include "QtYabause.h"
#include "ui/UIYabause.h"
#include "Settings.h"

#include <QApplication>
#include <QLabel>
#include <QGroupBox>
#include <QTreeWidget>

// cores

#ifdef Q_OS_WIN
extern CDInterface SPTICD;
#endif

M68K_struct * M68KCoreList[] = {
&M68KDummy,
#ifdef HAVE_C68K
&M68KC68K,
#endif
NULL
};

SH2Interface_struct *SH2CoreList[] = {
&SH2Interpreter,
&SH2DebugInterpreter,
NULL
};

PerInterface_struct *PERCoreList[] = {
&PERDummy,
&PERQT,
#ifdef HAVE_LIBSDL
&PERSDLJoy,
#endif
#ifdef __APPLE__
&PERMacJoy,
#endif
NULL
};

CDInterface *CDCoreList[] = {
&DummyCD,
&ISOCD,
&ArchCD,
NULL
};

SoundInterface_struct *SNDCoreList[] = {
&SNDDummy,
#ifdef HAVE_LIBSDL
&SNDSDL,
#endif
#ifdef HAVE_LIBAL
&SNDAL,
#endif
NULL
};

VideoInterface_struct *VIDCoreList[] = {
&VIDDummy,
#ifdef HAVE_LIBGL
&VIDOGL,
#endif
&VIDSoft,
NULL
};

// main window
UIYabause* mUIYabause = 0;
// settings object
Settings* mSettings = 0;
// ports padbits
QMap<uint, PerPad_struct*> mPort1PadsBits;
QMap<uint, PerPad_struct*> mPort2PadsBits;

extern "C" 
{
	void YuiErrorMsg(const char *string)
	{ QtYabause::mainWindow()->appendLog( string ); }

	int YuiSetVideoMode(int /*width*/, int /*height*/, int /*bpp*/, int /*fullscreen*/)
	{ return 0; }

	void YuiSetVideoAttribute(int /*type*/, int /*val*/)
	{}
	
	void YuiSwapBuffers()
	{ QtYabause::mainWindow()->swapBuffers(); }
}

UIYabause* QtYabause::mainWindow()
{
	if ( !mUIYabause )
		mUIYabause = new UIYabause;
	return mUIYabause;
}

Settings* QtYabause::settings()
{
	if ( !mSettings )
		mSettings = new Settings();
	return mSettings;
}

int QtYabause::setTranslationFile()
{
#ifdef HAVE_LIBMINI18N
	if ( mini18n_set_domain( YTSDIR ) == 0 )
	{
		QtYabause::retranslateApplication();
		if ( logTranslation() != 0 )
			qWarning( "Can't log translation !" );
		return 0;
	}

	const QString s = settings()->value( "General/Translation" ).toString();
	if ( s.isEmpty() )
		return 0;
	const char* filePath = qstrdup( s.toLocal8Bit().constData() );
	if ( mini18n_set_locale( filePath ) == 0 )
	{
		QtYabause::retranslateApplication();
		if ( logTranslation() != 0 )
			qWarning( "Can't log translation !" );
		return 0;
	}
#endif
	return -1;
}

int QtYabause::logTranslation()
{
	return 0;
#ifdef HAVE_LIBMINI18N
	const QString s = settings()->value( "General/Translation" ).toString().replace( ".yts", "_log.yts" );
	if ( s.isEmpty() )
		return 0;
	const char* filePath = qstrdup( s.toLocal8Bit().constData() );
	return mini18n_set_log( filePath );
#else
	return 0;
#endif
}

void QtYabause::closeTranslation()
{
#ifdef HAVE_LIBMINI18N
	mini18n_close();
#endif
}

QString QtYabause::translate( const QString& string )
{
#ifdef HAVE_LIBMINI18N
	return QString::fromUtf8( _( string.toUtf8().constData() ) );
#else
	return string;
#endif
}

void QtYabause::retranslateWidget( QWidget* widget )
{
#ifdef HAVE_LIBMINI18N
	if ( !widget )
		return;
	// translate all widget based members
	widget->setAccessibleDescription( translate( widget->accessibleDescription() ) );
	widget->setAccessibleName( translate( widget->accessibleName() ) );
	widget->setStatusTip( translate( widget->statusTip() ) );
	widget->setStyleSheet( translate( widget->styleSheet() ) );
	widget->setToolTip( translate( widget->toolTip() ) );
	widget->setWhatsThis( translate( widget->whatsThis() ) );
	widget->setWindowIconText( translate( widget->windowIconText() ) );
	widget->setWindowTitle( translate( widget->windowTitle() ) );
	// get class name
	const QString className = widget->metaObject()->className();
	if ( className == "QWidget" )
		return;
	else if ( className == "QLabel" )
	{
		QLabel* l = qobject_cast<QLabel*>( widget );
		l->setText( translate( l->text() ) );
	}
	else if ( className == "QAbstractButton" || widget->inherits( "QAbstractButton" ) )
	{
		QAbstractButton* ab = qobject_cast<QAbstractButton*>( widget );
		ab->setText( translate( ab->text() ) );
	}
	else if ( className == "QGroupBox" )
	{
		QGroupBox* gb = qobject_cast<QGroupBox*>( widget );
		gb->setTitle( translate( gb->title() ) );
	}
	else if ( className == "QMenu" || className == "QMenuBar" )
	{
		QList<QMenu*> menus;
		if ( className == "QMenuBar" )
			menus = qobject_cast<QMenuBar*>( widget )->findChildren<QMenu*>();
		else
			menus << qobject_cast<QMenu*>( widget );
		foreach ( QMenu* m, menus )
		{
			m->setTitle( translate( m->title() ) );
			// retranslate menu actions
			foreach ( QAction* a, m->actions() )
			{
				a->setIconText( translate( a->iconText() ) );
				a->setStatusTip( translate( a->statusTip() ) );
				a->setText( translate( a->text() ) );
				a->setToolTip( translate( a->toolTip() ) );
				a->setWhatsThis( translate( a->whatsThis() ) );
			}
		}
	}
	else if ( className == "QTreeWidget" )
	{
		QTreeWidget* tw = qobject_cast<QTreeWidget*>( widget );
		QTreeWidgetItem* twi = tw->headerItem();
		for ( int i = 0; i < twi->columnCount(); i++ )
		{
			twi->setStatusTip( i, translate( twi->statusTip( i ) ) );
			twi->setText( i, translate( twi->text( i ) ) );
			twi->setToolTip( i, translate( twi->toolTip( i ) ) );
			twi->setWhatsThis( i, translate( twi->whatsThis( i ) ) );
		}
	}
	else if ( className == "QTabWidget" )
	{
		QTabWidget* tw = qobject_cast<QTabWidget*>( widget );
		for ( int i = 0; i < tw->count(); i++ )
			tw->setTabText( i, translate( tw->tabText( i ) ) );
	}
	// translate children
	foreach ( QWidget* w, widget->findChildren<QWidget*>() )
		retranslateWidget( w );
#endif
}

void QtYabause::retranslateApplication()
{
#ifdef HAVE_LIBMINI18N
	foreach ( QWidget* widget, QApplication::allWidgets() )
		retranslateWidget( widget );
#endif
}

const char* QtYabause::getCurrentCdSerial()
{ return cdip ? cdip->itemnum : 0; }

M68K_struct* QtYabause::getM68KCore( int id )
{
	for ( int i = 0; M68KCoreList[i] != NULL; i++ )
		if ( M68KCoreList[i]->id == id )
			return M68KCoreList[i];
	return 0;
}

SH2Interface_struct* QtYabause::getSH2Core( int id )
{
	for ( int i = 0; SH2CoreList[i] != NULL; i++ )
		if ( SH2CoreList[i]->id == id )
			return SH2CoreList[i];
	return 0;
}

PerInterface_struct* QtYabause::getPERCore( int id )
{
	for ( int i = 0; PERCoreList[i] != NULL; i++ )
		if ( PERCoreList[i]->id == id )
			return PERCoreList[i];
	return 0;
}

CDInterface* QtYabause::getCDCore( int id )
{
	for ( int i = 0; CDCoreList[i] != NULL; i++ )
		if ( CDCoreList[i]->id == id )
			return CDCoreList[i];
	return 0;
}

SoundInterface_struct* QtYabause::getSNDCore( int id )
{
	for ( int i = 0; SNDCoreList[i] != NULL; i++ )
		if ( SNDCoreList[i]->id == id )
			return SNDCoreList[i];
	return 0;
}

VideoInterface_struct* QtYabause::getVDICore( int id )
{
	for ( int i = 0; VIDCoreList[i] != NULL; i++ )
		if ( VIDCoreList[i]->id == id )
			return VIDCoreList[i];
	return 0;
}

CDInterface QtYabause::defaultCDCore()
{
	return ArchCD;
}

SoundInterface_struct QtYabause::defaultSNDCore()
{
#ifdef HAVE_LIBSDL
	return SNDSDL;
#else
	return SNDDummy;
#endif
}

VideoInterface_struct QtYabause::defaultVIDCore()
{
#if defined HAVE_LIBGL
	return VIDOGL;
#elif defined HAVE_LIBSDL
	return VIDSoft;
#else
	return VIDDummy;
#endif
}

PerInterface_struct QtYabause::defaultPERCore()
{
	return PERQT;
}

SH2Interface_struct QtYabause::defaultSH2Core()
{
	return SH2Interpreter;
}

QMap<uint, PerPad_struct*>* QtYabause::portPadsBits( uint portNumber )
{
	switch ( portNumber )
	{
		case 1:
			return &mPort1PadsBits;
			break;
		case 2:
			return &mPort2PadsBits;
			break;
		default:
			return 0;
			break;
	}
}

void QtYabause::clearPadsBits()
{
	mPort1PadsBits.clear();
	mPort2PadsBits.clear();
}
