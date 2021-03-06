;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; secondlife setup.nsi
;; Copyright 2004-2011, Linden Research, Inc.
;;
;; This library is free software; you can redistribute it and/or
;; modify it under the terms of the GNU Lesser General Public
;; License as published by the Free Software Foundation;
;; version 2.1 of the License only.
;;
;; This library is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
;; Lesser General Public License for more details.
;;
;; You should have received a copy of the GNU Lesser General Public
;; License along with this library; if not, write to the Free Software
;; Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
;;
;; Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
;;
;; NSIS Unicode 2.38.1 or higher required
;; http://www.scratchpaper.com/
;;
;; Author: James Cook, Don Kjer, Callum Prentice
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Compiler flags
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
SetOverwrite on				; overwrite files
SetCompress auto			; compress iff saves space
SetCompressor /solid lzma	; compress whole installer as one block
SetDatablockOptimize off	; only saves us 0.1%, not worth it
XPStyle on                  ; add an XP manifest to the installer
RequestExecutionLevel admin	; on Vista we must be admin because we write to Program Files

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Project flags
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

%%VERSION%%

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; - language files - one for each language (or flavor thereof)
;; (these files are in the same place as the nsi template but the python script generates a new nsi file in the 
;; application directory so we have to add a path to these include files)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
!include "%%SOURCE%%\installers\windows\lang_da.nsi"
!include "%%SOURCE%%\installers\windows\lang_de.nsi"
!include "%%SOURCE%%\installers\windows\lang_en-us.nsi"
!include "%%SOURCE%%\installers\windows\lang_es.nsi"
!include "%%SOURCE%%\installers\windows\lang_fr.nsi"
!include "%%SOURCE%%\installers\windows\lang_ja.nsi"
!include "%%SOURCE%%\installers\windows\lang_it.nsi"
!include "%%SOURCE%%\installers\windows\lang_pl.nsi"
!include "%%SOURCE%%\installers\windows\lang_pt-br.nsi"
!include "%%SOURCE%%\installers\windows\lang_ru.nsi"
!include "%%SOURCE%%\installers\windows\lang_tr.nsi"
!include "%%SOURCE%%\installers\windows\lang_zh.nsi"

;;!include "%%SOURCE%%\installers\windowsMUI.nsh"

# *TODO: Move these into the language files themselves
LangString LanguageCode ${LANG_DANISH}   "da"
LangString LanguageCode ${LANG_GERMAN}   "de"
LangString LanguageCode ${LANG_ENGLISH}  "en"
LangString LanguageCode ${LANG_SPANISH}  "es"
LangString LanguageCode ${LANG_FRENCH}   "fr"
LangString LanguageCode ${LANG_JAPANESE} "ja"
LangString LanguageCode ${LANG_ITALIAN}  "it"
LangString LanguageCode ${LANG_POLISH}   "pl"
LangString LanguageCode ${LANG_PORTUGUESEBR} "pt"
LangString LanguageCode ${LANG_RUSSIAN}  "ru"
LangString LanguageCode ${LANG_TURKISH}  "tr"
LangString LanguageCode ${LANG_TRADCHINESE}  "zh"

;; this placeholder is replaced by viewer_manifest.py
%%INST_VARS%%

Name ${INSTNAME}

LicenseText "Vivox Voice System License Agreement"
LicenseData "VivoxAUP.txt"

;SubCaption 0 $(LicenseSubTitleSetup)	; override "license agreement" text

BrandingText " "						; bottom of window text
Icon          %%SOURCE%%\installers\windows\casviewer_icon.ico
UninstallIcon %%SOURCE%%\installers\windows\casviewer_icon.ico
WindowIcon on							; show our icon in left corner
BGGradient off							; no big background window
CRCCheck on								; make sure CRC is OK
InstProgressFlags smooth colored		; new colored smooth look
; <FS:Ansariel> Expose details button (details hidden by default)
;ShowInstDetails nevershow				; no details, no "show" button
SetOverwrite on							; stomp files by default
; <FS:Ansariel> Don't auto-close so we can check details
;AutoCloseWindow true					; after all files install, close window

InstallDir "$PROGRAMFILES\${INSTNAME}"
InstallDirRegKey HKEY_LOCAL_MACHINE "SOFTWARE\CtrlAltStudio Viewer\${INSTNAME}" ""
DirText $(DirectoryChooseTitle) $(DirectoryChooseSetup)

Page license
; <FS:Ansariel> Optional start menu entry
;Page directory dirPre
Page directory dirPre "" dirPost
; </FS:Ansariel>
Page instfiles

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Variables
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Var INSTPROG
Var INSTEXE
Var INSTSHORTCUT
Var COMMANDLINE         ; command line passed to this installer, set in .onInit
Var SHORTCUT_LANG_PARAM ; "--set InstallLanguage de", passes language to viewer
Var SKIP_DIALOGS        ; set from command line in  .onInit. autoinstall 
                        ; GUI and the defaults.
; Var SKIP_AUTORUN		; skip automatic launch of viewer after install -- <FS:PP> Commented out: Disable autorun
Var DO_UNINSTALL_V2     ; If non-null, path to a previous Viewer 2 installation that will be uninstalled.
Var NO_STARTMENU        ; <FS:Ansariel> Optional start menu entry

;;; Function definitions should go before file includes, because calls to
;;; DLLs like LangDLL trigger an implicit file include, so if that call is at
;;; the end of this script NSIS has to decompress the whole installer before 
;;; it can call the DLL function. JC

!include "FileFunc.nsh"     ; For GetParameters, GetOptions
!insertmacro GetParameters
!insertmacro GetOptions
!include WinVer.nsh			; for OS and SP detection
!include x64.nsh

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; After install completes, launch app
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function .onInstSuccess
Call CheckWindowsServPack				; Warn if not on the latest SP before asking to launch.
    Push $R0	# Option value, unused
	; <FS:PP> Disable autorun
	; StrCmp $SKIP_AUTORUN "true" +2;
	; # Assumes SetOutPath $INSTDIR
	; Exec '"$INSTDIR\$INSTEXE" $SHORTCUT_LANG_PARAM'
	; Pop $R0
    StrCmp $SKIP_DIALOGS "true" label_launch 

    ${GetOptions} $COMMANDLINE "/AUTOSTART" $R0
    # If parameter was there (no error) just launch
    # Otherwise ask
    IfErrors label_ask_launch label_launch

label_ask_launch:
    # Don't launch by default when silent
    IfSilent label_no_launch
	MessageBox MB_YESNO $(InstSuccesssQuestion) \
        IDYES label_launch IDNO label_no_launch

label_launch:
	# Assumes SetOutPath $INSTDIR
	Exec '"$INSTDIR\$INSTEXE" $SHORTCUT_LANG_PARAM'
label_no_launch:
	Pop $R0
	; </FS:PP>
FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Pre-directory page callback
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function dirPre
    StrCmp $SKIP_DIALOGS "true" 0 +2
	Abort
FunctionEnd    

; <FS:Ansariel> Optional start menu entry
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Post-directory page callback
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function dirPost
    StrCmp $SKIP_DIALOGS "true" label_create_start_menu
	
    MessageBox MB_YESNO|MB_ICONQUESTION $(CreateStartMenuEntry) IDYES label_create_start_menu
    StrCpy $NO_STARTMENU "true"

label_create_start_menu:

FunctionEnd
; </FS:Ansariel>

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Make sure this computer meets the minimum system requirements.
; Currently: Windows 32bit XP SP3, 64bit XP SP2 and Server 2003 SP2
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function CheckWindowsVersion
  ${If} ${AtMostWin2000}
    MessageBox MB_OK $(CheckWindowsVersionMB)
    Quit
  ${EndIf}

  ${If} ${IsWinXP}
  ${AndIfNot} ${RunningX64}
  ${AndIfNot} ${IsServicePack} 3
    MessageBox MB_OK $(CheckWindowsVersionMB)
    Quit
  ${EndIf}

  ${If} ${IsWinXP}
  ${AndIf} ${RunningX64}
  ${AndIfNot} ${IsServicePack} 2
    MessageBox MB_OK $(CheckWindowsVersionMB)
    Quit
  ${EndIf}

  ${If} ${IsWin2003}
  ${AndIfNot} ${IsServicePack} 2
    MessageBox MB_OK $(CheckWindowsVersionMB)
    Quit
  ${EndIf}
FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;Recommend Upgrading Service Pack
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function CheckWindowsServPack
  ${If} ${IsWinVista}
  ${AndIfNot} ${IsServicePack} 2
    MessageBox MB_OK $(CheckWindowsServPackMB)
    DetailPrint $(UseLatestServPackDP)
    Return
  ${EndIf}

  ${If} ${IsWin2008}
  ${AndIfNot} ${IsServicePack} 2
    MessageBox MB_OK $(CheckWindowsServPackMB)
    DetailPrint $(UseLatestServPackDP)
    Return
  ${EndIf}

  ${If} ${IsWin7}
  ${AndIfNot} ${IsServicePack} 1
    MessageBox MB_OK $(CheckWindowsServPackMB)
    DetailPrint $(UseLatestServPackDP)
    Return
  ${EndIf}

  ${If} ${IsWin2008R2}
  ${AndIfNot} ${IsServicePack} 1
    MessageBox MB_OK $(CheckWindowsServPackMB)
    DetailPrint $(UseLatestServPackDP)
    Return
  ${EndIf}
FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Make sure the user can install/uninstall
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function CheckIfAdministrator
    DetailPrint $(CheckAdministratorInstDP)
    UserInfo::GetAccountType
    Pop $R0
    StrCmp $R0 "Admin" lbl_is_admin
        MessageBox MB_OK $(CheckAdministratorInstMB)
        Quit
lbl_is_admin:
    Return
FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function un.CheckIfAdministrator
    DetailPrint $(CheckAdministratorUnInstDP)
    UserInfo::GetAccountType
    Pop $R0
    StrCmp $R0 "Admin" lbl_is_admin
        MessageBox MB_OK $(CheckAdministratorUnInstMB)
        Quit
lbl_is_admin:
    Return
FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Checks to see if the current version has already been installed (according to the registry).
; If it has, allow user to bail out of install process.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function CheckIfAlreadyCurrent
    Push $0
    ReadRegStr $0 HKEY_LOCAL_MACHINE "SOFTWARE\CtrlAltStudio Viewer\$INSTPROG" "Version"
    StrCmp $0 ${VERSION_LONG} 0 continue_install
    StrCmp $SKIP_DIALOGS "true" continue_install
    MessageBox MB_OKCANCEL $(CheckIfCurrentMB) /SD IDOK IDOK continue_install
    Quit
continue_install:
    Pop $0
    Return
FunctionEnd
	
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Checks for CPU valid (must have SSE2 support)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function CheckCPUFlags
    Push $1
    System::Call 'kernel32::IsProcessorFeaturePresent(i) i(10) .r1'
    IntCmp $1 1 OK_SSE2
    MessageBox MB_OKCANCEL $(MissingSSE2) /SD IDOK IDOK OK_SSE2
    Quit

  OK_SSE2:
    Pop $1
    Return
FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Close the program, if running. Modifies no variables.
; Allows user to bail out of install process.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function CloseSecondLife
  Push $0
  FindWindow $0 "Second Life" ""
  IntCmp $0 0 DONE
  
  StrCmp $SKIP_DIALOGS "true" CLOSE
    MessageBox MB_OKCANCEL $(CloseSecondLifeInstMB) IDOK CLOSE IDCANCEL CANCEL_INSTALL

  CANCEL_INSTALL:
    Quit

  CLOSE:
    DetailPrint $(CloseSecondLifeInstDP)
    SendMessage $0 16 0 0

  LOOP:
	  FindWindow $0 "Second Life" ""
	  IntCmp $0 0 DONE
	  Sleep 500
	  Goto LOOP

  DONE:
    Pop $0
    Return
FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Test our connection to secondlife.com
; Also allows us to count attempted installs by examining web logs.
; *TODO: Return current SL version info and have installer check
; if it is up to date.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function CheckNetworkConnection
    ; Disabling this, not needed for Firestorm -AO
    Return 
    Push $0
    Push $1
    Push $2	# Option value for GetOptions
    DetailPrint $(CheckNetworkConnectionDP)
    ; Look for a tag value from the stub installer, used for statistics
    ; to correlate installs.  Default to "" if not found on command line.
    StrCpy $2 ""
    ${GetOptions} $COMMANDLINE "/STUBTAG=" $2
    GetTempFileName $0
    !define HTTP_TIMEOUT 5000 ; milliseconds
    ; Don't show secondary progress bar, this will be quick.
    NSISdl::download_quiet \
        /TIMEOUT=${HTTP_TIMEOUT} \
        "http://install.secondlife.com/check/?stubtag=$2&version=${VERSION_LONG}" \
        $0
    Pop $1 ; Return value, either "success", "cancel" or an error message
    ; MessageBox MB_OK "Download result: $1"
    ; Result ignored for now
	; StrCmp $1 "success" +2
	;	DetailPrint "Connection failed: $1"
    Delete $0 ; temporary file
    Pop $2
    Pop $1
    Pop $0
    Return
FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Function CheckOldExeName
; Viewer versions < 3.6.12 used the name 'SecondLife.exe'
; If that name is found in the install folder, delete it to invalidate any
; old shortcuts to it that may be in non-standard locations, so that the user
; does not end up running the old version (potentially getting caught in an 
; infinite update loop). See MAINT-3575
; ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

Function CheckOldExeName
  ; <FS:Ansariel> We don't need that
  Return

  IfFileExists "$INSTDIR\SecondLife.exe" CHECKOLDEXE_FOUND CHECKOLDEXE_DONE

CHECKOLDEXE_FOUND:
  Delete "$INSTDIR\SecondLife.exe"
CHECKOLDEXE_DONE:
FunctionEnd


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Function CheckWillUninstallV2               
;
; If we are being called through auto-update, we need to uninstall any
; existing V2 installation. Otherwise, we wind up with
; SecondLifeViewer2 and SecondLifeViewer installations existing side
; by side no indication which to use.
; ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function CheckWillUninstallV2

  StrCpy $DO_UNINSTALL_V2 ""

  ; <FS:Ansariel> Don't mess with the official viewer
  Return

  StrCmp $SKIP_DIALOGS "true" 0 CHECKV2_DONE
  StrCmp $INSTDIR "$PROGRAMFILES\SecondLifeViewer2" CHECKV2_DONE ; don't uninstall our own install dir.
  IfFileExists "$PROGRAMFILES\SecondLifeViewer2\uninst.exe" CHECKV2_FOUND CHECKV2_DONE

CHECKV2_FOUND:
  StrCpy $DO_UNINSTALL_V2 "true"

CHECKV2_DONE:

FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Save user files to temp location
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function PreserveUserFiles

Push $0
Push $1
Push $2

    RMDir /r "$TEMP\SecondLifeSettingsBackup"
    CreateDirectory "$TEMP\SecondLifeSettingsBackup"
    StrCpy $0 0 ; Index number used to iterate via EnumRegKey

  LOOP:
    EnumRegKey $1 HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows NT\CurrentVersion\ProfileList" $0
    StrCmp $1 "" DONE               ; no more users

    ReadRegStr $2 HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows NT\CurrentVersion\ProfileList\$1" "ProfileImagePath" 
    StrCmp $2 "" CONTINUE 0         ; "ProfileImagePath" value is missing

    ; Required since ProfileImagePath is of type REG_EXPAND_SZ
    ExpandEnvStrings $2 $2

    CreateDirectory "$TEMP\SecondLifeSettingsBackup\$0"
    CopyFiles /SILENT "$2\Application Data\SecondLife\*" "$TEMP\SecondLifeSettingsBackup\$0"

  CONTINUE:
    IntOp $0 $0 + 1
    Goto LOOP
  DONE:

Pop $2
Pop $1
Pop $0

; Copy files in Documents and Settings\All Users\SecondLife
Push $0
    ReadRegStr $0 HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\Shell Folders" "Common AppData"
    StrCmp $0 "" +2
    CreateDirectory "$TEMP\SecondLifeSettingsBackup\AllUsers\"
    CopyFiles /SILENT "$2\Application Data\SecondLife\*" "$TEMP\SecondLifeSettingsBackup\AllUsers\"
Pop $0

FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Restore user files from temp location
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function RestoreUserFiles

Push $0
Push $1
Push $2

    StrCpy $0 0 ; Index number used to iterate via EnumRegKey

  LOOP:
    EnumRegKey $1 HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows NT\CurrentVersion\ProfileList" $0
    StrCmp $1 "" DONE               ; no more users

    ReadRegStr $2 HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows NT\CurrentVersion\ProfileList\$1" "ProfileImagePath" 
    StrCmp $2 "" CONTINUE 0         ; "ProfileImagePath" value is missing

    ; Required since ProfileImagePath is of type REG_EXPAND_SZ
    ExpandEnvStrings $2 $2

    CreateDirectory "$2\Application Data\SecondLife\"
    CopyFiles /SILENT "$TEMP\SecondLifeSettingsBackup\$0\*" "$2\Application Data\SecondLife\" 

  CONTINUE:
    IntOp $0 $0 + 1
    Goto LOOP
  DONE:

Pop $2
Pop $1
Pop $0

; Copy files in Documents and Settings\All Users\SecondLife
Push $0
    ReadRegStr $0 HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\Shell Folders" "Common AppData"
    StrCmp $0 "" +2
    CreateDirectory "$2\Application Data\SecondLife\"
    CopyFiles /SILENT "$TEMP\SecondLifeSettingsBackup\AllUsers\*" "$2\Application Data\SecondLife\" 
Pop $0

FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Remove temp dirs
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function RemoveTempUserFiles

Push $0
Push $1
Push $2

    StrCpy $0 0 ; Index number used to iterate via EnumRegKey

  LOOP:
    EnumRegKey $1 HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows NT\CurrentVersion\ProfileList" $0
    StrCmp $1 "" DONE               ; no more users

    ReadRegStr $2 HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows NT\CurrentVersion\ProfileList\$1" "ProfileImagePath" 
    StrCmp $2 "" CONTINUE 0         ; "ProfileImagePath" value is missing

    ; Required since ProfileImagePath is of type REG_EXPAND_SZ
    ExpandEnvStrings $2 $2

    RMDir /r "$TEMP\SecondLifeSettingsBackup\$0\*"

  CONTINUE:
    IntOp $0 $0 + 1
    Goto LOOP
  DONE:

Pop $2
Pop $1
Pop $0

; Copy files in Documents and Settings\All Users\SecondLife
Push $0
    ReadRegStr $0 HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\Shell Folders" "Common AppData"
    StrCmp $0 "" +2
    RMDir /r "$TEMP\SecondLifeSettingsBackup\AllUsers\*"
Pop $0

FunctionEnd


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Clobber user files - TEST ONLY
; This is here for testing, generally not desirable to call it.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;Function ClobberUserFilesTESTONLY

;Push $0
;Push $1
;Push $2
;
;    StrCpy $0 0 ; Index number used to iterate via EnumRegKey
;
;  LOOP:
;    EnumRegKey $1 HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows NT\CurrentVersion\ProfileList" $0
;    StrCmp $1 "" DONE               ; no more users
;
;    ReadRegStr $2 HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows NT\CurrentVersion\ProfileList\$1" "ProfileImagePath" 
;    StrCmp $2 "" CONTINUE 0         ; "ProfileImagePath" value is missing
;
;    ; Required since ProfileImagePath is of type REG_EXPAND_SZ
;    ExpandEnvStrings $2 $2
;
;    RMDir /r "$2\Application Data\SecondLife\"
;
;  CONTINUE:
;    IntOp $0 $0 + 1
;    Goto LOOP
;  DONE:
;
;Pop $2
;Pop $1
;Pop $0
;
;; Copy files in Documents and Settings\All Users\SecondLife
;Push $0
;    ReadRegStr $0 HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\Shell Folders" "Common AppData"
;    StrCmp $0 "" +2
;    RMDir /r "$2\Application Data\SecondLife\"
;Pop $0
;
;FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Delete the installed shader files
;;; Since shaders are in active development, we'll likely need to shuffle them
;;; around a bit from build to build.  This ensures that shaders that we move
;;; or rename in the dev tree don't get left behind in the install.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function RemoveOldShaders

;; Remove old shader files first so fallbacks will work. see DEV-5663
RMDir /r "$INSTDIR\app_settings\shaders\*"

FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Delete the installed XUI files
;;; We've changed the directory hierarchy for skins, putting all XUI and texture
;;; files under a specific skin directory, i.e. skins/default/xui/en-us as opposed
;;; to skins/xui/en-us.  Need to clean up the old path when upgrading
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function RemoveOldXUI

;; remove old XUI and texture files
; <FS:Ansariel> FIRE-869: Delete all existing skins prior installation
;RmDir /r "$INSTDIR\skins\html"
;RmDir /r "$INSTDIR\skins\xui"
;RmDir /r "$INSTDIR\skins\textures"
;Delete "$INSTDIR\skins\*.txt"
RMDir /r "$INSTDIR\skins"
; </FS:Ansariel>

FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Remove any releasenotes files.
;;; We are no longer including release notes with the viewer. This will delete
;;; any that were left behind by an older installer. Delete will not fail if
;;; the files do not exist
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function RemoveOldReleaseNotes

;; remove releasenotes.txt file from application directory, and the shortcut
;; from the start menu.
Delete "$SMPROGRAMS\$INSTSHORTCUT\SL Release Notes.lnk"
Delete "$INSTDIR\releasenotes.txt"

FunctionEnd


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Delete files in Documents and Settings\<user>\SecondLife
; Delete files in Documents and Settings\All Users\SecondLife
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function un.DocumentsAndSettingsFolder

; Delete files in Documents and Settings\<user>\SecondLife
Push $0
Push $1
Push $2

  DetailPrint $(DeleteDocumentAndSettingsDP)

  StrCpy $0 0 ; Index number used to iterate via EnumRegKey

  LOOP:
    EnumRegKey $1 HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows NT\CurrentVersion\ProfileList" $0
    StrCmp $1 "" DONE               ; no more users

    ReadRegStr $2 HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows NT\CurrentVersion\ProfileList\$1" "ProfileImagePath" 
    StrCmp $2 "" CONTINUE 0         ; "ProfileImagePath" value is missing

    ; Required since ProfileImagePath is of type REG_EXPAND_SZ
    ExpandEnvStrings $2 $2

	; If uninstalling a normal install remove everything
	; Otherwise (preview/dmz etc) just remove cache

        # Local Settings directory is the cache, there is no "cache" subdir
        RMDir /r "$2\Local Settings\Application Data\CtrlAltStudio Viewer\user_settings"
	RMDir /r "$2\Local Settings\Application Data\CtrlAltStudio Viewer\data"
        # Vista version of the same
        RMDir /r "$2\AppData\Local\CtrlAltStudio Viewer\user_settings"
	RMDir /r "$2\AppData\Local\CtrlAltStudio Viewer\data"
    Delete  "$2\Application Data\CtrlAltStudio Viewer\*.bmp"
    Delete  "$2\Application Data\CtrlAltStudio Viewer\search_history.txt"
    Delete  "$2\Application Data\CtrlAltStudio Viewer\plugin_cookies.txt"
    Delete  "$2\Application Data\CtrlAltStudio Viewer\typed_locations.txt"

  CONTINUE:
    IntOp $0 $0 + 1
    Goto LOOP
  DONE:
  
  MessageBox MB_OK $(UnChatlogsNoticeMB)

Pop $2
Pop $1
Pop $0

; Delete files in Documents and Settings\All Users\CtrlAltStudio Viewer
Push $0
  ReadRegStr $0 HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\Shell Folders" "Common AppData"
  StrCmp $0 "" +2
  RMDir /r "$0\CtrlAltStudio Viewer"
Pop $0

; Delete files in C:\Windows\Application Data\SecondLife
; If the user is running on a pre-NT system, Application Data lives here instead of
; in Documents and Settings.
RMDir /r "$WINDIR\Application Data\CtrlAltStudio Viewer"

FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Close the program, if running. Modifies no variables.
; Allows user to bail out of uninstall process.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function un.CloseSecondLife
  Push $0
  FindWindow $0 "Second Life" ""
  IntCmp $0 0 DONE
  MessageBox MB_OKCANCEL $(CloseSecondLifeUnInstMB) IDOK CLOSE IDCANCEL CANCEL_UNINSTALL

  CANCEL_UNINSTALL:
    Quit

  CLOSE:
    DetailPrint $(CloseSecondLifeUnInstDP)
    SendMessage $0 16 0 0

  LOOP:
	  FindWindow $0 "Second Life" ""
	  IntCmp $0 0 DONE
	  Sleep 500
	  Goto LOOP

  DONE:
    Pop $0
    Return
FunctionEnd


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
;   Delete the stored password for the current Windows user
;   DEV-10821 -- Unauthorised user can gain access to an SL account after a real user has uninstalled
;
Function un.RemovePassword

DetailPrint $(UnRemovePasswordsDP)

SetShellVarContext current
Delete "$APPDATA\CtrlAltStudio Viewer\user_settings\password.dat"
SetShellVarContext all

FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Delete the installed files
;;; This deletes the uninstall executable, but it works 
;;; because it is copied to temp directory before running
;;;
;;; Note:  You must list all files here, because we only
;;; want to delete our files, not things users left in the
;;; application directories.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function un.ProgramFiles

;; Remove mozilla file first so recursive directory deletion doesn't get hung up
Delete "$INSTDIR\app_settings\mozilla\components"

;; This placeholder is replaced by the complete list of files to uninstall by viewer_manifest.py
%%DELETE_FILES%%

;; Optional/obsolete files.  Delete won't fail if they don't exist.
Delete "$INSTDIR\dronesettings.ini"
Delete "$INSTDIR\message_template.msg"
Delete "$INSTDIR\newview.pdb"
Delete "$INSTDIR\newview.map"
Delete "$INSTDIR\SecondLife.pdb"
Delete "$INSTDIR\SecondLife.map"
Delete "$INSTDIR\comm.dat"
Delete "$INSTDIR\*.glsl"
Delete "$INSTDIR\motions\*.lla"
Delete "$INSTDIR\trial\*.html"
Delete "$INSTDIR\newview.exe"
Delete "$INSTDIR\SecondLife.exe"
;; MAINT-3099 workaround - prevent these log files, if present, from causing a user alert
Delete "$INSTDIR\VivoxVoiceService-*.log"
;; Remove entire help directory
Delete "$INSTDIR\help\Advanced\*"
RMDir  "$INSTDIR\help\Advanced"
Delete "$INSTDIR\help\basics\*"
RMDir  "$INSTDIR\help\basics"
Delete "$INSTDIR\help\Concepts\*"
RMDir  "$INSTDIR\help\Concepts"
Delete "$INSTDIR\help\welcome\*"
RMDir  "$INSTDIR\help\welcome"
Delete "$INSTDIR\help\*"
RMDir  "$INSTDIR\help"

Delete "$INSTDIR\uninst.exe"
RMDir "$INSTDIR"

IfFileExists "$INSTDIR" FOLDERFOUND NOFOLDER

FOLDERFOUND:
  ; Silent uninstall always removes all files (/SD IDYES)
  MessageBox MB_YESNO $(DeleteProgramFilesMB) /SD IDYES IDNO NOFOLDER
  RMDir /r "$INSTDIR"

NOFOLDER:

FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Uninstall settings
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
UninstallText $(UninstallTextMsg)
;<FS:TM> Expose detail button instead of always showing detailed uninstall info
;ShowUninstDetails show 

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Uninstall section
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Section Uninstall

; Start with some default values.
StrCpy $INSTPROG "${INSTNAME}"
StrCpy $INSTEXE "${INSTEXE}"
StrCpy $INSTSHORTCUT "${SHORTCUT}"
Call un.CheckIfAdministrator		; Make sure the user can install/uninstall

; uninstall for all users (if you change this, change it in the install as well)
SetShellVarContext all			

; Make sure we're not running
Call un.CloseSecondLife

; Clean up registry keys and subkeys (these should all be !defines somewhere)
DeleteRegKey HKEY_LOCAL_MACHINE "SOFTWARE\CtrlAltStudio Viewer\$INSTPROG"
DeleteRegKey HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\$INSTPROG"

; <FS:ND> BUG-2707 Remove entry that disabled SEHOP
DeleteRegKey HKEY_LOCAL_MACHINE "Software\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\$INSTEXE"

; Clean up shortcuts
Delete "$SMPROGRAMS\$INSTSHORTCUT\*.*"
RMDir  "$SMPROGRAMS\$INSTSHORTCUT"

Delete "$DESKTOP\$INSTSHORTCUT.lnk"
Delete "$INSTDIR\$INSTSHORTCUT.lnk"
Delete "$INSTDIR\Uninstall $INSTSHORTCUT.lnk"

; Clean up cache and log files.
; Leave them in-place for non AGNI installs.

!ifdef UNINSTALL_SETTINGS
Call un.DocumentsAndSettingsFolder
!endif

; remove stored password on uninstall
Call un.RemovePassword

Call un.ProgramFiles

SectionEnd 				; end of uninstall section

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;  Note: to add new languages, add a language file include to the list 
;;  at the top of this file, add an entry to the menu and then add an 
;;  entry to the language ID selector below
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function .onInit
Call CheckWindowsVersion		;Check the version of windows we are installing on
    Push $0
    ${GetParameters} $COMMANDLINE              ; get our command line

    ${GetOptions} $COMMANDLINE "/SKIP_DIALOGS" $0   
    ; <FS:Ansariel> Auto-close if auto-updating
    ; IfErrors +2 0 ; If error jump past setting SKIP_DIALOGS
    ;    StrCpy $SKIP_DIALOGS "true"
    IfErrors +3 0 ; If error jump past setting SKIP_DIALOGS
        StrCpy $SKIP_DIALOGS "true"
        SetAutoClose true
    ; </FS:Ansariel>

	; <FS:PP> Disable autorun
	; ${GetOptions} $COMMANDLINE "/SKIP_AUTORUN" $0
    ; IfErrors +2 0 ; If error jump past setting SKIP_AUTORUN
	; 	StrCpy $SKIP_AUTORUN "true"
	; </FS:PP>

    ${GetOptions} $COMMANDLINE "/LANGID=" $0   ; /LANGID=1033 implies US English

    ; If no language (error), then proceed
    IfErrors lbl_configure_default_lang
    ; No error means we got a language, so use it
    StrCpy $LANGUAGE $0
    Goto lbl_return

lbl_configure_default_lang:
    ; If we currently have a version of SL installed, default to the language of that install
    ; Otherwise don't change $LANGUAGE and it will default to the OS UI language.
    ReadRegStr $0 HKEY_LOCAL_MACHINE "SOFTWARE\CtrlAltStudio Viewer\${INSTNAME}" "InstallerLanguage"
    IfErrors +2 0 ; If error skip the copy instruction 
	StrCpy $LANGUAGE $0

    ; For silent installs, no language prompt, use default
    IfSilent lbl_return
    StrCmp $SKIP_DIALOGS "true" lbl_return

; <FS:Ansariel> Commented out; Warning in build log about not being used
;lbl_build_menu:
; </FS:Ansariel> Commented out; Warning in build log about not being used
	Push ""
    # Use separate file so labels can be UTF-16 but we can still merge changes
    # into this ASCII file. JC
    !include "%%SOURCE%%\installers\windows\language_menu.nsi"
    
	Push A ; A means auto count languages for the auto count to work the first empty push (Push "") must remain
	LangDLL::LangDialog $(InstallerLanguageTitle) $(SelectInstallerLanguage)
	Pop $0
	StrCmp $0 "cancel" 0 +2
		Abort
    StrCpy $LANGUAGE $0

	; save language in registry		
	WriteRegStr HKEY_LOCAL_MACHINE "SOFTWARE\CtrlAltStudio Viewer\${INSTNAME}" "InstallerLanguage" $LANGUAGE
lbl_return:
    Pop $0
    Return
FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Function un.onInit
	; read language from registry and set for uninstaller
    ; Key will be removed on successful uninstall
	ReadRegStr $0 HKEY_LOCAL_MACHINE "SOFTWARE\CtrlAltStudio Viewer\${INSTNAME}" "InstallerLanguage"
    IfErrors lbl_end
	StrCpy $LANGUAGE $0
lbl_end:
    Return
FunctionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; MAIN SECTION
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
Section ""						; (default section)

SetShellVarContext all			; install for all users (if you change this, change it in the uninstall as well)

; Start with some default values.
StrCpy $INSTPROG "${INSTNAME}"
StrCpy $INSTEXE "${INSTEXE}"
StrCpy $INSTSHORTCUT "${SHORTCUT}"

Call CheckCPUFlags				; Make sure we have SSE2 support
Call CheckIfAdministrator		; Make sure the user can install/uninstall
Call CheckIfAlreadyCurrent		; Make sure that we haven't already installed this version
Call CloseSecondLife			; Make sure we're not running
Call CheckNetworkConnection		; ping secondlife.com
Call CheckWillUninstallV2		; See if a V2 install exists and will be removed.
Call CheckOldExeName                    ; Clean up a previous version of the exe

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
StrCmp $DO_UNINSTALL_V2 "" PRESERVE_DONE
  Call PreserveUserFiles
PRESERVE_DONE:

;;; Don't remove cache files during a regular install, removing the inventory cache on upgrades results in lots of damage to the servers.
;Call RemoveCacheFiles			; Installing over removes potentially corrupted
								; VFS and cache files.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Need to clean out shader files from previous installs to fix DEV-5663
Call RemoveOldShaders

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Need to clean out old XUI files that predate skinning
Call RemoveOldXUI

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Clear out old releasenotes.txt files. These are now on the public wiki.
Call RemoveOldReleaseNotes

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Files
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; This placeholder is replaced by the complete list of all the files in the installer, by viewer_manifest.py
%%INSTALL_FILES%%

# Pass the installer's language to the client to use as a default
StrCpy $SHORTCUT_LANG_PARAM "--set InstallLanguage $(LanguageCode)"

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Shortcuts in start menu
; <FS:Ansariel> Optional start menu entry
StrCmp $NO_STARTMENU "true" label_skip_start_menu
; </FS:Ansariel>

CreateDirectory	"$SMPROGRAMS\$INSTSHORTCUT"
SetOutPath "$INSTDIR"
CreateShortCut	"$SMPROGRAMS\$INSTSHORTCUT\$INSTSHORTCUT.lnk" \
				"$INSTDIR\$INSTEXE" "$SHORTCUT_LANG_PARAM"


WriteINIStr		"$SMPROGRAMS\$INSTSHORTCUT\SL Create Account.url" \
				"InternetShortcut" "URL" \
				"http://join.secondlife.com/"
WriteINIStr		"$SMPROGRAMS\$INSTSHORTCUT\SL Your Account.url" \
				"InternetShortcut" "URL" \
				"http://www.secondlife.com/account/"
WriteINIStr		"$SMPROGRAMS\$INSTSHORTCUT\LSL Scripting Language Help.url" \
				"InternetShortcut" "URL" \
                "http://wiki.secondlife.com/wiki/LSL_Portal"
CreateShortCut	"$SMPROGRAMS\$INSTSHORTCUT\Uninstall $INSTSHORTCUT.lnk" \
				'"$INSTDIR\uninst.exe"' ''

; <FS:Ansariel> Optional start menu entry
label_skip_start_menu:
; </FS:Ansariel>

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Other shortcuts
SetOutPath "$INSTDIR"
CreateShortCut "$DESKTOP\$INSTSHORTCUT.lnk" \
        "$INSTDIR\$INSTEXE" "$SHORTCUT_LANG_PARAM"
CreateShortCut "$INSTDIR\$INSTSHORTCUT.lnk" \
        "$INSTDIR\$INSTEXE" "$SHORTCUT_LANG_PARAM"
CreateShortCut "$INSTDIR\Uninstall $INSTSHORTCUT.lnk" \
				'"$INSTDIR\uninst.exe"' ''


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Write registry
WriteRegStr HKEY_LOCAL_MACHINE "SOFTWARE\CtrlAltStudio Viewer\$INSTPROG" "" "$INSTDIR"
WriteRegStr HKEY_LOCAL_MACHINE "SOFTWARE\CtrlAltStudio Viewer\$INSTPROG" "Version" "${VERSION_LONG}"
WriteRegStr HKEY_LOCAL_MACHINE "SOFTWARE\CtrlAltStudio Viewer\$INSTPROG" "Shortcut" "$INSTSHORTCUT"
WriteRegStr HKEY_LOCAL_MACHINE "SOFTWARE\CtrlAltStudio Viewer\$INSTPROG" "Exe" "$INSTEXE"
WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\$INSTPROG" "DisplayName" "$INSTPROG (remove only)"
WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\$INSTPROG" "UninstallString" '"$INSTDIR\uninst.exe"'
; <FS:Ansariel> Add additional data for uninstall list in Windows
WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\$INSTPROG" "Publisher" "CtrlAltStudio"
WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\$INSTPROG" "URLInfoAbout" "http://ctrlaltstudio.com/viewer"
WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\$INSTPROG" "URLUpdateInfo" "http://ctrlaltstudio.com/viewer"
WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\$INSTPROG" "HelpLink" "http://ctrlaltstudio.com/viewer"
WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\$INSTPROG" "DisplayIcon" '"$INSTDIR\$INSTEXE"'
WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\$INSTPROG" "DisplayVersion" "${VERSION_LONG}"
WriteRegDWORD HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\$INSTPROG" "EstimatedSize" "0x00030C00" ; 195 MB
; </FS:Ansariel>

; <FS:ND> BUG-2707 Disable SEHOP for installed viewer.
WriteRegDWORD HKEY_LOCAL_MACHINE "Software\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\$INSTEXE" "DisableExceptionChainValidation" 1

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Write URL registry info
WriteRegStr HKEY_CLASSES_ROOT "${URLNAME}" "(default)" "URL:Second Life"
WriteRegStr HKEY_CLASSES_ROOT "${URLNAME}" "URL Protocol" ""
WriteRegStr HKEY_CLASSES_ROOT "${URLNAME}\DefaultIcon" "" '"$INSTDIR\$INSTEXE"'
;; URL param must be last item passed to viewer, it ignores subsequent params
;; to avoid parameter injection attacks.
WriteRegExpandStr HKEY_CLASSES_ROOT "${URLNAME}\shell\open\command" "" '"$INSTDIR\$INSTEXE" -url "%1"'
WriteRegStr HKEY_CLASSES_ROOT "x-grid-location-info"(default)" "URL:Second Life"
WriteRegStr HKEY_CLASSES_ROOT "x-grid-location-info" "URL Protocol" ""
WriteRegStr HKEY_CLASSES_ROOT "x-grid-location-info\DefaultIcon" "" '"$INSTDIR\$INSTEXE"'
;; URL param must be last item passed to viewer, it ignores subsequent params
;; to avoid parameter injection attacks.
WriteRegExpandStr HKEY_CLASSES_ROOT "x-grid-location-info\shell\open\command" "" '"$INSTDIR\$INSTEXE" -url "%1"'

; <FS:CR> Register hop:// protocol registry info
WriteRegStr HKEY_CLASSES_ROOT "hop" "(default)" "URL:Second Life"
WriteRegStr HKEY_CLASSES_ROOT "hop" "URL Protocol" ""
WriteRegStr HKEY_CLASSES_ROOT "hop\DefaultIcon" "" '"$INSTDIR\$INSTEXE"'
WriteRegExpandStr HKEY_CLASSES_ROOT "hop\shell\open\command" "" '"$INSTDIR\$INSTEXE" -url "%1"'
; </FS:CR>

; write out uninstaller
WriteUninstaller "$INSTDIR\uninst.exe"

; Uninstall existing "Second Life Viewer 2" install if needed.
StrCmp $DO_UNINSTALL_V2 "" REMOVE_SLV2_DONE
  ExecWait '"$PROGRAMFILES\SecondLifeViewer2\uninst.exe" /S _?=$PROGRAMFILES\SecondLifeViewer2'
  Delete "$PROGRAMFILES\SecondLifeViewer2\uninst.exe" ; with _? option above, uninst.exe will be left behind.
  RMDir "$PROGRAMFILES\SecondLifeViewer2" ; will remove only if empty.

  Call RestoreUserFiles
  Call RemoveTempUserFiles
REMOVE_SLV2_DONE:

; end of default section
SectionEnd

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; EOF  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
