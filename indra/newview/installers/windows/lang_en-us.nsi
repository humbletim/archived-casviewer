; First is default
LoadLanguageFile "${NSISDIR}\Contrib\Language files\English.nlf"

; Language selection dialog
LangString InstallerLanguageTitle  ${LANG_ENGLISH} "Installer Language"
LangString SelectInstallerLanguage  ${LANG_ENGLISH} "Please select the language of the installer"

; subtitle on license text caption
LangString LicenseSubTitleUpdate ${LANG_ENGLISH} " Update"
LangString LicenseSubTitleSetup ${LANG_ENGLISH} " Setup"

; installation directory text
LangString DirectoryChooseTitle ${LANG_ENGLISH} "Installation Directory" 
LangString DirectoryChooseUpdate ${LANG_ENGLISH} "Select the CtrlAltStudio Viewer directory to update to version ${VERSION_LONG}.(XXX):"
LangString DirectoryChooseSetup ${LANG_ENGLISH} "Select the directory to install CtrlAltStudio Viewer in:"

; CheckStartupParams message box
LangString CheckStartupParamsMB ${LANG_ENGLISH} "Could not find the program '$INSTPROG'. Silent update failed."

; installation success dialog
LangString InstSuccesssQuestion ${LANG_ENGLISH} "Start CtrlAltStudio Viewer now?"

; remove old NSIS version
LangString RemoveOldNSISVersion ${LANG_ENGLISH} "Checking for old version..."

; check windows version
LangString CheckWindowsVersionDP ${LANG_ENGLISH} "Checking Windows version..."
LangString CheckWindowsVersionMB ${LANG_ENGLISH} "CtrlAltStudio Viewer only supports Windows XP SP3 and later.$\nInstallation on this Operating System is not supported. Quitting."
LangString CheckWindowsServPackMB ${LANG_ENGLISH} "It is recomended to run CtrlAltStudio Viewer on the latest service pack for your operating system.$\nThis will help with performance and stability of the program."
LangString UseLatestServPackDP ${LANG_ENGLISH} "Please use Windows Update to install the latest Service Pack."

; checkifadministrator function (install)
LangString CheckAdministratorInstDP ${LANG_ENGLISH} "Checking for permission to install..."
LangString CheckAdministratorInstMB ${LANG_ENGLISH} 'You appear to be using a "limited" account.$\nYou must be an "administrator" to install CtrlAltStudio Viewer.'

; checkifadministrator function (uninstall)
LangString CheckAdministratorUnInstDP ${LANG_ENGLISH} "Checking for permission to uninstall..."
LangString CheckAdministratorUnInstMB ${LANG_ENGLISH} 'You appear to be using a "limited" account.$\nYou must be an "administrator" to uninstall CtrlAltStudio Viewer.'

; checkifalreadycurrent
LangString CheckIfCurrentMB ${LANG_ENGLISH} "It appears that CtrlAltStudio Viewer ${VERSION_LONG} is already installed.$\n$\nWould you like to install it again?"

; checkcpuflags
LangString MissingSSE2 ${LANG_ENGLISH} "This machine may not have a CPU with SSE2 support, which is required to run CtrlAltStudio Viewer ${VERSION_LONG}. Do you want to continue?"

; closesecondlife function (install)
LangString CloseSecondLifeInstDP ${LANG_ENGLISH} "Waiting for CtrlAltStudio Viewer to shut down..."
LangString CloseSecondLifeInstMB ${LANG_ENGLISH} "CtrlAltStudio Viewer can't be installed while it is already running.$\n$\nFinish what you're doing then select OK to close CtrlAltStudio Viewer and continue.$\nSelect CANCEL to cancel installation."

; closesecondlife function (uninstall)
LangString CloseSecondLifeUnInstDP ${LANG_ENGLISH} "Waiting for CtrlAltStudio Viewer to shut down..."
LangString CloseSecondLifeUnInstMB ${LANG_ENGLISH} "CtrlAltStudio Viewer can't be uninstalled while it is already running.$\n$\nFinish what you're doing then select OK to close CtrlAltStudio Viewer and continue.$\nSelect CANCEL to cancel."

; CheckNetworkConnection
LangString CheckNetworkConnectionDP ${LANG_ENGLISH} "Checking network connection..."

; removecachefiles
LangString RemoveCacheFilesDP ${LANG_ENGLISH} "Deleting cache files in Documents and Settings folder"

; delete program files
LangString DeleteProgramFilesMB ${LANG_ENGLISH} "There are still files in your CtrlAltStudio Viewer program directory.$\n$\nThese are possibly files you created or moved to:$\n$INSTDIR$\n$\nDo you want to remove them?"

; uninstall text
LangString UninstallTextMsg ${LANG_ENGLISH} "This will uninstall CtrlAltStudio Viewer ${VERSION_LONG} from your system."

; <FS:Ansariel> Optional start menu entry
LangString CreateStartMenuEntry ${LANG_ENGLISH} "Create an entry in the start menu?"

LangString DeleteDocumentAndSettingsDP ${LANG_ENGLISH} "Deleting files in Documents and Settings folder."
LangString UnChatlogsNoticeMB ${LANG_ENGLISH} "This uninstall will NOT delete your CtrlAltStudio Viewer chat logs and other private files. If you want to do that yourself, delete the CtrlAltStudio Viewer folder within your user Application data folder."
LangString UnRemovePasswordsDP ${LANG_ENGLISH} "Removing CtrlAltStudio Viewer saved passwords."
