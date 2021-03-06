; First is default
LoadLanguageFile "${NSISDIR}\Contrib\Language files\PortugueseBR.nlf"

; Language selection dialog
LangString InstallerLanguageTitle  ${LANG_PORTUGUESEBR} "Instalador Linguagem"
LangString SelectInstallerLanguage  ${LANG_PORTUGUESEBR} "Por favor seleccione a linguagem do instalador"

; subtitle on license text caption
LangString LicenseSubTitleUpdate ${LANG_PORTUGUESEBR} " Atualizar"
LangString LicenseSubTitleSetup ${LANG_PORTUGUESEBR} " Instalar"

; installation directory text
LangString DirectoryChooseTitle ${LANG_PORTUGUESEBR} "Diretório de Instalação" 
LangString DirectoryChooseUpdate ${LANG_PORTUGUESEBR} "Selecione o diretório do CtrlAltStudio Viewer para atualizar para a versão ${VERSION_LONG}.(XXX):"
LangString DirectoryChooseSetup ${LANG_PORTUGUESEBR} "Selecione o diretório para a instalação do CtrlAltStudio Viewer em:"

; CheckStartupParams message box
LangString CheckStartupParamsMB ${LANG_PORTUGUESEBR} "Não é possível encontrar o programa '$INSTPROG'. Ocorreu uma falha na atualização silenciosa."

; installation success dialog
LangString InstSuccesssQuestion ${LANG_PORTUGUESEBR} "Iniciar o CtrlAltStudio Viewer agora?"

; remove old NSIS version
LangString RemoveOldNSISVersion ${LANG_PORTUGUESEBR} "Verificando a versão anterior..."

; check windows version
LangString CheckWindowsVersionDP ${LANG_PORTUGUESEBR} "Verificando a versão do Windows..."
LangString CheckWindowsVersionMB ${LANG_PORTUGUESEBR} 'O CtrlAltStudio Viewer suporta apenas Windows XP e Mac OS X.$\n$\nA tentativa de instalar no Windows $R0 pode resultar em falhas e perda de dados.$\n$\nInstalar assim mesmo?'
LangString CheckWindowsServPackMB ${LANG_PORTUGUESEBR} "It is recomended to run CtrlAltStudio Viewer on the latest service pack for your operating system.$\nThis will help with performance and stability of the program."
LangString UseLatestServPackDP ${LANG_PORTUGUESEBR} "Please use Windows Update to install the latest Service Pack."

; checkifadministrator function (install)
LangString CheckAdministratorInstDP ${LANG_PORTUGUESEBR} "Verificando a permissão para instalação..."
LangString CheckAdministratorInstMB ${LANG_PORTUGUESEBR} 'Você parece estar usando uma conta "limitada".$\nVocê deve ser um "administrador" para poder instalar o CtrlAltStudio Viewer.'

; checkifadministrator function (uninstall)
LangString CheckAdministratorUnInstDP ${LANG_PORTUGUESEBR} "Verificando a permissão para desinstalação..."
LangString CheckAdministratorUnInstMB ${LANG_PORTUGUESEBR} 'Você parece estar usando uma conta "limitada".$\nVocê deve ser um "administrador" para poder desinstalar o CtrlAltStudio Viewer.'

; checkifalreadycurrent
LangString CheckIfCurrentMB ${LANG_PORTUGUESEBR} "Parece que o CtrlAltStudio Viewer ${VERSION_LONG} já está instalado.$\n$\nDeseja instalar novamente?"

; checkcpuflags
LangString MissingSSE2 ${LANG_PORTUGUESEBR} "This machine may not have a CPU with SSE2 support, which is required to run SecondLife ${VERSION_LONG}. Do you want to continue?"

; closesecondlife function (install)
LangString CloseSecondLifeInstDP ${LANG_PORTUGUESEBR} "Esperando o encerramento do CtrlAltStudio Viewer..."
LangString CloseSecondLifeInstMB ${LANG_PORTUGUESEBR} "O CtrlAltStudio Viewer não pode ser instalado enquanto ainda está sendo executado.$\n$\nTermine o que estava fazendo e selecione OK para fechar o CtrlAltStudio Viewer e continuar.$\nSelecione CANCELAR para cancelar a instalação."

; closesecondlife function (uninstall)
LangString CloseSecondLifeUnInstDP ${LANG_PORTUGUESEBR} "Esperando o encerramento do CtrlAltStudio Viewer..."
LangString CloseSecondLifeUnInstMB ${LANG_PORTUGUESEBR} "O CtrlAltStudio Viewer não pode ser desinstalado enquanto ainda está sendo executado.$\n$\nTermine o que estava fazendo e selecione OK para fechar o CtrlAltStudio Viewer e continuar.$\nSelecione CANCELAR para cancelar."

; CheckNetworkConnection
LangString CheckNetworkConnectionDP ${LANG_PORTUGUESEBR} "Verificando a conexão de rede..."

; removecachefiles
LangString RemoveCacheFilesDP ${LANG_PORTUGUESEBR} "Excluindo arquivos de cache na pasta Documents and Settings"

; delete program files
LangString DeleteProgramFilesMB ${LANG_PORTUGUESEBR} "Ainda existem arquivos em seu diretório do programa CtrlAltStudio Viewer.$\n$\nProvavelmente são arquivos que você criou ou moveu para:$\n$INSTDIR$\n$\nDeseja removê-los?"

; uninstall text
LangString UninstallTextMsg ${LANG_PORTUGUESEBR} "Isso desinstalará o CtrlAltStudio Viewer ${VERSION_LONG} do seu sistema."

; <FS:Ansariel> Optional start menu entry
LangString CreateStartMenuEntry ${LANG_PORTUGUESEBR} "Create an entry in the start menu?"

LangString DeleteDocumentAndSettingsDP ${LANG_PORTUGUESEBR} "Deleting files in Documents and Settings folder."
LangString UnChatlogsNoticeMB ${LANG_PORTUGUESEBR} "This uninstall will NOT delete your CtrlAltStudio Viewer chat logs and other private files. If you want to do that yourself, delete the CtrlAltStudio Viewer folder within your user Application data folder."
LangString UnRemovePasswordsDP ${LANG_PORTUGUESEBR} "Removing CtrlAltStudio Viewer saved passwords."
