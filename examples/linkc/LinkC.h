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

#ifndef WIN32
#define CALLBACK
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void* PABLETONLINK;
typedef void* PABLETONLINKSESSIONSTATE;
typedef void (*PNUMPEERSCALLBACK)(size_t);
typedef void (*PTEMPOCALLBACK)(double);
typedef void (*PSTARTSTOPCALLBACK)(int); // this one has to deal with the unfortunate fact that C has no "bool".


  // create a Link instance
PABLETONLINK CALLBACK AbLinkCreate(double bpm);
// destroy a Link instance
void CALLBACK AbLinkDestroy(PABLETONLINK inst);
// the rest is an overlay for the BasicLink methods
int CALLBACK AbLinkIsEnabled(PABLETONLINK inst);
void CALLBACK AbLinkEnable(PABLETONLINK inst, int bEnable);
int CALLBACK AbLinkIsStartStopSyncEnabled(PABLETONLINK inst);
void CALLBACK AbLinkEnableStartStopSync(PABLETONLINK inst, int bEnable);
size_t CALLBACK AbLinkNumPeers(PABLETONLINK inst);
void CALLBACK AbLinkSetNumPeersCallback(PABLETONLINK inst, PNUMPEERSCALLBACK callback);
void CALLBACK AbLinkSetTempoCallback(PABLETONLINK inst, PTEMPOCALLBACK callback);
void CALLBACK AbLinkSetStartStopCallback(PABLETONLINK inst, PSTARTSTOPCALLBACK callback);
long long CALLBACK AbLinkClockMicros(PABLETONLINK inst);
PABLETONLINKSESSIONSTATE CALLBACK AbLinkCaptureAudioSessionState(PABLETONLINK inst);
void CALLBACK AbLinkCommitAudioSessionState(PABLETONLINK inst, PABLETONLINKSESSIONSTATE state);
PABLETONLINKSESSIONSTATE CALLBACK AbLinkCaptureAppSessionState(PABLETONLINK inst);
void CALLBACK AbLinkCommitAppSessionState(PABLETONLINK inst, PABLETONLINKSESSIONSTATE state);
void CALLBACK AbLinkDestroySessionState(PABLETONLINKSESSIONSTATE state);
double CALLBACK AbLinkSessionStateTempo(PABLETONLINKSESSIONSTATE state);
void CALLBACK AbLinkSessionStateSetTempo(PABLETONLINKSESSIONSTATE state, double bpm, long long atTime);
double CALLBACK AbLinkSessionStateBeatAtTime(PABLETONLINKSESSIONSTATE state, long long time, double quantum);
double CALLBACK AbLinkSessionStatePhaseAtTime(PABLETONLINKSESSIONSTATE state, long long time, double quantum);
long long CALLBACK AbLinkSessionStateTimeAtBeat(PABLETONLINKSESSIONSTATE state, double beat, double quantum);
void CALLBACK AbLinkSessionStateRequestBeatAtTime(PABLETONLINKSESSIONSTATE state, double beat, long long time, double quantum);
void CALLBACK AbLinkSessionStateForceBeatAtTime(PABLETONLINKSESSIONSTATE state, double beat, long long time, double quantum);
void CALLBACK AbLinkSessionStateSetIsPlaying(PABLETONLINKSESSIONSTATE state, bool isPlaying, long long time);
int CALLBACK AbLinkSessionStateIsPlaying(PABLETONLINKSESSIONSTATE state);
long long CALLBACK AbLinkSessionStateTimeForIsPlaying(PABLETONLINKSESSIONSTATE state);
void CALLBACK AbLinkSessionStateRequestBeatAtStartPlayingTime(PABLETONLINKSESSIONSTATE state, double beat, double quantum);
void CALLBACK AbLinkSessionStateSetIsPlayingAndRequestBeatAtTime(PABLETONLINKSESSIONSTATE state, int isPlaying, long long time, double beat, double quantum);

#ifdef __cplusplus
}
#endif
