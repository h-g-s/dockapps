//
//  Mixer.app / AlsaMixer.app
//
//  Copyright (c) 1998-2002 Per Liden
//  Copyright (C) 2004, Petr Hlavka
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1307,
//  USA.
//

#include <X11/Xlib.h>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <string>
#include <vector>
#include <unistd.h>
#include "Xpm.h"
#include "Mixer.h"

#include "AMixer/AMixer.h"

#include "pixmaps/main.xpm"
#include "pixmaps/button.xpm"
#include "pixmaps/mutebutton.xpm"
#include "pixmaps/redlight.xpm"

#define ROUND_POS(x)	(int ((x) + 0.5) > int (x)) ? (int) ((x) + 1) : (int) (x)

using namespace std;

static const int ButtonX[] = {6, 24, 42};
static const char* DefaultMixerSources[] = { "Master", "PCM", "CD" };
static std::string MixerSources[] = { "Master", "PCM", "CD" };
static bool MixerSourceSet[] = { false, false, false };
static volatile sig_atomic_t shutdownRequested = 0;

static void requestShutdown(int)
{
   shutdownRequested = 1;
}

int positionToPercent(int position) {
  return ROUND_POS(100 - (((position - BUTTON_MAX) * 100.0) / (BUTTON_MIN - BUTTON_MAX)));
}

int percentToPosition(int percent) {
  return ROUND_POS(BUTTON_MIN - (percent * (BUTTON_MIN - BUTTON_MAX)) / 100.0);
}

Mixer::Mixer(int argc, char** argv)
   : mDisplay(NULL), mRoot(None), mAppWin(None), mIconWin(None), mButton{},
     mDeleteWindow(None), mMixerDevice("default"),
     mInstanceName(INSTANCENAME), mLabelText(), mSettingsFile(), mCommand(),
     mError(0), mVolume{}, mVolumePos{},
     mVolumeMute{}, mWheelButton(1), mSaveSettings(false),
     mLoadSettings(false), aMixer(NULL)
{
   XClassHint classHint;
   XSizeHints sizeHints;
   XWMHints   wmHints;
   Xpm*       image;
   char*      displayName = NULL;
   string card = "default";

   // Parse command line
   if (argc>1) {
      for (int i=1; i<argc; i++) {
         // Display
         if (!strcmp(argv[i], "-d")) {
            checkArgument(argv, argc, i);
            displayName = argv[i+1];
            i++;
         }

         // Sound source
         else if (!strcmp(argv[i], "-1") || !strcmp(argv[i], "-2") || !strcmp(argv[i], "-3")) {
            checkArgument(argv, argc, i);
	    MixerSources[argv[i][1] - '1'] = argv[i + 1];
	    MixerSourceSet[argv[i][1] - '1'] = true;
	    i++;
         }

         // Wheel binding
         else if (!strcmp(argv[i], "-w")) {
            checkArgument(argv, argc, i);
            mWheelButton = atoi(argv[i+1]);

            if (mWheelButton < 1 || mWheelButton > 3) {
               cerr << APPNAME << ": invalid wheel binding, must be 1, 2 or 3, not " << argv[i+1] << endl;
               tryHelp(argv[0]);
               exit(EXIT_FAILURE);
            }

            i++;
         }

         // Label text
         else if (!strcmp(argv[i], "-l")) {
            checkArgument(argv, argc, i);
            mLabelText = argv[i+1];
            i++;
         }

         // Save settings on exit
         else if (!strcmp(argv[i], "-S")) {
	    mSaveSettings = true;
         }

	 // Load settings on startup
	 else if (!strcmp(argv[i], "-L")) {
	    mLoadSettings = true;
	 }

         // Load/Save settings file
         else if (!strcmp(argv[i], "-f")) {
            checkArgument(argv, argc, i);
            mSettingsFile = argv[i+1];
            i++;
         }

	 // Execute command on middle click
         else if (!strcmp(argv[i], "-e")) {
           checkArgument(argv, argc, i);
	   mCommand = argv[i + 1];
	   i++;
	 }

         // Instance name
         else if (!strcmp(argv[i], "-n")) {
            checkArgument(argv, argc, i);
            mInstanceName = argv[i+1];
            i++;
         }

         // Version
         else if (!strcmp(argv[i], "-v")) {
            cerr << APPNAME << " version " << VERSION << endl;
            exit(0);
         }

         // Help
         else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            showHelp();
            exit(0);
         }

	 // card
	 else if (!strcmp(argv[i], "--card")) {
	   checkArgument(argv, argc, i);
	   card = AMixer::convertIDToCard(argv[i + 1]);
	   if (card.empty()) {
	     cerr << APPNAME << ": invalid card number '" << argv[i + 1] << "'" << endl;
	     tryHelp(argv[0]);
	     exit(EXIT_FAILURE);
	   }
	   i++;
	 }

	 // device
	 else if (!strcmp(argv[i], "--device")) {
	   checkArgument(argv, argc, i);
	   card = argv[i + 1];
	   mMixerDevice = card;
	   i++;
	 }

         // Unknown option
         else {
            cerr << APPNAME << ": invalid option '" << argv[i] << "'" << endl;
            tryHelp(argv[0]);
            exit(EXIT_FAILURE);
         }
      }
   }

   // default settings file
   if (mSettingsFile.empty()) {
     char* home = getenv("HOME");
     if (home) {
       mSettingsFile = string(home) + SETTINGS;
     } else {
       cerr << APPNAME << ": $HOME not set, could not find saved settings" << endl;
     }
   }

   // init mixer
   mMixerDevice = card;
   aMixer = new AMixer(card.c_str());
   if (!aMixer->opened()) {
     cerr << APPNAME << ": could not open mixer device for card '" << card << "'" << endl;
     exit(EXIT_FAILURE);
   }

   selectSources(true);
   if (!aMixer->itemOK(0) && !aMixer->itemOK(1) && !aMixer->itemOK(2))
     cerr << APPNAME << ": no usable mixer controls found on '" << card << "'" << endl;

   // Open display
   if ((mDisplay = XOpenDisplay(displayName)) == NULL) {
      cerr << APPNAME << ": could not open display " << displayName << endl;
      exit(EXIT_FAILURE);
   }

   // Get root window
   mRoot = RootWindow(mDisplay, DefaultScreen(mDisplay));

   // Create windows
   mAppWin = XCreateSimpleWindow(mDisplay, mRoot, 1, 1, 64, 64, 0, 0, 0);
   mIconWin = XCreateSimpleWindow(mDisplay, mAppWin, 0, 0, 64, 64, 0, 0, 0);

   // Set classhint
   classHint.res_name = const_cast<char*>(mInstanceName.c_str());
   classHint.res_class = const_cast<char*>(CLASSNAME);
   XSetClassHint(mDisplay, mAppWin, &classHint);

   // Create delete atom
   mDeleteWindow = XInternAtom(mDisplay, "WM_DELETE_WINDOW", False);
   XSetWMProtocols(mDisplay, mAppWin, &mDeleteWindow, 1);
   XSetWMProtocols(mDisplay, mIconWin, &mDeleteWindow, 1);

   // Set windowname
   XStoreName(mDisplay, mAppWin, APPNAME);
   XSetIconName(mDisplay, mAppWin, APPNAME);

   // Set sizehints
   sizeHints.flags= USPosition;
   sizeHints.x = 0;
   sizeHints.y = 0;
   XSetWMNormalHints(mDisplay, mAppWin, &sizeHints);

   // Set wmhints
   wmHints.initial_state = WithdrawnState;
   wmHints.icon_window = mIconWin;
   wmHints.icon_x = 0;
   wmHints.icon_y = 0;
   wmHints.window_group = mAppWin;
   wmHints.flags = StateHint | IconWindowHint | IconPositionHint | WindowGroupHint;
   XSetWMHints(mDisplay, mAppWin, &wmHints);

   // Set command
   XSetCommand(mDisplay, mAppWin, argv, argc);

   // Set background image
   image = new Xpm(mDisplay, mRoot, main_xpm);
   if (!mLabelText.empty()) {
      image->drawString(LABEL_X, LABEL_Y, mLabelText.c_str());
   }
   image->setWindowPixmapShaped(mIconWin);
   delete image;

   // Create buttons
   mButton[0] = XCreateSimpleWindow(mDisplay, mIconWin, ButtonX[0], BUTTON_MIN, 5, 5, 0, 0, 0);
   mButton[1] = XCreateSimpleWindow(mDisplay, mIconWin, ButtonX[1], BUTTON_MIN, 5, 5, 0, 0, 0);
   mButton[2] = XCreateSimpleWindow(mDisplay, mIconWin, ButtonX[2], BUTTON_MIN, 5, 5, 0, 0, 0);

   image = new Xpm(mDisplay, mRoot, button_xpm);
   image->setWindowPixmap(mButton[0]);
   image->setWindowPixmap(mButton[1]);
   image->setWindowPixmap(mButton[2]);
   delete image;

   XSelectInput(mDisplay, mButton[0], ButtonPressMask | ButtonReleaseMask | PointerMotionMask);
   XSelectInput(mDisplay, mButton[1], ButtonPressMask | ButtonReleaseMask | PointerMotionMask);
   XSelectInput(mDisplay, mButton[2], ButtonPressMask | ButtonReleaseMask | PointerMotionMask);
   XSelectInput(mDisplay, mIconWin, ButtonPressMask);

   for (int i = 0; i < 3; i++) {
      if (aMixer->itemOK(i))
         XMapWindow(mDisplay, mButton[i]);
   }

   XMapWindow(mDisplay, mIconWin);
   XMapWindow(mDisplay, mAppWin);
   XSync(mDisplay, False);

   shutdownRequested = 0;
   signal(SIGPIPE, requestShutdown);
   signal(SIGINT, requestShutdown);
   signal(SIGTERM, requestShutdown);

   // Check if error
   if (mError) {
      showErrorLed();
   } else {
      getVolume();
      if (mLoadSettings)
	loadVolumeSettings();
   }
}

Mixer::~Mixer()
{
   saveVolumeSettings();
   delete aMixer;
   if (mDisplay)
      XCloseDisplay(mDisplay);
}

// Attach configured sources; replace missing default sources with other
// available controls (e.g. 'Capture' on PipeWire/PulseAudio)
void Mixer::selectSources(bool reportErrors)
{
   for (int i = 0; i < 3; i++) {
     if (!MixerSourceSet[i])
       MixerSources[i] = DefaultMixerSources[i];
     aMixer->attachItem(i, MixerSources[i].c_str());
     if (reportErrors && !aMixer->itemOK(i) && MixerSourceSet[i])
       cerr << APPNAME << ": could not select mixer source '" << MixerSources[i] << "'" << endl;
   }

   const vector<string> available = aMixer->volumeItemNames();
   for (int i = 0; i < 3; i++) {
     if (aMixer->itemOK(i) || MixerSourceSet[i])
       continue;
     for (const string& name : available) {
       bool used = false;
       for (int j = 0; j < 3; j++)
         used = used || (aMixer->itemOK(j) && MixerSources[j] == name);
       if (!used) {
         MixerSources[i] = name;
         aMixer->attachItem(i, name.c_str());
         break;
       }
     }
   }
}

// ALSA plugins such as pulse fix their controls when opened, so controls that
// appear later (e.g. a microphone once PipeWire is ready) need a reopen
void Mixer::reopenMixerIfIncomplete()
{
   // retry after 3 s, backing off to once a minute (loop ticks are 100 ms)
   static int idleTicks = 0;
   static int retryTicks = 30;
   bool missing = false;

   for (int i = 0; i < 3; i++)
     missing = missing || !aMixer->itemOK(i);
   if (!missing || ++idleTicks < retryTicks)
     return;
   idleTicks = 0;
   if (retryTicks < 600)
     retryTicks *= 2;

   AMixer* mixer = new AMixer(mMixerDevice.c_str());
   if (!mixer->opened()) {
     delete mixer;
     return;
   }
   if (mixer->volumeItemNames().size() <= aMixer->volumeItemNames().size()) {
     delete mixer;
     return;
   }
   delete aMixer;
   aMixer = mixer;
   selectSources(false);
   getVolume();
}

void Mixer::tryHelp(const char* appname)
{
   cerr << "Try `" << appname << " --help' for more information" << endl;
}

void Mixer::showHelp()
{
   cerr << APPNAME << " Copyright (c) 1998-2002 by Per Liden (per@fukt.bth.se), Petr Hlavka (xhlavk00@stud.fit.vutbr.cz)" << endl << endl
        << "options:" << endl
        << " -1 <source>     set sound source for control 1 (default is Master)" << endl
        << " -2 <source>     set sound source for control 2 (default is PCM)" << endl
        << " -3 <source>     set sound source for control 3 (default is CD)" << endl
        << "                 missing default sources are replaced by other" << endl
        << "                 available controls, or hidden if none remain" << endl
        << " -w 1|2|3        bind a control button to the mouse wheel (default is 1)" << endl
        << " -l <text>       set label text" << endl
        << " -S              save volume settings on exit" << endl
	<< " -L              load volume settings on start up" << endl
	<< " -f	<file>       use setting <file> instead of ~/GNUstep/Defaults/AlsaMixer" << endl
	<< " --card <id>     select card" << endl
	<< " --device <dev>  select device, default 'default'" << endl
	<< " -e <command>    execute <command> on middle click" << endl
        << " -n <name>       set client instance name" << endl
        << " -d <disp>       set display" << endl
        << " -v              print version and exit" << endl
        << " -h, --help      display this help and exit" << endl << endl;
}

void Mixer::checkArgument(char** argv, int argc, int index)
{
   if (index + 1 >= argc) {
      cerr << APPNAME << ": option '" << argv[index] << "' requires an argument" << endl;
      tryHelp(argv[0]);
      exit(EXIT_FAILURE);
   }
}

void Mixer::showErrorLed()
{
   Window led;
   Xpm*   image;

   led = XCreateSimpleWindow(mDisplay, mIconWin, LED_X, LED_Y, 3, 2, 0, 0, 0);

   // Set background image
   image = new Xpm(mDisplay, mRoot, redlight_xpm);
   image->setWindowPixmap(led);
   delete image;

   // Show window
   XMapWindow(mDisplay, led);
   mError = 1;
}

void Mixer::loadVolumeSettings()
{
   if (mSettingsFile.empty())
      return;

   ifstream file(mSettingsFile);
   if (!file)
      return;

   int volumes[3];
   string token;
   bool valid = static_cast<bool>(file >> token) && token == "{";
   for (int i = 0; valid && i < 3; i++) {
      const string key = "Volume" + to_string(i + 1);
      valid = static_cast<bool>(file >> token) && token == key &&
              static_cast<bool>(file >> token) && token == "=" &&
              static_cast<bool>(file >> volumes[i]) &&
              static_cast<bool>(file >> token) && token == ";";
      if (valid && (volumes[i] < 0 || volumes[i] > 100))
         valid = false;
   }
   valid = valid && static_cast<bool>(file >> token) && token == "}";
   if (!valid) {
      cerr << APPNAME << ": invalid volume settings in " << mSettingsFile << endl;
      return;
   }

   for (int i = 0; i < 3; i++) {
      setVolume(i, volumes[i]);
      setButtonPosition(i, percentToPosition(volumes[i]));
   }
}

void Mixer::saveVolumeSettings()
{
   if (mSaveSettings) {
      if (mSettingsFile.empty()) {
         cerr << APPNAME << ": cannot save volume settings without a settings file" << endl;
         return;
      }
      ofstream file(mSettingsFile);
      if (file) {
         // Files in ~/GNUstep/Defaults/ should follow the property list format
         file << "{" << endl
              << "  Volume1 = " << mVolume[0] << ";" << endl
              << "  Volume2 = " << mVolume[1] << ";" << endl
              << "  Volume3 = " << mVolume[2] << ";" << endl
              << "}" << endl;
         file.close();
         if (!file)
            cerr << APPNAME << ": failed to save volume settings in " << mSettingsFile << endl;
      } else {
         cerr << APPNAME << ": failed to save volume settings in " << mSettingsFile << endl;
      }
   }
}

void Mixer::getVolume()
{
   static int lastVolume[3] = {-1, -1, -1};
   static int lastVolumeMute[3] = {-1, -1, -1};
   static int lastAvailable[3] = {-1, -1, -1};

   if (mError) {
      return;
   }

   // Read from device
   for (int i=0; i<3; i++) {
      const int available = aMixer->itemOK(i);
      if (lastAvailable[i] != available) {
         if (available)
            XMapWindow(mDisplay, mButton[i]);
         else
            XUnmapWindow(mDisplay, mButton[i]);
         lastAvailable[i] = available;
      }

      mVolume[i] = aMixer->itemGetVolume(i);
      mVolumeMute[i] = aMixer->itemIsMuted(i);
      if (mVolume[i] < 0) {
         mVolume[i] = 0;
         mVolumeMute[i] = 0;
      }

      if (lastVolume[i] != mVolume[i]) {
         int y;

         // Set button position
         if (mError) {
            y = BUTTON_MIN;
         } else {
	    y = percentToPosition(mVolume[i]);
         }

	 setButtonPosition(i, y);
         lastVolume[i] = mVolume[i];
      }

      // set buttom type muted/unmuted
      if (lastVolumeMute[i] != mVolumeMute[i]) {
	setButtonType(i);
	lastVolumeMute[i] = mVolumeMute[i];
      }
   }

   if (mError) {
      cerr << APPNAME << ": unable to read from " << mMixerDevice << endl;
      showErrorLed();
      return;
   }
}

void Mixer::setVolume(int button, int volume)
{
   if (mError) {
      return;
   }

   // Store volume
   mVolume[button] = volume;

   // Write to device
   aMixer->itemSetVolume(button, mVolume[button]);
}

void Mixer::toggleMute(int button)
{
  if (!aMixer->itemOK(button))
    return;
  aMixer->itemToggleMute(button);
  mVolumeMute[button] = aMixer->itemIsMuted(button);
  setButtonType(button);
}

void Mixer::setButtonType(int button)
{
   Xpm* image;

   if (mVolumeMute[button] == 1) {		// muted
      image = new Xpm(mDisplay, mRoot, mutebutton_xpm);
      image->setWindowPixmap(mButton[button]);
      delete image;

      XClearWindow(mDisplay, mButton[button]);
   } else {
      image = new Xpm(mDisplay, mRoot, button_xpm);
      image->setWindowPixmap(mButton[button]);
      delete image;

      XClearWindow(mDisplay, mButton[button]);
   }
}

void Mixer::setButtonPosition(int button, int position) {
   if (position > BUTTON_MIN) {
      position = BUTTON_MIN;
   } else if (position < BUTTON_MAX) {
      position = BUTTON_MAX;
   }

   XMoveWindow(mDisplay, mButton[button], ButtonX[button], position);

   mVolumePos[button] = position;
}

void Mixer::setButtonPositionRelative(int button, int relativePosition)
{
   int y;

   if (!aMixer->itemOK(button))
      return;

   // Calc new button position
   y = mVolumePos[button] + relativePosition;

   if (y > BUTTON_MIN) {
      y = BUTTON_MIN;
   } else if (y < BUTTON_MAX) {
      y = BUTTON_MAX;
   }

   // Set button position and volume
   XMoveWindow(mDisplay, mButton[button], ButtonX[button], y);

   mVolumePos[button] = y;

   // set volume
   setVolume(button, positionToPercent(y));
}

void Mixer::run()
{
   XEvent event;
   int    buttonDown = 0;
   int    buttonDownPosition = 0;

   // Start handling events
   while(!shutdownRequested) {
      while(XPending(mDisplay) && !shutdownRequested) {
         XNextEvent(mDisplay, &event);

         switch(event.type) {
         case ClientMessage:
            if (static_cast<Atom>(event.xclient.data.l[0]) == mDeleteWindow)
               shutdownRequested = 1;
            break;

         case ButtonPress:
            if (event.xbutton.button == Button4 || event.xbutton.button == Button5) {
               // Wheel scroll
               setButtonPositionRelative(mWheelButton - 1, event.xbutton.button == Button5? 3: -3);
            } else if (event.xbutton.button == Button1 && event.xbutton.window != mIconWin) {
               // Volume change
               buttonDown = 1;
               buttonDownPosition = event.xbutton.y;
            } else if (event.xbutton.button == Button3 && buttonDown == 0 && event.xbutton.window != mIconWin) {
               // Mute
               for (int i=0; i<3; i++) {
                  if (mButton[i] == event.xbutton.window) {
		     toggleMute(i);
                     break;
                  }
               }
            } else if (event.xbutton.button == Button2) {
	       // Load defaults or execute command
	       if (!mCommand.empty()) {
		 system((mCommand + " &").c_str());
	       }
	       else
		 loadVolumeSettings();
	    }
            break;

         case ButtonRelease:
            if (event.xbutton.button == Button1) {
               buttonDown = 0;
            }
            break;

         case MotionNotify:
            if (buttonDown) {
               // Find button
               for (int i=0; i<3; i++) {
                  if (mButton[i] == event.xmotion.window) {
                     setButtonPositionRelative(i, event.xmotion.y - buttonDownPosition);
                     break;
                  }
               }
            }
            break;
         }
      }

      if (shutdownRequested)
         break;

      // Idle for a moment
      usleep(100000);

      // Update volume status
      aMixer->handleEvents();
      if (AMixer::mixerChanged()) {
	aMixer->reInit();
	getVolume();
      } else if (AMixer::mixerElemsChanged()) {
	getVolume();
      }
      reopenMixerIfIncomplete();
      XSync(mDisplay, False);
   }
}
