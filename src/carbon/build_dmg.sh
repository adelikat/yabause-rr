#   Copyright 2006 Guillaume Duhamel
#   Copyright 2006 Lawrence Sebald
#   Copyright 2007 Anders Montonen
#
#   This file is part of Yabause.
#
#   Yabause is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
#   Yabause is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with Yabause; if not, write to the Free Software
#   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA


hdiutil create -megabytes 20 tmp.dmg -layout NONE -fs HFS+ -volname Yabause -ov
tmp=`hdid tmp.dmg`
disk=`echo $tmp | cut -f 1 -d\ `
cp -R Yabause.app /Volumes/Yabause/
cp ../../ChangeLog /Volumes/Yabause/
cp ../../README /Volumes/Yabause/
cp ../../README.MAC /Volumes/Yabause/
cp ../../AUTHORS /Volumes/Yabause/
cp ../../COPYING /Volumes/Yabause/
hdiutil eject $disk
hdiutil convert -format UDZO tmp.dmg -o Yabause.dmg
rm tmp.dmg
