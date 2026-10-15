Analog Drive — Windows Install Instructions (BETA)
=====================================================================

Thanks for beta testing Analog Drive! This is a pre-release build, so
expect the odd rough edge, and please tell us about anything that seems
off.

Needs Windows 10 or 11, 64-bit, on an Intel or AMD PC.


!! BEFORE YOU START: WINDOWS WILL WARN YOU ABOUT THE INSTALLER !!
---------------------------------------------------------------------
Beta builds aren't signed with a paid code-signing certificate yet, so
when you open "AnaDrive_Installer_1.exe" Windows will show a blue
"Windows protected your PC" (SmartScreen) message. That's expected and
doesn't mean anything is wrong. To continue:

  1. Click "More info" on the blue message.
  2. Click "Run anyway".
  3. Click "Yes" when Windows asks whether to allow the app to make
     changes to your device.

You only have to do this for the installer. Once it has run, the
plug-in loads normally.

If your browser blocks the download first (e.g. "isn't commonly
downloaded"), choose "Keep" / "Keep anyway" from the download menu.


1. RUN THE INSTALLER
   Close your DAW first, then follow the installer's steps. It installs:
     - VST3 plug-in   -> C:\Program Files\Common Files\VST3\
                         (Ableton Live and most other DAWs)
     - Standalone app -> C:\Program Files\Saturation Sounds\Analog Drive\
                         (with a Start menu shortcut)
   Choose "Custom installation" if you only want one of these.

2. LOAD IT IN YOUR DAW
   - Ableton Live: Options -> Preferences -> Plug-Ins. Make sure "Use
     VST3 Plug-in System Folders" is on, then click Rescan. Analog Drive
     will appear under Plug-Ins in the browser.
   - Other DAWs: rescan plug-ins in the DAW's settings. They all look in
     the standard Common Files\VST3 folder.

   If you tried an earlier build by copying files in by hand, the
   installer replaces them.

   Pro Tools / AAX isn't part of this beta yet — that's coming later.

3. WHAT TO WATCH FOR
   - Does it load and show its UI correctly?
   - Any crashes, especially on load/unload or when resizing the window?
   - Any weirdness with presets saving/loading?
   - How does CPU usage feel with a few instances running, especially at
     smaller buffer sizes (256/512)?

UNINSTALLING
   Settings -> Apps -> Installed apps -> Analog Drive -> Uninstall.

Thanks for beta testing — flag anything unusual, even if it seems minor!
