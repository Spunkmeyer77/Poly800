; Inno Setup script for the EX-800 VST3 on Windows.
; Built by .github/workflows/poly800-windows-linux.yml:
;   iscc /DAppVersion=0.8.0 /DSrcDir=<repo> plugin\installer\windows\EX-800.iss

#ifndef AppVersion
  #define AppVersion "0.8.0"
#endif
#ifndef SrcDir
  #define SrcDir "..\..\.."
#endif

[Setup]
AppId={{6E3B2F1A-8C40-4D7B-9F2A-80020C0F1800}
AppName=EX-800
AppVersion={#AppVersion}
AppPublisher=Ticco Ross
AppPublisherURL=https://github.com/Spunkmeyer77/Poly800
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
LicenseFile={#SrcDir}\LICENSE
OutputDir={#SrcDir}\dist
OutputBaseFilename=EX-800 {#AppVersion} Windows Setup
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes
UninstallDisplayName=EX-800 (Ticco Ross)

[InstallDelete]
; EX-800 was called P-800 up to 0.5.2: remove the old bundle (same plugin ID) and its shortcut.
Type: filesandordirs; Name: "{commoncf64}\VST3\P-800.vst3"
Type: files; Name: "{commonprograms}\Ticco Ross\P-800 Manual.lnk"

[Files]
Source: "{#SrcDir}\plugin\build\P800_artefacts\Release\VST3\EX-800.vst3\*"; DestDir: "{commoncf64}\VST3\EX-800.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "{#SrcDir}\plugin\docs\EX-800 Manual.pdf"; DestDir: "{commondocs}\Ticco Ross\EX-800"; Flags: ignoreversion
Source: "{#SrcDir}\plugin\docs\EX-800 Update History.pdf"; DestDir: "{commondocs}\Ticco Ross\EX-800"; Flags: ignoreversion

[Icons]
Name: "{commonprograms}\Ticco Ross\EX-800 Manual"; Filename: "{commondocs}\Ticco Ross\EX-800\EX-800 Manual.pdf"

[Run]
Filename: "{commondocs}\Ticco Ross\EX-800\EX-800 Manual.pdf"; Description: "Open the EX-800 manual"; Flags: postinstall shellexec skipifsilent unchecked
