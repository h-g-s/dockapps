// AItem.h
//
// Copyright (C) 2004, Petr Hlavka
//
// SPDX-License-Identifier: GPL-2.0+

#ifndef AITEM_H
#define AITEM_H

#include "AMixer.h"
#include "AChannel.h"

#include <alsa/asoundlib.h>
#include <vector>


class AChannel;
class AMixer;

class AItem {
  private:
    AMixer *mixer;			// parent mixer
    std::vector<AChannel *> pbChannels;	// item channels

    long minVolume, maxVolume;		// min/max volume
    bool capture;			// controls capture instead of playback

  public:
    snd_mixer_elem_t *aElem;		// mixer item element
    const char *name;			// item name

    AItem(AMixer *m, snd_mixer_elem_t *e);	// ctor
    static bool hasVolume(snd_mixer_elem_t *e);
    ~AItem();					// dtor

    void setVolumePerc(unsigned int percent);
    unsigned int getVolumePerc();
    bool isCapture() const { return capture; }
    bool isMuted();
    void mute();
    void unmute();
};

#endif
