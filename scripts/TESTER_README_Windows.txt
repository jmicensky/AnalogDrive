Expanded Saturation — Windows Install Instructions (early test build)
=====================================================================

This is an early friends-and-family test build. It isn't signed with a
paid code-signing certificate yet, so Windows may show a SmartScreen
warning ("Windows protected your PC") the first time you run something
from it. That's expected for this early test round.

1. INSTALL
   Copy the folder:
     "Expanded Saturation.vst3"  ->  C:\Program Files\Common Files\VST3\
   (The Standalone app doesn't need installing — run it directly from the
   "Standalone" folder.)

2. IF WINDOWS BLOCKS IT
   Click "More info" on the SmartScreen popup, then "Run anyway."

3. LOAD IT IN YOUR DAW
   Ableton Live and most other Windows DAWs pick up VST3 automatically from
   that Common Files folder — rescan plugins in your DAW's preferences if it
   doesn't show up right away.

   Pro Tools / AAX isn't part of this test round yet — that's coming later.

4. WHAT TO WATCH FOR
   - Does it load and show its UI correctly?
   - Any crashes, especially on load/unload or when resizing the window?
   - Any weirdness with presets saving/loading?
   - How does CPU usage feel with a few instances running, especially at
     smaller buffer sizes (256/512)?

Thanks for testing — flag anything unusual, even if it seems minor!
