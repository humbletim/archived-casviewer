; First is default
LoadLanguageFile "${NSISDIR}\Contrib\Language files\Italian.nlf"

; Language selection dialog
LangString InstallerLanguageTitle  ${LANG_ITALIAN} "Linguaggio del programma di installazione"
LangString SelectInstallerLanguage  ${LANG_ITALIAN} "Scegliere per favore il linguaggio del programma di installazione"

; subtitle on license text caption
LangString LicenseSubTitleUpdate ${LANG_ITALIAN} " Update"
LangString LicenseSubTitleSetup ${LANG_ITALIAN} " Setup"

; installation directory text
LangString DirectoryChooseTitle ${LANG_ITALIAN} "Directory di installazione" 
LangString DirectoryChooseUpdate ${LANG_ITALIAN} "Scegli la directory di CtrlAltStudio Viewer per l’update alla versione ${VERSION_LONG}.(XXX):"
LangString DirectoryChooseSetup ${LANG_ITALIAN} "Scegli la directory dove installare CtrlAltStudio Viewer:"

; CheckStartupParams message box
LangString CheckStartupParamsMB ${LANG_ITALIAN} "Non riesco a trovare il programma '$INSTPROG'. Silent Update fallito."

; installation success dialog
LangString InstSuccesssQuestion ${LANG_ITALIAN} "Avvia ora CtrlAltStudio Viewer?"

; remove old NSIS version
LangString RemoveOldNSISVersion ${LANG_ITALIAN} "Controllo delle precedenti versioni…"

; check windows version
LangString CheckWindowsVersionDP ${LANG_ITALIAN} "Controllo della versione di Windows…"
LangString CheckWindowsVersionMB ${LANG_ITALIAN} 'CtrlAltStudio Viewer supporta solo Windows XP e Mac OS X.$\n$\nTentare l’installazione su Windows $R0 può provocare blocchi di sistema e perdita di dati.$\n$\nInstallare comunque?'
LangString CheckWindowsServPackMB ${LANG_ITALIAN} "It is recomended to run CtrlAltStudio Viewer on the latest service pack for your operating system.$\nThis will help with performance and stability of the program."
LangString UseLatestServPackDP ${LANG_ITALIAN} "Please use Windows Update to install the latest Service Pack."

; checkifadministrator function (install)
LangString CheckAdministratorInstDP ${LANG_ITALIAN} "Controllo del permesso di installazione…"
LangString CheckAdministratorInstMB ${LANG_ITALIAN} 'Stai utilizzando un account “limitato”.$\nSolo un “amministratore” può installare CtrlAltStudio Viewer.'

; checkifadministrator function (uninstall)
LangString CheckAdministratorUnInstDP ${LANG_ITALIAN} "Controllo del permesso di installazione…"
LangString CheckAdministratorUnInstMB ${LANG_ITALIAN} 'Stai utilizzando un account “limitato”.$\nSolo un “amministratore” può installare CtrlAltStudio Viewer.'

; checkifalreadycurrent
LangString CheckIfCurrentMB ${LANG_ITALIAN} "CtrlAltStudio Viewer ${VERSION_LONG} è stato sia già installato.$\n$\nVuoi ripetere l’installazione?"

; checkcpuflags
LangString MissingSSE2 ${LANG_ITALIAN} "This machine may not have a CPU with SSE2 support, which is required to run SecondLife ${VERSION_LONG}. Do you want to continue?"

; closesecondlife function (install)
LangString CloseSecondLifeInstDP ${LANG_ITALIAN} "In attesa che CtrlAltStudio Viewer chiuda…"
LangString CloseSecondLifeInstMB ${LANG_ITALIAN} "Non è possibile installare CtrlAltStudio Viewer se è già in funzione..$\n$\nTermina le operazioni in corso e scegli OK per chiudere CtrlAltStudio Viewer e continuare.$\nScegli CANCELLA per annullare l’installazione."

; closesecondlife function (uninstall)
LangString CloseSecondLifeUnInstDP ${LANG_ITALIAN} "In attesa della chiusura di CtrlAltStudio Viewer…"
LangString CloseSecondLifeUnInstMB ${LANG_ITALIAN} "Non è possibile installare CtrlAltStudio Viewer se è già in funzione.$\n$\nTermina le operazioni in corso e scegli OK per chiudere CtrlAltStudio Viewer e continuare.$\nScegli CANCELLA per annullare."

; CheckNetworkConnection
LangString CheckNetworkConnectionDP ${LANG_ITALIAN} "Verifica connessione di rete in corso..."

; removecachefiles
LangString RemoveCacheFilesDP ${LANG_ITALIAN} "Cancellazione dei file cache nella cartella Documents and Settings"

; delete program files
LangString DeleteProgramFilesMB ${LANG_ITALIAN} "Sono ancora presenti dei file nella directory programmi di CtrlAltStudio Viewer.$\n$\nPotrebbe trattarsi di file creati o trasferiti in:$\n$INSTDIR$\n$\nVuoi cancellarli?"

; uninstall text
LangString UninstallTextMsg ${LANG_ITALIAN} "Così facendo CtrlAltStudio Viewer verrà disinstallato ${VERSION_LONG} dal tuo sistema."

; <FS:Ansariel> Optional start menu entry
LangString CreateStartMenuEntry ${LANG_ITALIAN} "Create an entry in the start menu?"

LangString DeleteDocumentAndSettingsDP ${LANG_ITALIAN} "Deleting files in Documents and Settings folder."
LangString UnChatlogsNoticeMB ${LANG_ITALIAN} "This uninstall will NOT delete your CtrlAltStudio Viewer chat logs and other private files. If you want to do that yourself, delete the CtrlAltStudio Viewer folder within your user Application data folder."
LangString UnRemovePasswordsDP ${LANG_ITALIAN} "Removing CtrlAltStudio Viewer saved passwords."
