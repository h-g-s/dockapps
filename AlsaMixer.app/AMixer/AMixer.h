// AMixer.h
//
// Copyright (C) 2004, Petr Hlavka
//
// SPDX-License-Identifier: GPL-2.0+

#ifndef AMIXER_H
#define AMIXER_H

#include "AItem.h"

#include <alsa/asoundlib.h>
#include <string>
#include <vector>

#define MIXER_ITEMS	3


class AItem;

class AMixer {
  private:
    snd_mixer_t *mixerHandle;
    AItem *mixerItems[MIXER_ITEMS];
    std::string cardName;
    std::string itemNames[MIXER_ITEMS];
    int lastEventError;

    void error(const char *errorString, int errorCode);

  public:
    AMixer(const char *card);		// ctor
    ~AMixer();				// dtor

    bool opened();
    int handleEvents();
    AItem *attachItem(unsigned int itemNumber, const char *itemName);
    std::vector<std::string> volumeItemNames();
    bool itemOK(unsigned int itemNumber);
    int itemGetVolume(unsigned int itemNumber);
    void itemSetVolume(unsigned int itemNumber, unsigned int volume);
    int itemIsMuted(unsigned int itemNumber);
    void itemToggleMute(unsigned int itemNumber);
    void reInit();

    static bool mixerElemsChanged();
    static bool mixerChanged();
    static std::string convertIDToCard(const char* cardId);
};

#endif
