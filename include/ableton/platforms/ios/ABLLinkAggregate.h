// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#pragma once

#include "Link.hpp"
#include "settings/ABLSettingsViewController.h"
#include <memory>

extern "C"
{
  using IsEnabledCallback = std::function<void (bool)>;

  struct ABLLinkCallbacks
  {
    ABLLinkCallbacks(
      PeerCountCallback peerCount,
      TempoCallback tempo,
      IsEnabledCallback enabled)
      : mPeerCountCallback(std::move(peerCount))
      , mTempoCallback(std::move(tempo))
      , mIsEnabledCallback(std::move(enabled))
    {
    }

    PeerCountCallback mPeerCountCallback;
    TempoCallback mTempoCallback;
    IsEnabledCallback mIsEnabledCallback;
  };

  struct ABLLink
  {
    ABLLink(double initialBpm, double syncQuantum);

    void updateEnabled();

    std::shared_ptr<ABLLinkCallbacks> mpCallbacks;
    bool mActive;
    std::atomic<bool> mEnabled;
    Clock mClock;
    Link mLink;
    ABLSettingsViewController *mpSettingsViewController;
  };
}
