Unicode true
!include "MUI2.nsh"
!include "x64.nsh"
!ifndef BUILD
!error "Set BUILD to the Release artefacts directory"
!endif
!ifndef OUTPUT
!error "Set OUTPUT to the installer file"
!endif
!define PRODUCT "Swell Realistic guitar"
!define VERSION "1.0.1"
!define REGKEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\SwellRealisticGuitar"
Icon "..\Assets\guitar-icon.ico"
Name "Swell Realistic Guitar ${VERSION}"
OutFile "${OUTPUT}"
InstallDir "$PROGRAMFILES64\Swell\Realistic Guitar"
RequestExecutionLevel admin
SetCompressor /SOLID lzma
VIProductVersion ${VERSION}.0
VIAddVersionKey "ProductName" "Swell Realistic Guitar"
VIAddVersionKey "ProductVersion" "${VERSION}"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "FileDescription" "Swell Realistic Guitar Setup"
VIAddVersionKey "LegalCopyright" "Swell Audio"
!define MUI_WELCOMEPAGE_TEXT "Cet assistant installe l’application autonome et le VST3 Swell Realistic Guitar. Les sons ne sont pas inclus : au premier lancement, active ta licence puis choisis ton dossier d'echantillons (AG, NG, LP, TC)."
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_SHOWREADME "$INSTDIR\README.txt"
!define MUI_FINISHPAGE_SHOWREADME_TEXT "Lire le guide"
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "French"

Function .onInit
    ${IfNot} ${RunningX64}
        MessageBox MB_OK "Windows 64 bits est requis."
        Abort
    ${EndIf}
FunctionEnd

Section "Instrument"
    SetRegView 64
    SetShellVarContext all
    SetOutPath "$INSTDIR"
    File "README.txt"
    File "${BUILD}\Standalone\${PRODUCT}.exe"
    SetOutPath "$COMMONFILES64\VST3\${PRODUCT}.vst3"
    File /r "${BUILD}\VST3\${PRODUCT}.vst3\*"
    CreateDirectory "$SMPROGRAMS\Swell Audio"
    CreateShortCut "$SMPROGRAMS\Swell Audio\Guide Realistic Guitar.lnk" "$INSTDIR\README.txt"
    CreateShortCut "$SMPROGRAMS\Swell Audio\Realistic Guitar.lnk" "$INSTDIR\${PRODUCT}.exe"
    WriteUninstaller "$INSTDIR\Uninstall.exe"
    WriteRegStr HKLM "${REGKEY}" "DisplayName" "Swell Realistic Guitar"
    WriteRegStr HKLM "${REGKEY}" "Publisher" "Swell Audio"
    WriteRegStr HKLM "${REGKEY}" "DisplayVersion" "${VERSION}"
    WriteRegStr HKLM "${REGKEY}" "InstallLocation" "$INSTDIR"
    WriteRegStr HKLM "${REGKEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
SectionEnd

Section "Uninstall"
    SetRegView 64
    SetShellVarContext all
    Delete "$SMPROGRAMS\Swell Audio\Guide Realistic Guitar.lnk"
    Delete "$SMPROGRAMS\Swell Audio\Realistic Guitar.lnk"
    Delete "$INSTDIR\${PRODUCT}.exe"
    RMDir "$SMPROGRAMS\Swell Audio"
    RMDir /r "$COMMONFILES64\VST3\${PRODUCT}.vst3"
    Delete "$INSTDIR\README.txt"
    Delete "$INSTDIR\Uninstall.exe"
    RMDir "$INSTDIR"
    DeleteRegKey HKLM "${REGKEY}"
SectionEnd
