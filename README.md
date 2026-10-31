# randomsaver

A randomizing screensaver for Haiku

This is not a port of the randomizing screensaver from the BeOS era with 
the same name. It is a new program, unapologetically vibe-coded for a new era.

RandomSaver draws nothing itself: every time it starts it picks one of the
other installed screen savers at random and runs that one instead. Blackness
is never chosen, and neither is RandomSaver itself. If the chosen screen
saver fails to load, the next random candidate is tried.

## Preferences

The configuration panel lists every installed screen saver (except Blackness
and RandomSaver itself) with a check box in front of it. Unchecking an entry
blacklists it, so RandomSaver will never pick it. With no saved
configuration - that is, on the first run - every screen saver is enabled.

The list is taken from the same four directories the ScreenSaver preferences
app itself scans, so it shows exactly what that list shows. There is no complex
node monitoring - if you install a new screensaver while the Screensaver app is
open, close and restart it to see it on the list.

## Building

    mkdir build
    cd build
    cmake ..
    make

`make clean` works as usual. The build produces a `RandomSaver` add-on file;
copy it into one of the screen saver directories, for example

    ~/config/non-packaged/add-ons/Screen Savers/

and select it in the ScreenSaver preferences.

## Notes

When RandomSaver delegates to another screen saver, it passes that screen
saver its own saved settings (read from `~/config/settings/ScreenSaver_settings`)
and adopts its tick size and loop counts, so the delegated saver behaves the
same as it does when run directly.
