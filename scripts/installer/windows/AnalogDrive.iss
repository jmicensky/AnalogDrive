; Inno Setup script for the Windows installer (the counterpart to
; build_installer_macos.sh). Built by CI; to build by hand on Windows:
;   ISCC.exe /DAppVersion=1.0.0 /DBuildDir=<repo>\build\AnalogDrive_artefacts\Release /O<out dir> AnalogDrive.iss

#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef BuildDir
  #define BuildDir "..\..\..\build\AnalogDrive_artefacts\Release"
#endif

[Setup]
; Never change AppId: Windows uses it to recognise later betas as upgrades of
; this install rather than a second copy.
AppId={{3557C755-6AFD-4DF7-9DCD-F77094BC1ED9}
AppName=Analog Drive
AppVersion={#AppVersion}
AppVerName=Analog Drive {#AppVersion} Beta
AppPublisher=Saturation Sounds
DefaultDirName={commonpf64}\Saturation Sounds\Analog Drive
DisableDirPage=yes
DisableProgramGroupPage=yes
OutputBaseFilename=AnaDrive_Installer_1
; 64-bit only, matching the x64 CI build.
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
; Admin needed to write into Common Files\VST3 and Program Files.
PrivilegesRequired=admin
WizardStyle=modern
Compression=lzma2
SolidCompression=yes
UninstallDisplayName=Analog Drive

[Messages]
WelcomeLabel2=This will install the Analog Drive beta on your computer.%n%nThanks for testing! This is a pre-release build, so please report anything that seems off.%n%nClose your DAW before continuing.

[Types]
Name: "full"; Description: "Full installation"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plug-in (Ableton Live and most other DAWs)"; Types: full custom
Name: "app"; Description: "Standalone app (runs without a DAW)"; Types: full custom

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut for the standalone app"; Components: app; Flags: unchecked

[Files]
; The VST3 is a folder bundle; copy it whole into the standard system VST3 folder.
Source: "{#BuildDir}\VST3\Analog Drive.vst3\*"; DestDir: "{commoncf64}\VST3\Analog Drive.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#BuildDir}\Standalone\Analog Drive.exe"; DestDir: "{app}"; Components: app; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\Analog Drive"; Filename: "{app}\Analog Drive.exe"; Components: app
Name: "{autodesktop}\Analog Drive"; Filename: "{app}\Analog Drive.exe"; Tasks: desktopicon
