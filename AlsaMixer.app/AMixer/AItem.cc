// AItem.cc
//
// Copyright (C) 2004, Petr Hlavka
//
// SPDX-License-Identifier: GPL-2.0+

#include "AItem.h"
#include "AMixer.h"

#include <alsa/asoundlib.h>
#include <vector>

#define ROUND_POS(x)	(long ((long (x) + 0.5 > (x)) ? (x) : (x) + 1))


// prefer playback controls; use capture controls such as "Capture" otherwise
AItem::AItem(AMixer *m, snd_mixer_elem_t *e) {
  mixer = m;
  aElem = e;

  name = snd_mixer_selem_get_name(aElem);

  capture = !snd_mixer_selem_has_playback_volume(aElem) &&
	    snd_mixer_selem_has_capture_volume(aElem);

  if (capture)
    snd_mixer_selem_get_capture_volume_range(aElem, &minVolume, &maxVolume);
  else if (snd_mixer_selem_has_playback_volume(aElem))
    snd_mixer_selem_get_playback_volume_range(aElem, &minVolume, &maxVolume);
  else
    minVolume = maxVolume = 0;

  for (int channel = 0; channel <= (int) SND_MIXER_SCHN_LAST; channel++) {
    bool present = capture ?
      snd_mixer_selem_has_capture_channel(aElem, (SNDCHID_T) channel) :
      snd_mixer_selem_has_playback_channel(aElem, (SNDCHID_T) channel);
    if (present)
      pbChannels.push_back(new AChannel(this, (SNDCHID_T) channel));
  }
}


bool AItem::hasVolume(snd_mixer_elem_t *e) {
  return snd_mixer_selem_has_playback_volume(e) ||
	 snd_mixer_selem_has_capture_volume(e);
}


AItem::~AItem() {
  for (unsigned int i = 0; i < pbChannels.size(); i++)
    delete pbChannels[i];
}


// set same volume for all channels in this item in percent
void AItem::setVolumePerc(unsigned int percent) {
  if (percent > 100)
    percent = 100;

  long volume = (long) ROUND_POS((minVolume + (maxVolume - minVolume) * percent / 100.0));
  if (capture)
    snd_mixer_selem_set_capture_volume_all(aElem, volume);
  else
    snd_mixer_selem_set_playback_volume_all(aElem, volume);
}


// get max channel volume of all channels in this item in percent
unsigned int AItem::getVolumePerc() {
  long max_vol = minVolume, act_vol;

  // find the max volume
  for (unsigned int i = 0; i < pbChannels.size(); i++)
    if ((act_vol = pbChannels[i]->getVolume()) > max_vol)
      max_vol = act_vol;

  // convert it into percent
  if (minVolume != maxVolume)
    return ((unsigned int) ROUND_POS((max_vol - minVolume) * 100.0 / (maxVolume - minVolume)));
  else
    return (0);
}


// in this app side of view, item is muted if all channels is muted
bool AItem::isMuted() {
  for (unsigned int i = 0; i < pbChannels.size(); i++)
    if (!pbChannels[i]->isMuted())
      return (false);

  return (true);
}


// mute all channels
void AItem::mute() {
  if (capture)
    snd_mixer_selem_set_capture_switch_all(aElem, false);
  else
    snd_mixer_selem_set_playback_switch_all(aElem, false);
}


// unmute all channels
void AItem::unmute() {
  if (capture)
    snd_mixer_selem_set_capture_switch_all(aElem, true);
  else
    snd_mixer_selem_set_playback_switch_all(aElem, true);
}
