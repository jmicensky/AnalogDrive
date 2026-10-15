Analog Drive — macOS Install Instructions (BETA)
====================================================================

Thanks for beta testing Analog Drive! This is a pre-release build, so
expect the odd rough edge, and please tell us about anything that seems
off.

Works on Intel and Apple Silicon Macs. Needs macOS 10.13 or newer on
Intel, or macOS 11 or newer on Apple Silicon.


!! BEFORE YOU START: macOS WILL BLOCK THE INSTALLER THE FIRST TIME !!
--------------------------------------------------------------------
Beta builds aren't signed with a paid Apple certificate yet, so when
you first open "AnaDrive_Installer_1.pkg" macOS will refuse to open it,
with a message like "Apple could not verify..." or "unidentified
developer". That's expected and doesn't mean anything is wrong. You'll
need to change one security setting to allow it, just once:

  macOS 15 (Sequoia) or newer:
    1. Double-click the installer, then click "Done" on the warning.
    2. Open System Settings -> Privacy & Security.
    3. Scroll down to the Security section and click "Open Anyway"
       next to "AnaDrive_Installer_1.pkg".
    4. Enter your Mac password and confirm. The installer opens.

  macOS 14 (Sonoma) or older:
    Right-click (or Control-click) "AnaDrive_Installer_1.pkg" -> Open,
    then click "Open" in the dialog.

You only have to allow the installer. Once it has run, the plug-ins
load normally with no further warnings.


1. RUN THE INSTALLER
   Follow the installer's steps. It installs:
     - VST3 plug-in   (Ableton Live and most other DAWs)
     - Audio Unit     (Logic Pro, GarageBand, and Ableton Live)
     - Standalone app (in your Applications folder)
   It asks for your Mac password because plug-ins go into the shared
   /Library/Audio/Plug-Ins folders.

2. LOAD IT IN YOUR DAW
   Quit your DAW before installing, or quit and reopen it afterwards.
   - Ableton Live: Settings -> Plug-Ins. Make sure "Use VST3 Plug-in
     System Folders" is on, then click Rescan. Analog Drive will appear
     under Plug-Ins in the browser.
   - Logic Pro: it should show up automatically under Audio Units ->
     Saturation Sounds. If it doesn't, open Logic Pro -> Settings ->
     Plug-in Manager, select it and click "Reset & Rescan Selection".

   If you tried an earlier build by copying files into
   ~/Library/Audio/Plug-Ins yourself, the installer removes those old
   copies automatically.

   Pro Tools / AAX isn't part of this beta yet — that's coming later.

3. WHAT TO WATCH FOR
   - Does it load and show its UI correctly?
   - Any crashes, especially on load/unload or when resizing the window?
   - Any weirdness with presets saving/loading?
   - How does CPU usage feel with a few instances running, especially at
     smaller buffer sizes (256/512)?

Thanks for beta testing — flag anything unusual, even if it seems minor!
