; Inno Setup script for the P-800 VST3 on Windows.
; Built by .github/workflows/poly800-windows-linux.yml:
;   iscc /DAppVersion=0.3.0 /DSrcDir=<repo> plugin\installer\windows\P-800.iss

#ifndef AppVersion
  #define AppVersion "0.3.0"
#endif
#ifndef SrcDir
  #define SrcDir "..\..\.."
#endif

[Setup]
AppId={{6E3B2F1A-8C40-4D7B-9F2A-80020C0F1800}
AppName=P-800
AppVersion={#AppVersion}
AppPublisher=Ticco Ross
AppPublisherURL=https://github.com/Spunkmeyer77/Poly800
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
LicenseFile={#SrcDir}\LICENSE
OutputDir={#SrcDir}\dist
OutputBaseFilename=P-800 {#AppVersion} Windows Setup
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes
UninstallDisplayName=P-800 (Ticco Ross)

[Files]
Source: "{#SrcDir}\plugin\build\P800_artefacts\Release\VST3\P-800.vst3\*"; DestDir: "{commoncf64}\VST3\P-800.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "{#SrcDir}\plugin\docs\P-800 Manual.pdf"; DestDir: "{commondocs}\Ticco Ross\P-800"; Flags: ignoreversion

[Icons]
Name: "{commonprograms}\Ticco Ross\P-800 Manual"; Filename: "{commondocs}\Ticco Ross\P-800\P-800 Manual.pdf"

[Run]
Filename: "{commondocs}\Ticco Ross\P-800\P-800 Manual.pdf"; Description: "Open the P-800 manual"; Flags: postinstall shellexec skipifsilent unchecked
