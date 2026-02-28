!include "LogicLib.nsh"

!define PRODUCT_NAME "serialosc"
!define PRODUCT_VERSION "1.4.6"
!define PRODUCT_PUBLISHER "monome"
!define PRODUCT_WEB_SITE "https://monome.org/"
!define PRODUCT_DIR_REGKEY "Software\Microsoft\Windows\CurrentVersion\App Paths\serialoscd.exe"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define PRODUCT_UNINST_ROOT_KEY "HKLM"

!ifndef FILE_SRC
  !define FILE_SRC "."
!endif
!define SVC_NAME "serialosc"

SetCompressor /SOLID lzma

!include "MUI2.nsh"

; MUI Settings
!define MUI_ABORTWARNING
!define MUI_ICON "mlogo.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\win-uninstall.ico"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "license.txt"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

; Uninstaller pages
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

; Language files
!insertmacro MUI_LANGUAGE "English"

Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
!ifdef ARCH
  OutFile "${PRODUCT_NAME}-${PRODUCT_VERSION}-${ARCH}.exe"
!else
  OutFile "${PRODUCT_NAME}-${PRODUCT_VERSION}.exe"
!endif
InstallDir "$PROGRAMFILES64\Monome\serialosc"
InstallDirRegKey HKLM "${PRODUCT_DIR_REGKEY}" ""
RequestExecutionLevel admin
ShowInstDetails show
ShowUnInstDetails show
BrandingText " "

Section "serialosc" SEC01
  nsExec::ExecToLog 'net stop ${SVC_NAME}'

  SetOutPath "$INSTDIR"
  SetOverwrite on
  File "${FILE_SRC}\serialoscd.exe"
  File "${FILE_SRC}\serialosc-detector.exe"
  File "${FILE_SRC}\serialosc-device.exe"

  ; remove old serialosc executable
  Delete "$INSTDIR\serialosc.exe"

  ; remove old serialosc libs
  Delete "$INSTDIR\libmonome.dll"
  Delete "$INSTDIR\liblo-7.dll"

  Delete "$INSTDIR\monome\protocol_40h.dll"
  Delete "$INSTDIR\monome\protocol_mext.dll"
  Delete "$INSTDIR\monome\protocol_series.dll"

  RMDir "$INSTDIR\monome"
SectionEnd

Section -Post
  WriteUninstaller "$INSTDIR\uninstall.exe"
  WriteRegStr HKLM "${PRODUCT_DIR_REGKEY}" "" "$INSTDIR\serialoscd.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayName" "$(^Name)"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "UninstallString" "$INSTDIR\uninstall.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "QuietUninstallString" "$INSTDIR\uninstall.exe /S"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayIcon" "$INSTDIR\serialoscd.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "URLInfoAbout" "${PRODUCT_WEB_SITE}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"

  nsExec::ExecToLog 'sc create ${SVC_NAME} binPath= "$INSTDIR\serialoscd.exe" start= auto type= own'
  nsExec::ExecToLog 'sc config ${SVC_NAME} binPath= "$INSTDIR\serialoscd.exe"'
  nsExec::ExecToLog 'sc description ${SVC_NAME} "OSC server for monome devices"'
  nsExec::ExecToLog 'sc start ${SVC_NAME}'
SectionEnd

Section Uninstall
  nsExec::ExecToLog 'net stop ${SVC_NAME}'
  nsExec::ExecToLog 'sc delete ${SVC_NAME}'

  Delete "$INSTDIR\uninstall.exe"
  Delete "$INSTDIR\libmonome.dll"
  Delete "$INSTDIR\liblo-7.dll"
  Delete "$INSTDIR\serialoscd.exe"
  Delete "$INSTDIR\serialosc-detector.exe"
  Delete "$INSTDIR\serialosc-device.exe"
  Delete "$INSTDIR\monome\protocol_series.dll"
  Delete "$INSTDIR\monome\protocol_mext.dll"
  Delete "$INSTDIR\monome\protocol_40h.dll"

  RMDir "$INSTDIR\monome"
  RMDir "$INSTDIR"

  DeleteRegKey ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}"
  DeleteRegKey HKLM "${PRODUCT_DIR_REGKEY}"
SectionEnd
