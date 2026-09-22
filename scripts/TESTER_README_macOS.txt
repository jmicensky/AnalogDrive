Analog Drive — macOS Install Instructions (early test build)
====================================================================

This is an early friends-and-family test build, so it's not yet signed with
a paid Apple Developer certificate. macOS WILL warn you about it the first
time — that's expected, not a sign anything is wrong. Here's how to get past
that and get it installed.

1. INSTALL
   Copy these into place:
     "Analog Drive.vst3"        -> ~/Library/Audio/Plug-Ins/VST3/
     "Analog Drive.component"   -> ~/Library/Audio/Plug-Ins/Components/
   (The Standalone app doesn't need installing — run it from wherever you
   unzipped it, or drag it to Applications if you want it to stick around.)

2. IF macOS BLOCKS IT ("unidentified developer" / "can't be opened")
   Go to System Settings -> Privacy & Security, scroll down to the Security
   section, and click "Open Anyway" next to the Analog Drive warning.
   Then try opening it again and confirm.

   (Alternative: right-click the .component/.vst3/.app -> Open -> confirm
   "Open" in the dialog that appears.)

3. LOAD IT IN YOUR DAW
   - Ableton Live: uses the VST3 version. If it doesn't show up, go to
     Preferences -> Plug-Ins and trigger a rescan.
   - Logic Pro: uses the AU (.component) version — Logic doesn't support
     VST3 at all. It should appear automatically; if not, try Logic's
     AU Manager (under Preferences) and re-validate plugins.

   Pro Tools / AAX isn't part of this test round yet — that's coming later.

4. WHAT TO WATCH FOR
   - Does it load and show its UI correctly?
   - Any crashes, especially on load/unload or when resizing the window?
   - Any weirdness with presets saving/loading?
   - How does CPU usage feel with a few instances running, especially at
     smaller buffer sizes (256/512)?

Thanks for testing — flag anything unusual, even if it seems minor!
