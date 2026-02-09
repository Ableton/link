/* Copyright 2025, Ableton AG, Berlin. All rights reserved.
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

#include <ableton/LinkAudio.hpp>

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

namespace
{

struct State
{
  std::atomic<bool> running;
  ableton::LinkAudio link;
  ableton::linkaudio::AudioPlatform<ableton::LinkAudio> audioPlatform;

  State(std::string name)
    : running(true)
    , link(120., name)
    , audioPlatform(link)
  {
  }
};

void disableBufferedInput()
{
#if defined(LINK_PLATFORM_UNIX)
  termios t;
  tcgetattr(STDIN_FILENO, &t);
  t.c_lflag &= static_cast<unsigned long>(~ICANON);
  tcsetattr(STDIN_FILENO, TCSANOW, &t);
#endif
}

void enableBufferedInput()
{
#if defined(LINK_PLATFORM_UNIX)
  termios t;
  tcgetattr(STDIN_FILENO, &t);
  t.c_lflag |= ICANON;
  tcsetattr(STDIN_FILENO, TCSANOW, &t);
#endif
}

void clearLine()
{
  std::cout << "   \r" << std::flush;
  std::cout.fill(' ');
}

void printHelp()
{
  std::cout << std::endl << " < L I N K  A U D I O  H U T >" << std::endl << std::endl;
  std::cout << "usage:" << std::endl;
  std::cout << "  enable / disable Link: a" << std::endl;
  std::cout << "  start / stop: space" << std::endl;
  std::cout << "  decrease / increase tempo: w / e" << std::endl;
  std::cout << "  decrease / increase quantum: r / t" << std::endl;
  std::cout << "  enable / disable start stop sync: s" << std::endl;
  std::cout << "  enable / disable LinkAudio: c" << std::endl;
  std::cout << "  enable / disable LinkAudio source: o" << std::endl;
  std::cout << "  quit: q" << std::endl << std::endl;
}

void printStateHeader()
{
  std::cout
    << "enabled [au] | num peers | start stop sync | source | tempo  | beats   | metro"
    << std::endl;
}

void printState(const std::chrono::microseconds time,
                const ableton::Link::SessionState sessionState,
                const State& state)
{
  using namespace std;
  const std::string linkEnabled = state.link.isEnabled() ? "yes" : "no";
  const auto linkAudioEnabled = state.link.isLinkAudioEnabled() ? "yes" : "no";
  const auto enabled = linkEnabled + " [" + linkAudioEnabled + "]";
  const auto numPeers = state.link.numPeers();
  const auto quantum = state.audioPlatform.mEngine.quantum();
  const auto beats = sessionState.beatAtTime(time, quantum);
  const auto phase = sessionState.phaseAtTime(time, quantum);
  const auto startStop = state.link.isStartStopSyncEnabled() ? "yes" : "no";
  const auto source =
    state.audioPlatform.mEngine.mLinkAudioRenderer.mpSource ? "yes" : "no";
  const auto isPlaying = sessionState.isPlaying() ? "[playing]" : "[stopped]";
  cout << defaultfloat << left << setw(12) << enabled << " | " << setw(9) << numPeers
       << " | " << setw(3) << startStop << " " << setw(11) << isPlaying << " | " << fixed
       << setw(6) << source << " | " << sessionState.tempo() << " | " << fixed
       << setprecision(2) << setw(7) << beats << " | ";
  for (int i = 0; i < ceil(quantum); ++i)
  {
    if (i < phase)
    {
      std::cout << 'X';
    }
    else
    {
      std::cout << 'O';
    }
  }
  clearLine();
}

void printChannels(const State& state)
{
  clearLine();
  std::cout << "\n\nLinkAudio Peers:" << std::endl;
  for (const auto& channel : state.link.channels())
  {
    std::cout << channel.peerName << " | " << channel.name << std::endl;
  }
  std::cout << std::endl;
  printStateHeader();
}

void input(State& state)
{
  char in;

  for (;;)
  {
#if defined(LINK_PLATFORM_WINDOWS)
    HANDLE stdinHandle = GetStdHandle(STD_INPUT_HANDLE);
    DWORD numCharsRead;
    INPUT_RECORD inputRecord;
    do
    {
      ReadConsoleInput(stdinHandle, &inputRecord, 1, &numCharsRead);
    } while ((inputRecord.EventType != KEY_EVENT) || inputRecord.Event.KeyEvent.bKeyDown);
    in = inputRecord.Event.KeyEvent.uChar.AsciiChar;
#elif defined(LINK_PLATFORM_UNIX)
    in = static_cast<char>(std::cin.get());
#endif

    const auto tempo = state.link.captureAppSessionState().tempo();
    auto& engine = state.audioPlatform.mEngine;

    switch (in)
    {
    case 'q':
      state.running = false;
      clearLine();
      return;
    case 'a':
      state.link.enable(!state.link.isEnabled());
      break;
    case 'w':
      engine.setTempo(tempo - 1);
      break;
    case 'e':
      engine.setTempo(tempo + 1);
      break;
    case 'r':
      engine.setQuantum(engine.quantum() - 1);
      break;
    case 't':
      engine.setQuantum(std::max(1., engine.quantum() + 1));
      break;
    case 's':
      engine.setStartStopSyncEnabled(!engine.isStartStopSyncEnabled());
      break;
    case 'c':
      state.link.enableLinkAudio(!state.link.isLinkAudioEnabled());
      break;
    case 'o':
      state.audioPlatform.mEngine.mLinkAudioRenderer.toggleSource();
      break;
    case ' ':
      if (engine.isPlaying())
      {
        engine.stopPlaying();
      }
      else
      {
        engine.startPlaying();
      }
      break;
    default:
      break;
    }
  }
}

} // namespace

int main(int nargs, char** args)
{
  if (nargs < 2)
  {
    std::cout << "Usage: LinkAudioHut [name]" << std::endl;
    return -1;
  }

  State state(args[1]);
  state.link.setChannelsChangedCallback([&]() { printChannels(state); });

  state.link.callOnLinkThread(ableton::link::platform::HighThreadPriority{});

  printHelp();
  printStateHeader();
  disableBufferedInput();

  std::thread thread(input, std::ref(state));

  while (state.running)
  {
    const auto time = state.link.clock().micros();
    auto sessionState = state.link.captureAppSessionState();
    printState(time, sessionState, state);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  enableBufferedInput();
  thread.join();
  return 0;
}
