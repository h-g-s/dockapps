// AChannel.cc
//
// Copyright (C) 2004, Petr Hlavka
//
// SPDX-License-Identifier: GPL-2.0+

#include "AMixer.h"
#include "AItem.h"
#include "AChannel.h"

AChannel::AChannel(AItem *item, SNDCHID_T cID) {
  aItem = item;
  id = cID;
}


AChannel::~AChannel() {
}


long AChannel::getVolume() {
  long vol = 0;

  if (aItem->isCapture())
    snd_mixer_selem_get_capture_volume(aItem->aElem, id, &vol);
  else
    snd_mixer_selem_get_playback_volume(aItem->aElem, id, &vol);

  return (vol);
}


bool AChannel::isMuted() {
  int val = 1;

  if (aItem->isCapture()) {
    if (!snd_mixer_selem_has_capture_switch(aItem->aElem))
      return (false);
    snd_mixer_selem_get_capture_switch(aItem->aElem, id, &val);
  } else {
    if (!snd_mixer_selem_has_playback_switch(aItem->aElem))
      return (false); /* can't be muted? isn't muted. */
    snd_mixer_selem_get_playback_switch(aItem->aElem, id, &val);
  }

  return (!val);
}
