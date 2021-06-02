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

#include "AudioPlatform.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <thread>
#if defined(LINK_PLATFORM_UNIX)
#include <termios.h>
#endif

#include "LinkC.h"


/* primitive C binding
*/
extern "C"
{
  // create a Link instance
  PABLETONLINK CALLBACK AbLinkCreate(double bpm)
  {
    return new ableton::Link(bpm);
  }
  // destroy a Link instance
  void CALLBACK AbLinkDestroy(PABLETONLINK inst)
  {
    delete static_cast<ableton::Link*>(inst);
  }
  // the rest is an overlay for the BasicLink methods
  int CALLBACK AbLinkIsEnabled(PABLETONLINK inst)
  {
    return static_cast<ableton::Link*>(inst)->isEnabled();
  }
  void CALLBACK AbLinkEnable(PABLETONLINK inst, int bEnable)
  {
    static_cast<ableton::Link*>(inst)->enable(!!bEnable);
  }
  int CALLBACK AbLinkIsStartStopSyncEnabled(PABLETONLINK inst)
  {
    return static_cast<ableton::Link*>(inst)->isStartStopSyncEnabled();
  }
  void CALLBACK AbLinkEnableStartStopSync(PABLETONLINK inst, int bEnable)
  {
    static_cast<ableton::Link*>(inst)->enableStartStopSync(!!bEnable);
  }
  size_t CALLBACK AbLinkNumPeers(PABLETONLINK inst)
  {
    return static_cast<unsigned>(static_cast<ableton::Link*>(inst)->numPeers());
  }
  void CALLBACK AbLinkSetNumPeersCallback(PABLETONLINK inst, PNUMPEERSCALLBACK callback)
  {
    static_cast<ableton::Link*>(inst)->setNumPeersCallback(callback);
  }
  void CALLBACK AbLinkSetTempoCallback(PABLETONLINK inst, PTEMPOCALLBACK callback)
  {
    static_cast<ableton::Link*>(inst)->setTempoCallback(callback);
  }
  void CALLBACK AbLinkSetStartStopCallback(PABLETONLINK inst, PSTARTSTOPCALLBACK callback)
  {
    static_cast<ableton::Link*>(inst)->setStartStopCallback(callback);
  }
  long long CALLBACK AbLinkClockMicros(PABLETONLINK inst)
  {
    return static_cast<ableton::Link*>(inst)->clock().micros().count();
  }
  PABLETONLINKSESSIONSTATE CALLBACK AbLinkCaptureAudioSessionState(PABLETONLINK inst)
  {
    return new ableton::Link::SessionState(static_cast<ableton::Link*>(inst)->captureAudioSessionState());
  }
  void CALLBACK AbLinkCommitAudioSessionState(PABLETONLINK inst, PABLETONLINKSESSIONSTATE state)
  {
    ableton::Link::SessionState* pState = static_cast<ableton::Link::SessionState*>(state);
    static_cast<ableton::Link*>(inst)->commitAudioSessionState(*pState);
  }
  PABLETONLINKSESSIONSTATE CALLBACK AbLinkCaptureAppSessionState(PABLETONLINK inst)
  {
    return new ableton::Link::SessionState(static_cast<ableton::Link*>(inst)->captureAppSessionState());
  }
  void CALLBACK AbLinkCommitAppSessionState(PABLETONLINK inst, PABLETONLINKSESSIONSTATE state)
  {
    ableton::Link::SessionState* pState = static_cast<ableton::Link::SessionState*>(state);
    static_cast<ableton::Link*>(inst)->commitAppSessionState(*pState);
  }
  // this one's a C specialty. If an opaque session state is managed, it needs to be destroyed as well.
  void CALLBACK AbLinkDestroySessionState(PABLETONLINKSESSIONSTATE state)
  {
    delete static_cast<ableton::Link::SessionState*>(state);
  }
  double CALLBACK AbLinkSessionStateTempo(PABLETONLINKSESSIONSTATE state)
  {
    return static_cast<ableton::Link::SessionState*>(state)->tempo();
  }
  void CALLBACK AbLinkSessionStateSetTempo(PABLETONLINKSESSIONSTATE state, double bpm, long long atTime)
  {
    static_cast<ableton::Link::SessionState*>(state)->setTempo(bpm, std::chrono::microseconds(atTime));
  }
  double CALLBACK AbLinkSessionStateBeatAtTime(PABLETONLINKSESSIONSTATE state, long long time, double quantum)
  {
    return static_cast<ableton::Link::SessionState*>(state)->beatAtTime(std::chrono::microseconds(time), quantum);
  }
  double CALLBACK AbLinkSessionStatePhaseAtTime(PABLETONLINKSESSIONSTATE state, long long time, double quantum)
  {
    return static_cast<ableton::Link::SessionState*>(state)->phaseAtTime(std::chrono::microseconds(time), quantum);
  }
  long long CALLBACK AbLinkSessionStateTimeAtBeat(PABLETONLINKSESSIONSTATE state, double beat, double quantum)
  {
    return static_cast<ableton::Link::SessionState*>(state)->timeAtBeat(beat, quantum).count();
  }
  void CALLBACK AbLinkSessionStateRequestBeatAtTime(PABLETONLINKSESSIONSTATE state, double beat, long long time, double quantum)
  {
    static_cast<ableton::Link::SessionState*>(state)->requestBeatAtTime(beat, std::chrono::microseconds(time), quantum);
  }
  void CALLBACK AbLinkSessionStateForceBeatAtTime(PABLETONLINKSESSIONSTATE state, double beat, long long time, double quantum)
  {
    static_cast<ableton::Link::SessionState*>(state)->forceBeatAtTime(beat, std::chrono::microseconds(time), quantum);
  }
  void CALLBACK AbLinkSessionStateSetIsPlaying(PABLETONLINKSESSIONSTATE state, bool isPlaying, long long time)
  {
    static_cast<ableton::Link::SessionState*>(state)->setIsPlaying(isPlaying, std::chrono::microseconds(time));
  }
  int CALLBACK AbLinkSessionStateIsPlaying(PABLETONLINKSESSIONSTATE state)
  {
    return static_cast<ableton::Link::SessionState*>(state)->isPlaying();
  }
  long long CALLBACK AbLinkSessionStateTimeForIsPlaying(PABLETONLINKSESSIONSTATE state)
  {
    return static_cast<ableton::Link::SessionState*>(state)->timeForIsPlaying().count();
  }
  void CALLBACK AbLinkSessionStateRequestBeatAtStartPlayingTime(PABLETONLINKSESSIONSTATE state, double beat, double quantum)
  {
    static_cast<ableton::Link::SessionState*>(state)->requestBeatAtStartPlayingTime(beat, quantum);
  }
  void CALLBACK AbLinkSessionStateSetIsPlayingAndRequestBeatAtTime(PABLETONLINKSESSIONSTATE state, int isPlaying, long long time, double beat, double quantum)
  {
    static_cast<ableton::Link::SessionState*>(state)->setIsPlayingAndRequestBeatAtTime(!!isPlaying, std::chrono::microseconds(time), beat, quantum);
  }

}
