#!/bin/bash
#
#   Helper script to install Qt5 dependencies on Travis CI.
#

wordsize=$1
if [ "$wordsize" -eq '64' ]; then
  wget http://download.qt.io/online/qtsdkrepository/linux_x64/desktop/qt5_55/qt.55.gcc_64/5.5.1-0qt5_essentials.7z &&
  wget http://download.qt.io/online/qtsdkrepository/linux_x64/desktop/qt5_55/qt.55.gcc_64/5.5.1-0icu-linux-g++-Rhel6.6-x64.7z &&
  wget http://download.qt.io/online/qtsdkrepository/linux_x64/desktop/qt5_55/qt.55.qtquick1.gcc_64/5.5.1-0qt5_qtquick1.7z &&
  wget http://download.qt.io/online/qtsdkrepository/linux_x64/desktop/qt5_55/qt.55.qtwebengine.gcc_64/5.5.1-0qt5_qtwebengine.7z &&
  wget http://download.qt.io/online/qtsdkrepository/linux_x64/desktop/qt5_55/qt.55.gcc_64/5.5.1-0qt5_addons.7z &&
  wget http://download.qt.io/online/qtsdkrepository/linux_x64/desktop/qt5_55/qt.55.qtscript.gcc_64/5.5.1-0qt5_qtscript.7z &&
  wget http://download.qt.io/online/qtsdkrepository/linux_x64/desktop/qt5_55/qt.55.qtlocation.gcc_64/5.5.1-0qt5_qtlocation.7z &&
  wget http://download.qt.io/online/qtsdkrepository/linux_x64/desktop/qt5_55/qt.55.gcc_64/5.5.1-0qt5_qtpositioning.7z &&
  for i in *.7z; do 7z x "$i" > /dev/null; done
elif [ "$wordsize" -eq 32 ]; then
  wget http://download.qt.io/online/qtsdkrepository/linux_x86/desktop/qt5_55/qt.55.gcc/5.5.1-0qt5_essentials.7z &&
  wget http://download.qt.io/online/qtsdkrepository/linux_x86/desktop/qt5_55/qt.55.gcc/5.5.1-0icu-linux-g++-Rhel6.6-x86.7z &&
  wget http://download.qt.io/online/qtsdkrepository/linux_x86/desktop/qt5_55/qt.55.qtquick1.gcc/5.5.1-0qt5_qtquick1.7z &&
  wget http://download.qt.io/online/qtsdkrepository/linux_x86/desktop/qt5_55/qt.55.qtwebengine.gcc/5.5.1-0qt5_qtwebengine.7z &&
  wget http://download.qt.io/online/qtsdkrepository/linux_x86/desktop/qt5_55/qt.55.gcc/5.5.1-0qt5_addons.7z &&
  wget http://download.qt.io/online/qtsdkrepository/linux_x86/desktop/qt5_55/qt.55.qtscript.gcc/5.5.1-0qt5_qtscript.7z &&
  wget http://download.qt.io/online/qtsdkrepository/linux_x86/desktop/qt5_55/qt.55.qtlocation.gcc/5.5.1-0qt5_qtlocation.7z &&
  wget http://download.qt.io/online/qtsdkrepository/linux_x86/desktop/qt5_55/qt.55.gcc/5.5.1-0qt5_qtpositioning.7z &&
  for i in *.7z; do 7z x "$i" > /dev/null; done
else
  echo "usage: ./install-qt-5.sh wordsize"
fi
