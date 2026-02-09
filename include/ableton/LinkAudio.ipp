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

#pragma once

namespace ableton
{

template <typename Clock>
inline BasicLinkAudio<Clock>::BasicLinkAudio(const double bpm, std::string name)
  : BasicLink<Clock>(bpm)
{
  setPeerName(name);
}

template <typename Clock>
inline bool BasicLinkAudio<Clock>::isLinkAudioEnabled() const
{
  return this->mController.isLinkAudioEnabled();
}

template <typename Clock>
inline void BasicLinkAudio<Clock>::enableLinkAudio(const bool bEnable)
{
  this->mController.enableLinkAudio(bEnable);
}

template <typename Clock>
inline void BasicLinkAudio<Clock>::setPeerName(std::string name)
{
  this->mController.setName(std::move(name));
}

template <typename Clock>
template <typename Function>
inline void BasicLinkAudio<Clock>::callOnLinkThread(Function func)
{
  this->mController.callOnLinkThread(std::move(func));
}

} // namespace ableton
