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

#include "Controller.hpp"

namespace ableton
{
namespace qlinkhut
{

Controller::Controller()
  : mTempo(120)
  , mLink(mTempo)
  , mAudioPlatform(mLink)
{
  mLink.setNumPeersCallback([this](std::size_t) { Q_EMIT onNumberOfPeersChanged(); });
  mLink.setTempoCallback([this](const double bpm) {
    mTempo = bpm;
    Q_EMIT onTempoChanged();
  });
}

void Controller::setIsPlaying(bool isPlaying)
{
  if (isPlaying)
  {
    mAudioPlatform.mEngine.startPlaying();
  }
  else
  {
    mAudioPlatform.mEngine.stopPlaying();
  }
}

bool Controller::isPlaying()
{
  return mAudioPlatform.mEngine.isPlaying();
}

void Controller::setTempo(double bpm)
{
  mAudioPlatform.mEngine.setTempo(bpm);
}

double Controller::tempo()
{
  return mTempo;
}

double Controller::quantum()
{
  return mAudioPlatform.mEngine.quantum();
}

void Controller::setQuantum(const double quantum)
{
  mAudioPlatform.mEngine.setQuantum(quantum);
  Q_EMIT onQuantumChanged();
}

unsigned long Controller::numberOfPeers()
{
  return static_cast<unsigned long>(mLink.numPeers());
}

void Controller::setLinkEnabled(const bool isEnabled)
{
  mLink.enable(isEnabled);
  Q_EMIT onIsLinkEnabledChanged();
}

bool Controller::isLinkEnabled()
{
  return mLink.isEnabled();
}

void Controller::setStartStopSyncEnabled(const bool isEnabled)
{
  mAudioPlatform.mEngine.setStartStopSyncEnabled(isEnabled);
  Q_EMIT onIsStartStopSyncEnabledChanged();
}

bool Controller::isStartStopSyncEnabled()
{
  return mAudioPlatform.mEngine.isStartStopSyncEnabled();
}

double Controller::beatTime()
{
  return mAudioPlatform.mEngine.beatTime();
}

} // namespace qlinkhut
} // namespace ableton
