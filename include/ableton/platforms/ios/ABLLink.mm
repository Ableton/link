// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#include "ABLLink.h"
#include "ABLLinkPrivate.h"
#include "ABLLinkAggregate.h"
#include "ABLLinkUtils.h"
#include <ableton/util/Injected.hpp>
#include "notifications/ABLNotificationView.h"
#include "settings/ABLSettingsViewController.h"
#include <dispatch/dispatch.h>


extern "C"
{
  ABLLink::ABLLink(const double initialBpm, const double quantum)
    : mpCallbacks(
        std::make_shared<ABLLinkCallbacks>(
          [](std::size_t) { }, [](ableton::link::Tempo) { }, [](bool) { }))
    , mActive(true)
    , mEnabled(false)
    , mClock{}
    , mLink(
        ableton::link::Tempo{initialBpm}, ableton::link::Beats{quantum},
        [this] (const std::size_t numPeers) {
          auto pCallbacks = mpCallbacks;
          dispatch_async(dispatch_get_main_queue(), ^{
              pCallbacks->mPeerCountCallback(numPeers);
            });
        },
        [this] (const ableton::link::Tempo tempo) {
          auto pCallbacks = mpCallbacks;
          dispatch_async(dispatch_get_main_queue(), ^{
              pCallbacks->mTempoCallback(tempo);
            });
        },
        mClock, Platform{}, ableton::util::injectVal(Log{}),
        ableton::util::injectUnique(std::unique_ptr<ableton::util::AsioService>(
          new ableton::util::AsioService{})))
  {
    mpSettingsViewController = [[ABLSettingsViewController alloc] initWithLink:this];
  }

  void ABLLink::updateEnabled()
  {
    mLink.enable(mActive && mEnabled);
  }

  // ABLLink API

  ABLLinkRef ABLLinkNew(const double bpm, const double quantum)
  {
    ABLLink* ablLink = new ABLLink(bpm, quantum);
    // Install notification callback
    ablLink->mpCallbacks->mPeerCountCallback = [ablLink](const std::size_t peers) {
      if(ablLink->mLink.isEnabled())
      {
        ABLLinkNotificationsShowMessage(ABLLINK_NOTIFY_CONNECTED, peers);
        [ablLink->mpSettingsViewController setNumberOfPeers:peers];
      }
    };
    return ablLink;
  }

  void ABLLinkDelete(ABLLinkRef ablLink)
  {
    [ablLink->mpSettingsViewController deinit];

    // clear all callbacks before deletion so that they won't be
    // invoked during or after destruction of the library
    ablLink->mpCallbacks->mPeerCountCallback = [](std::size_t) { };
    ablLink->mpCallbacks->mTempoCallback = [](ableton::link::Tempo) { };
    ablLink->mpCallbacks->mIsEnabledCallback = [](bool) { };

    delete ablLink;
  }

  void ABLLinkSetActive(ABLLinkRef ablLink, const bool active)
  {
    ablLink->mActive = active;
    ablLink->updateEnabled();
  }

  bool ABLLinkIsEnabled(ABLLinkRef ablLink)
  {
    return ablLink->mEnabled;
  }

  bool ABLLinkIsConnected(ABLLinkRef ablLink)
  {
    return ablLink->mLink.isEnabled() && ablLink->mLink.numPeers() > 0;
  }

  void ABLLinkSetSessionTempoCallback(
    ABLLinkRef ablLink,
    ABLLinkSessionTempoCallback callback,
    void* context)
  {
    ablLink->mpCallbacks->mTempoCallback = [=](const ableton::link::Tempo sessionTempo) {
      callback(bpm(sessionTempo), context);
    };
  }

  void ABLLinkSetIsEnabledCallback(
    ABLLinkRef ablLink,
    ABLLinkIsEnabledCallback callback,
    void* context)
  {
    ablLink->mpCallbacks->mIsEnabledCallback = [=](const bool isEnabled) {
      callback(isEnabled, context);
    };
  }

  void ABLLinkProposeTempo(
    ABLLinkRef ablLink,
    const double bpm,
    const std::uint64_t hostTimeAtOutput)
  {
    ablLink->mLink.setTempo(
      ableton::link::Tempo{bpm},
      ticksToMicros(ablLink->mClock, hostTimeAtOutput));
  }

  double ABLLinkGetSessionTempo(ABLLinkRef ablLink)
  {
    return bpm(tempo(ablLink->mLink));
  }

  double ABLLinkBeatTimeAtHostTime(ABLLinkRef ablLink, const std::uint64_t hostTime)
  {
    return floating(
      timeToBeats(ablLink->mLink, ticksToMicros(ablLink->mClock, hostTime)));
  }

  std::uint64_t ABLLinkHostTimeAtBeatTime(ABLLinkRef ablLink, const double beatTime)
  {
    return microsToTicks(
      ablLink->mClock, beatsToTime(ablLink->mLink, ableton::link::Beats{beatTime}));
  }

  double ABLLinkResetBeatTime(
    ABLLinkRef ablLink,
    const double beatTime,
    const std::uint64_t hostTime)
  {
    return floating(
      resetBeats(
        ablLink->mLink,
        ableton::link::Beats{beatTime},
        ticksToMicros(ablLink->mClock, hostTime)));
  }

  void ABLLinkForceBeatTime(
    ABLLinkRef ablLink,
    const double beatTime,
    const std::uint64_t hostTime,
    const double bpm)
  {
    forceBeats(
      ablLink->mLink,
      ableton::link::Tempo{bpm},
      ableton::link::Beats{beatTime},
      ticksToMicros(ablLink->mClock, hostTime));
  }

  void ABLLinkSetQuantum(ABLLinkRef ablLink, const double quantum)
  {
    ablLink->mLink.setQuantum(ableton::link::Beats{quantum});
  }

  double ABLLinkGetQuantum(ABLLinkRef ablLink)
  {
    return floating(ablLink->mLink.quantum());
  }

  double ABLLinkPhase(ABLLinkRef ablLink, double beatTime, double quantum)
  {
    return floating(
      phase(
        ablLink->mLink,
        ableton::link::Beats{beatTime},
        ableton::link::Beats{quantum}));
  }

} // extern "C"
