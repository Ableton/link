/* Copyright 2016, Ableton AG, Berlin. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  If you would like to incorporate Link into a proprietary software application,
 *  please contact <link-devs@ableton.com>.
 */

#pragma once

#include "AudioPlatform.hpp"
#include <QtCore/QObject>

namespace ableton
{
namespace qlinkhut
{

class Controller : public QObject
{
  Q_OBJECT
  Q_DISABLE_COPY(Controller)

public:
  Controller();

  void setIsPlaying(bool);
  bool isPlaying();
  Q_SIGNAL void onIsPlayingChanged();
  Q_PROPERTY(bool isPlaying READ isPlaying WRITE setIsPlaying)

  void setTempo(double);
  double tempo();
  Q_SIGNAL void onTempoChanged();
  Q_PROPERTY(double tempo READ tempo WRITE setTempo NOTIFY onTempoChanged)

  void setQuantum(double quantum);
  double quantum();
  Q_SIGNAL void onQuantumChanged();
  Q_PROPERTY(double quantum READ quantum WRITE setQuantum NOTIFY onQuantumChanged)

  unsigned long numberOfPeers();
  Q_SIGNAL void onNumberOfPeersChanged();
  Q_PROPERTY(unsigned long numberOfPeers READ numberOfPeers NOTIFY onNumberOfPeersChanged)

  void setLinkEnabled(bool isEnabled);
  bool isLinkEnabled();
  Q_SIGNAL void onIsLinkEnabledChanged();
  Q_PROPERTY(bool isLinkEnabled WRITE setLinkEnabled READ isLinkEnabled NOTIFY
      onIsLinkEnabledChanged)

  void setStartStopSyncEnabled(bool isEnabled);
  bool isStartStopSyncEnabled();
  Q_SIGNAL void onIsStartStopSyncEnabledChanged();
  Q_PROPERTY(bool isStartStopSyncEnabled WRITE setStartStopSyncEnabled READ
      isStartStopSyncEnabled NOTIFY onIsStartStopSyncEnabledChanged)

  Q_INVOKABLE double beatTime();

private:
  double mTempo;
  Link mLink;
  linkaudio::AudioPlatform mAudioPlatform;
};

} // namespace qlinkhut
} // namespace ableton
