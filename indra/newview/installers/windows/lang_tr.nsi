; First is default
LoadLanguageFile "${NSISDIR}\Contrib\Language files\Turkish.nlf"

; Language selection dialog
LangString InstallerLanguageTitle  ${LANG_TURKISH} "Yükleyici Dili"
LangString SelectInstallerLanguage  ${LANG_TURKISH} "Lütfen yükleyicinin dilini seçin"

; subtitle on license text caption
LangString LicenseSubTitleUpdate ${LANG_TURKISH} "Güncelleştir"
LangString LicenseSubTitleSetup ${LANG_TURKISH} "Ayarlar"

; installation directory text
LangString DirectoryChooseTitle ${LANG_TURKISH} "Yükleme Dizini" 
LangString DirectoryChooseUpdate ${LANG_TURKISH} "${VERSION_LONG}.(XXX) sürümüne güncelleştirme yapmak için CtrlAltStudio Viewer dizinini seçin:"
LangString DirectoryChooseSetup ${LANG_TURKISH} "CtrlAltStudio Viewer'ın yükleneceği dizini seçin:"

; CheckStartupParams message box
LangString CheckStartupParamsMB ${LANG_TURKISH} "'$INSTPROG' programı bulunamadı. Sessiz güncelleştirme başarılamadı."

; installation success dialog
LangString InstSuccesssQuestion ${LANG_TURKISH} "CtrlAltStudio Viewer şimdi başlatılsın mı?"

; remove old NSIS version
LangString RemoveOldNSISVersion ${LANG_TURKISH} "Eski sürüm kontrol ediliyor..."

; check windows version
LangString CheckWindowsVersionDP ${LANG_TURKISH} "Windows sürümü kontrol ediliyor..."
LangString CheckWindowsVersionMB ${LANG_TURKISH} "CtrlAltStudio Viewer sadece Windows XP ve Mac OS X'i destekler.$\n$\nWindows $R0 üzerine yüklemeye çalışmak sistem çökmelerine ve veri kaybına neden olabilir.$\n$\nYine de yüklensin mi?"
LangString CheckWindowsServPackMB ${LANG_TURKISH} "It is recomended to run CtrlAltStudio Viewer on the latest service pack for your operating system.$\nThis will help with performance and stability of the program."
LangString UseLatestServPackDP ${LANG_TURKISH} "Please use Windows Update to install the latest Service Pack."

; checkifadministrator function (install)
LangString CheckAdministratorInstDP ${LANG_TURKISH} "Yükleme izni kontrol ediliyor..."
LangString CheckAdministratorInstMB ${LANG_TURKISH} "'Sınırlı' bir hesap kullanıyor görünüyorsunuz.$\nCtrlAltStudio Viewer'ı yüklemek için bir 'yönetici' olmalısınız."

; checkifadministrator function (uninstall)
LangString CheckAdministratorUnInstDP ${LANG_TURKISH} "Kaldırma izni kontrol ediliyor..."
LangString CheckAdministratorUnInstMB ${LANG_TURKISH} "'Sınırlı' bir hesap kullanıyor görünüyorsunuz.$\nCtrlAltStudio Viewer'ı kaldırmak için bir 'yönetici' olmalısınız."

; checkifalreadycurrent
LangString CheckIfCurrentMB ${LANG_TURKISH} "CtrlAltStudio Viewer ${VERSION_LONG} zaten yüklü.$\n$\nTekrar yüklemek ister misiniz?"

; checkcpuflags
LangString MissingSSE2 ${LANG_TURKISH} "Bu makinede SSE2 desteğine sahip bir CPU bulunmayabilir, SecondLife ${VERSION_LONG} çalıştırmak için bu gereklidir. Devam etmek istiyor musunuz?"

; closesecondlife function (install)
LangString CloseSecondLifeInstDP ${LANG_TURKISH} "CtrlAltStudio Viewer'ın kapatılması bekleniyor..."
LangString CloseSecondLifeInstMB ${LANG_TURKISH} "CtrlAltStudio Viewer zaten çalışırken kapatılamaz.$\n$\nYaptığınız işi bitirdikten sonra CtrlAltStudio Viewer'ı kapatmak ve devam etmek için Tamam seçimini yapın.$\nYüklemeyi iptal etmek için İPTAL seçimini yapın."

; closesecondlife function (uninstall)
LangString CloseSecondLifeUnInstDP ${LANG_TURKISH} "CtrlAltStudio Viewer'ın kapatılması bekleniyor..."
LangString CloseSecondLifeUnInstMB ${LANG_TURKISH} "CtrlAltStudio Viewer zaten çalışırken kaldırılamaz.$\n$\nYaptığınız işi bitirdikten sonra CtrlAltStudio Viewer'ı kapatmak ve devam etmek için Tamam seçimini yapın.$\nİptal etmek için İPTAL seçimini yapın."

; CheckNetworkConnection
LangString CheckNetworkConnectionDP ${LANG_TURKISH} "Ağ bağlantısı kontrol ediliyor..."

; removecachefiles
LangString RemoveCacheFilesDP ${LANG_TURKISH} "Belgeler ve Ayarlar klasöründeki önbellek dosyaları siliniyor"

; delete program files
LangString DeleteProgramFilesMB ${LANG_TURKISH} "SecondLife program dizininizde hala dosyalar var.$\n$\nBunlar muhtemelen sizin oluşturduğunuz veya şuraya taşıdığınız dosyalar:$\n$INSTDIR$\n$\nBunları kaldırmak istiyor musunuz?"

; uninstall text
LangString UninstallTextMsg ${LANG_TURKISH} "Bu adımla CtrlAltStudio Viewer ${VERSION_LONG} sisteminizden kaldırılacaktır."

; <FS:Ansariel> Optional start menu entry
LangString CreateStartMenuEntry ${LANG_TURKISH} "Create an entry in the start menu?"

LangString DeleteDocumentAndSettingsDP ${LANG_TURKISH} "Deleting files in Documents and Settings folder."
LangString UnChatlogsNoticeMB ${LANG_TURKISH} "This uninstall will NOT delete your CtrlAltStudio Viewer chat logs and other private files. If you want to do that yourself, delete the CtrlAltStudio Viewer folder within your user Application data folder."
LangString UnRemovePasswordsDP ${LANG_TURKISH} "Removing CtrlAltStudio Viewer saved passwords."
