#!/usr/bin/env python
"""\
@file viewer_manifest.py
@author Ryan Williams
@brief Description of all installer viewer files, and methods for packaging
       them into installers for all supported platforms.

$LicenseInfo:firstyear=2006&license=viewerlgpl$
Second Life Viewer Source Code
Copyright (C) 2006-2014, Linden Research, Inc.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation;
version 2.1 of the License only.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
$/LicenseInfo$
"""
import sys
import os.path
import errno
import re
import tarfile
import time
import random
#AO
import os
import shlex
import subprocess
import zipfile
#/AO

from fs_viewer_manifest import FSViewerManifest #<FS:ND/> Manifest extensions for Firestorm

viewer_dir = os.path.dirname(__file__)
# Add indra/lib/python to our path so we don't have to muck with PYTHONPATH.
# Put it FIRST because some of our build hosts have an ancient install of
# indra.util.llmanifest under their system Python!
sys.path.insert(0, os.path.join(viewer_dir, os.pardir, "lib", "python"))
from indra.util.llmanifest import LLManifest, main, proper_windows_path, path_ancestors, CHANNEL_VENDOR_BASE, RELEASE_CHANNEL, ManifestError
try:
    from llbase import llsd
except ImportError:
    from indra.base import llsd

class ViewerManifest(LLManifest,FSViewerManifest):
    def is_packaging_viewer(self):
        # Some commands, files will only be included
        # if we are packaging the viewer on windows.
        # This manifest is also used to copy
        # files during the build (see copy_w_viewer_manifest
        # and copy_l_viewer_manifest targets)
        return 'package' in self.args['actions']

    def do_copy_artwork( self ):
        return self.args.has_key( 'copy_artwork' )

    def construct(self):
        super(ViewerManifest, self).construct()
        self.exclude("*.svn*")
        self.path(src="../../scripts/messages/message_template.msg", dst="app_settings/message_template.msg")
        self.path(src="../../etc/message.xml", dst="app_settings/message.xml")
        
        # <FS:LO> Copy dictionaries to a place where the viewer can find them if ran from visual studio
        if self.prefix(src="app_settings"):
            # ... and the included spell checking dictionaries
            pkgdir = os.path.join(self.args['build'], os.pardir, 'packages')
            if self.prefix(src=pkgdir,dst=""):
                self.path("dictionaries")
                self.end_prefix(pkgdir)
            self.end_prefix("app_settings")
        # </FS:LO>

        if self.is_packaging_viewer() or self.do_copy_artwork():
            if self.prefix(src="app_settings"):
                self.exclude("logcontrol.xml")
                self.exclude("logcontrol-dev.xml")
                self.path("*.pem")
                self.path("*.ini")
                self.path("*.xml")
                self.path("*.db2")

                # include the entire shaders directory recursively
                self.path("shaders")
                # include the extracted list of contributors
                contributions_path = "../../doc/contributions.txt"
                contributor_names = self.extract_names(contributions_path)
                self.put_in_file(contributor_names, "contributors.txt", src=contributions_path)
                # include the extracted list of translators
                translations_path = "../../doc/translations.txt"
                translator_names = self.extract_names(translations_path)
                self.put_in_file(translator_names, "translators.txt", src=translations_path)
                # include the list of Lindens (if any)
                #   see https://wiki.lindenlab.com/wiki/Generated_Linden_Credits
                linden_names_path = os.getenv("LINDEN_CREDITS")
                if not linden_names_path :
                    print "No 'LINDEN_CREDITS' specified in environment, using built-in list"
                else:
                    try:
                        linden_file = open(linden_names_path,'r')
                    except IOError:
                        print "No Linden names found at '%s', using built-in list" % linden_names_path
                    else:
                         # all names should be one line, but the join below also converts to a string
                        linden_names = ', '.join(linden_file.readlines())
                        self.put_in_file(linden_names, "lindens.txt", src=linden_names_path)
                        linden_file.close()
                        print "Linden names extracted from '%s'" % linden_names_path

                # ... and the entire windlight directory
                self.path("windlight")

                # <FS:LO> Copy dictionaries to a place where the viewer can find them if ran from visual studio
                # ... and the included spell checking dictionaries
#                pkgdir = os.path.join(self.args['build'], os.pardir, 'packages')
#                if self.prefix(src=pkgdir,dst=""):
#                    self.path("dictionaries")
#                    self.end_prefix(pkgdir)
                # </FS:LO>
                # include the entire beams directory
                self.path("beams")
                self.path("beamsColors")


                # CHOP-955: If we have "sourceid" or "viewer_channel" in the
                # build process environment, generate it into
                # settings_install.xml.
                settings_template = dict(
                    sourceid=dict(Comment='Identify referring agency to Linden web servers',
                                  Persist=1,
                                  Type='String',
                                  Value=''),
                    CmdLineGridChoice=dict(Comment='Default grid',
                                  Persist=0,
                                  Type='String',
                                  Value=''),
                    CmdLineChannel=dict(Comment='Command line specified channel name',
                                        Persist=0,
                                        Type='String',
                                        Value=''))
                settings_install = {}
                if 'sourceid' in self.args and self.args['sourceid']:
                    settings_install['sourceid'] = settings_template['sourceid'].copy()
                    settings_install['sourceid']['Value'] = self.args['sourceid']
                    print "Set sourceid in settings_install.xml to '%s'" % self.args['sourceid']

                if 'channel_suffix' in self.args and self.args['channel_suffix']:
                    settings_install['CmdLineChannel'] = settings_template['CmdLineChannel'].copy()
                    settings_install['CmdLineChannel']['Value'] = self.channel_with_pkg_suffix()
                    print "Set CmdLineChannel in settings_install.xml to '%s'" % self.channel_with_pkg_suffix()

                if 'grid' in self.args and self.args['grid']:
                    settings_install['CmdLineGridChoice'] = settings_template['CmdLineGridChoice'].copy()
                    settings_install['CmdLineGridChoice']['Value'] = self.grid()
                    print "Set CmdLineGridChoice in settings_install.xml to '%s'" % self.grid()

                # put_in_file(src=) need not be an actual pathname; it
                # only needs to be non-empty
                self.put_in_file(llsd.format_pretty_xml(settings_install),
                                 "settings_install.xml",
                                 src="environment")

                self.end_prefix("app_settings")

            if self.prefix(src="character"):
                self.path("*.llm")
                self.path("*.xml")
                self.path("*.tga")
                self.end_prefix("character")

            # Include our fonts
            if self.prefix(src="fonts"):
                self.path("*.ttf")
                self.path("*.txt")
                self.path("*.xml")
                self.end_prefix("fonts")
                
            # AO: Include casviewer resources
            if self.prefix(src="fs_resources"):
				self.path("*.txt")
				self.path("*.lsl")
				self.path("*.lsltxt")
				self.end_prefix("fs_resources");

            # skins
            if self.prefix(src="skins"):
		    self.path("skins.xml")
                    # include the entire textures directory recursively
                    if self.prefix(src="*/textures"):
                            self.path("*/*.tga")
                            self.path("*/*.j2c")
                            self.path("*/*.jpg")
                            self.path("*/*.png")
                            self.path("*.tga")
                            self.path("*.j2c")
                            self.path("*.jpg")
                            self.path("*.png")
                            self.path("textures.xml")
                            self.end_prefix("*/textures")
                    self.path("*/xui/*/*.xml")
                    self.path("*/xui/*/widgets/*.xml")
		    self.path("*/themes/*/colors.xml")
		    if self.prefix(src="*/themes/*/textures"):
                            self.path("*/*.tga")
                            self.path("*/*.j2c")
                            self.path("*/*.jpg")
                            self.path("*/*.png")
                            self.path("*.tga")
                            self.path("*.j2c")
                            self.path("*.jpg")
                            self.path("*.png")
                            self.end_prefix("*/themes/*/textures")

            # <FS:AO> - We intentionally do not package xui for themes, the reasoning is: 
            #         Themes are defined as color/texture mods, not structual mods. Structural changes are done as "skins".
            #         If a color is mentioned in xui, it can be refactored to use a more generic reference color, and
            #         then overwritten by the theme-specific colors.xml. This saves us from having to maintain more XUI 
            #         in more places than needed, and over time allows more and more of the viewer to be adjusted using
            #         only color definitions.
	 	
            ## FS:Ansariel: Fix packaging for xui folders in themes (FIRE-6859)
            #if self.prefix(src="*/themes/*/xui"):
            #        self.path("*/*.xml")
            #        self.path("*/widgets/*.xml")
            #        self.end_prefix("*/themes/*/xui")
            # </FS:AO>

                    self.path("*/*.xml")

                    # Local HTML files (e.g. loading screen)
                    # The claim is that we never use local html files any
                    # longer. But rather than commenting out this block, let's
                    # rename every html subdirectory as html.old. That way, if
                    # we're wrong, a user actually does have the relevant
                    # files; s/he just needs to rename every html.old
                    # directory back to html to recover them.
                    if self.prefix(src="*/html", dst="*/html.old"):
                            self.path("*.png")
                            self.path("*/*/*.html")
                            self.path("*/*/*.gif")
                            self.end_prefix("*/html")

                    self.end_prefix("skins")

            # local_assets dir (for pre-cached textures)
            if self.prefix(src="local_assets"):
                self.path("*.j2c")
                self.path("*.tga")
                self.end_prefix("local_assets")

            # Files in the newview/ directory
            self.path("gpu_table.txt")
            # The summary.json file gets left in the build directory by newview/CMakeLists.txt.
            if not self.path2basename(os.pardir, "summary.json"):
                print "No summary.json file"

    def grid(self):
        return self.args['grid']

    def channel(self):
        return self.args['channel']

    def channel_with_pkg_suffix(self):
        fullchannel=self.channel()
        if 'channel_suffix' in self.args and self.args['channel_suffix']:
            fullchannel+=' '+self.args['channel_suffix']
        return fullchannel

    def channel_variant(self):
        global CHANNEL_VENDOR_BASE
        return self.channel().replace(CHANNEL_VENDOR_BASE, "").strip()

    def channel_type(self): # returns 'release', 'beta', 'project', or 'test'
        global CHANNEL_VENDOR_BASE
        channel_qualifier=self.channel().replace(CHANNEL_VENDOR_BASE, "").lower().strip()
        if channel_qualifier.startswith('release'):
            channel_type='release'
        elif channel_qualifier.startswith('beta'):
            channel_type='beta'
        elif channel_qualifier.startswith('project'):
            channel_type='project'
        else:
            channel_type='test'
        return channel_type

    def channel_variant_app_suffix(self):
        # get any part of the compiled channel name after the CHANNEL_VENDOR_BASE
        suffix=self.channel_variant()
        # by ancient convention, we don't use Release in the app name
        if self.channel_type() == 'release':
            suffix=suffix.replace('Release', '').strip()
        # for the base release viewer, suffix will now be null - for any other, append what remains
        if len(suffix) > 0:
            suffix = "_"+ ("_".join(suffix.split()))
        # the additional_packages mechanism adds more to the installer name (but not to the app name itself)
        if 'channel_suffix' in self.args and self.args['channel_suffix']:
            suffix+='_'+("_".join(self.args['channel_suffix'].split()))
        return suffix

    def installer_base_name(self):
        global CHANNEL_VENDOR_BASE
        # a standard map of strings for replacing in the templates
        substitution_strings = {
            'channel_vendor_base' : '_'.join(CHANNEL_VENDOR_BASE.split()),
            'channel_variant_underscores':self.channel_variant_app_suffix(),
            'version_underscores' : '_'.join(self.args['version']),
            'arch':self.args['arch']
            }
        return "%(channel_vendor_base)s%(channel_variant_underscores)s_%(version_underscores)s_%(arch)s" % substitution_strings

    def app_name(self):
        global CHANNEL_VENDOR_BASE
        channel_type=self.channel_type()
        if channel_type == 'release':
            app_suffix='Viewer'
        else:
            app_suffix=self.channel_variant()
			
        #<FS:ND> tag "OS" after CHANNEL_VENDOR_BASE and before any suffix
        #if self.fs_flavor() == 'oss':
		#      app_suffix = "OS" + app_suffix
        #</FS:ND>

        #<FS:ND> Don't separate name by whitespace. This break a lot of things in the old FS installer logic.
        #return CHANNEL_VENDOR_BASE + ' ' + app_suffix
        return CHANNEL_VENDOR_BASE + app_suffix
        #</FS:ND>

    def app_name_oneword(self):
        return ''.join(self.app_name().split())
        

    def icon_path(self):
        # <FS:ND> Add -os for oss builds
        if self.fs_flavor() == 'oss':
            return "icons/" + self.channel_type() + "-os"
        # </FS:ND>
        return "icons/" + self.channel_type()

        
    def extract_names(self,src):
        try:
            contrib_file = open(src,'r')
        except IOError:
            print "Failed to open '%s'" % src
            raise
        lines = contrib_file.readlines()
        contrib_file.close()

        # All lines up to and including the first blank line are the file header; skip them
        lines.reverse() # so that pop will pull from first to last line
        while not re.match("\s*$", lines.pop()) :
            pass # do nothing

        # A line that starts with a non-whitespace character is a name; all others describe contributions, so collect the names
        names = []
        for line in lines :
            if re.match("\S", line) :
                names.append(line.rstrip())
        # It's not fair to always put the same people at the head of the list
        random.shuffle(names)
        return ', '.join(names)

class Windows_i686_Manifest(ViewerManifest):
    def final_exe(self):
        return self.app_name_oneword()+".exe"

    def test_msvcrt_and_copy_action(self, src, dst):
        # This is used to test a dll manifest.
        # It is used as a temporary override during the construct method
        from test_win32_manifest import test_assembly_binding
        if src and (os.path.exists(src) or os.path.islink(src)):
            # ensure that destination path exists
            self.cmakedirs(os.path.dirname(dst))
            self.created_paths.append(dst)
            if not os.path.isdir(src):
                if(self.args['configuration'].lower() == 'debug'):
                    test_assembly_binding(src, "Microsoft.VC80.DebugCRT", "8.0.50727.4053")
                else:
                    test_assembly_binding(src, "Microsoft.VC80.CRT", "8.0.50727.4053")
                self.ccopy(src,dst)
            else:
                raise Exception("Directories are not supported by test_CRT_and_copy_action()")
        else:
            print "Doesn't exist:", src

    def test_for_no_msvcrt_manifest_and_copy_action(self, src, dst):
        # This is used to test that no manifest for the msvcrt exists.
        # It is used as a temporary override during the construct method
        from test_win32_manifest import test_assembly_binding
        from test_win32_manifest import NoManifestException, NoMatchingAssemblyException
        if src and (os.path.exists(src) or os.path.islink(src)):
            # ensure that destination path exists
            self.cmakedirs(os.path.dirname(dst))
            self.created_paths.append(dst)
            if not os.path.isdir(src):
                try:
                    if(self.args['configuration'].lower() == 'debug'):
                        test_assembly_binding(src, "Microsoft.VC80.DebugCRT", "")
                    else:
                        test_assembly_binding(src, "Microsoft.VC80.CRT", "")
                    raise Exception("Unknown condition")
                except NoManifestException, err:
                    pass
                except NoMatchingAssemblyException, err:
                    pass
                    
                self.ccopy(src,dst)
            else:
                raise Exception("Directories are not supported by test_CRT_and_copy_action()")
        else:
            print "Doesn't exist:", src
        
    def construct(self):
        super(Windows_i686_Manifest, self).construct()

        if self.is_packaging_viewer():
            # Find secondlife-bin.exe in the 'configuration' dir, then rename it to the result of final_exe.
            self.path(src='%s/casviewer-bin.exe' % self.args['configuration'], dst=self.final_exe())

        # Plugin host application
        self.path2basename(os.path.join(os.pardir,
                                        'llplugin', 'slplugin', self.args['configuration']),
                           "slplugin.exe")
        
        self.path2basename("../viewer_components/updater/scripts/windows", "update_install.bat")

        # Get shared libs from the shared libs staging directory
        if self.prefix(src=os.path.join(os.pardir, 'sharedlibs', self.args['configuration']),
                       dst=""):

            # Get llcommon and deps. If missing assume static linkage and continue.
            try:
                self.path('llcommon.dll')
                self.path('libapr-1.dll')
                self.path('libaprutil-1.dll')
                self.path('libapriconv-1.dll')
                
            except RuntimeError, err:
                print err.message
                print "Skipping llcommon.dll (assuming llcommon was linked statically)"

            # Mesh 3rd party libs needed for auto LOD and collada reading
            try:
                if self.args['configuration'].lower() == 'debug':
                    self.path("libcollada14dom22-d.dll")
                else:
                    self.path("libcollada14dom22.dll")
                    
                self.path("glod.dll")
            except RuntimeError, err:
                print err.message
                print "Skipping COLLADA and GLOD libraries (assumming linked statically)"

            # Get fmodex dll, continue if missing
            try:
                if self.args['configuration'].lower() == 'debug':
                    self.path("fmodexL.dll")
                    self.path("fmodexL64.dll")
                else:
                    self.path("fmodex.dll")
                    self.path("fmodex64.dll")
            except:
                print "Skipping fmodex audio library(assuming other audio engine)"
			
            # Get Leap Motion SDK
            try:
                if self.args['configuration'].lower() == 'debug':
                    self.path("Leapd.dll")
                else:
                    self.path("Leap.dll")
            except:
                print "Leap Motion library was not found"

            # For textures
            if self.args['configuration'].lower() == 'debug':
                self.path("openjpegd.dll")
            else:
                self.path("openjpeg.dll")

            # These need to be installed as a SxS assembly, currently a 'private' assembly.
            # See http://msdn.microsoft.com/en-us/library/ms235291(VS.80).aspx
            if self.args['configuration'].lower() == 'debug':
                 self.path("msvcr100d.dll")
                 self.path("msvcp100d.dll")
            else:
                 self.path("msvcr100.dll")
                 self.path("msvcp100.dll")

            # Vivox runtimes
            self.path("SLVoice.exe")
            self.path("vivoxsdk.dll")
            self.path("ortp.dll")
            self.path("libsndfile-1.dll")
            self.path("zlib1.dll")
            self.path("vivoxplatform.dll")
            self.path("vivoxoal.dll")
            self.path("ca-bundle.crt")
            
            # Security
            self.path("ssleay32.dll")
            self.path("libeay32.dll")

            # Hunspell
            self.path("libhunspell.dll")

            # Growl
            self.path("growl.dll")
            self.path("growl++.dll")

            # <FS:ND> Copy symbols for breakpad
            self.path("ssleay32.pdb")
            self.path("libeay32.pdb")
            self.path("growl.pdb")
            self.path("growl++.pdb")
            self.path('apr-1.pdb', 'libarp.pdb')
            self.path('aprutil-1.pdb', 'libaprutil.pdb')
            # </FS:ND>

            # For google-perftools tcmalloc allocator.
            try:
                if self.args['configuration'].lower() == 'debug':
                    self.path('libtcmalloc_minimal-debug.dll')
                else:
                    self.path('libtcmalloc_minimal.dll')
            except:
                print "Skipping libtcmalloc_minimal.dll"

            self.end_prefix()

        self.path(src="licenses-win32.txt", dst="licenses.txt")
        self.path("featuretable.txt")
        self.path("featuretable_xp.txt")
        self.path("VivoxAUP.txt")

        # Media plugins - QuickTime
        if self.prefix(src='../media_plugins/quicktime/%s' % self.args['configuration'], dst="llplugin"):
            self.path("media_plugin_quicktime.dll")
            self.end_prefix()

        # Media plugins - WebKit/Qt
        if self.prefix(src='../media_plugins/webkit/%s' % self.args['configuration'], dst="llplugin"):
            self.path("media_plugin_webkit.dll")
            self.end_prefix()

        # winmm.dll shim
        if self.prefix(src='../media_plugins/winmmshim/%s' % self.args['configuration'], dst=""):
            self.path("winmm.dll")
            self.end_prefix()


        if self.args['configuration'].lower() == 'debug' and not self.fs_is_64bit_build():
            if self.prefix(src=os.path.join(os.pardir, 'packages', 'lib', 'debug'),
                           dst="llplugin"):
                self.path("libeay32.dll")
                self.path("qtcored4.dll")
                self.path("qtguid4.dll")
                self.path("qtnetworkd4.dll")
                self.path("qtopengld4.dll")
                self.path("qtwebkitd4.dll")
                self.path("qtxmlpatternsd4.dll")
                self.path("ssleay32.dll")

                # For WebKit/Qt plugin runtimes (image format plugins)
                if self.prefix(src="imageformats", dst="imageformats"):
                    self.path("qgifd4.dll")
                    self.path("qicod4.dll")
                    self.path("qjpegd4.dll")
                    self.path("qmngd4.dll")
                    self.path("qsvgd4.dll")
                    self.path("qtiffd4.dll")
                    self.end_prefix()

                # For WebKit/Qt plugin runtimes (codec/character encoding plugins)
                if self.prefix(src="codecs", dst="codecs"):
                    self.path("qcncodecsd4.dll")
                    self.path("qjpcodecsd4.dll")
                    self.path("qkrcodecsd4.dll")
                    self.path("qtwcodecsd4.dll")
                    self.end_prefix()

                self.end_prefix()
        elif not self.fs_is_64bit_build():
            if self.prefix(src=os.path.join(os.pardir, 'packages', 'lib', 'release'),
                           dst="llplugin"):
                self.path("libeay32.dll")
                self.path("qtcore4.dll")
                self.path("qtgui4.dll")
                self.path("qtnetwork4.dll")
                self.path("qtopengl4.dll")
                self.path("qtwebkit4.dll")
                self.path("qtxmlpatterns4.dll")
                self.path("ssleay32.dll")

                # For WebKit/Qt plugin runtimes (image format plugins)
                if self.prefix(src="imageformats", dst="imageformats"):
                    self.path("qgif4.dll")
                    self.path("qico4.dll")
                    self.path("qjpeg4.dll")
                    self.path("qmng4.dll")
                    self.path("qsvg4.dll")
                    self.path("qtiff4.dll")
                    self.end_prefix()

                # For WebKit/Qt plugin runtimes (codec/character encoding plugins)
                if self.prefix(src="codecs", dst="codecs"):
                    self.path("qcncodecs4.dll")
                    self.path("qjpcodecs4.dll")
                    self.path("qkrcodecs4.dll")
                    self.path("qtwcodecs4.dll")
                    self.end_prefix()

                self.end_prefix()
        elif self.fs_is_64bit_build() and self.prefix( src = "../packages/bin_x86/slplugin", dst="" ):
            self.path( "slplugin.exe" )

            if self.prefix( src = "llplugin", dst="llplugin" ):
              self.path( "*.dll" )

              if self.prefix( src = "imageformats", dst="imageformats" ):
                self.path( "*.dll" )
                self.end_prefix()
              if self.prefix( src = "codecs", dst="codecs" ):
                self.path( "*.dll" )
                self.end_prefix()

              self.end_prefix()

            self.end_prefix()


        # pull in the crash logger and updater from other projects
        # tag:"crash-logger" here as a cue to the exporter
        self.path(src='../win_crash_logger/%s/windows-crash-logger.exe' % self.args['configuration'],
                  dst="win_crash_logger.exe")

        if not self.is_packaging_viewer():
            self.package_file = "copied_deps"    

    def nsi_file_commands(self, install=True):
        def wpath(path):
            if path.endswith('/') or path.endswith(os.path.sep):
                path = path[:-1]
            path = path.replace('/', '\\')
            return path

        result = ""
        dest_files = [pair[1] for pair in self.file_list if pair[0] and os.path.isfile(pair[1]) and not pair[1].endswith(".pdb") ] #<FS:ND/> Don't include pdb files.
        # sort deepest hierarchy first
        dest_files.sort(lambda a,b: cmp(a.count(os.path.sep),b.count(os.path.sep)) or cmp(a,b))
        dest_files.reverse()
        out_path = None
        for pkg_file in dest_files:
            rel_file = os.path.normpath(pkg_file.replace(self.get_dst_prefix()+os.path.sep,''))
            installed_dir = wpath(os.path.join('$INSTDIR', os.path.dirname(rel_file)))
            pkg_file = wpath(os.path.normpath(pkg_file))
            if installed_dir != out_path:
                if install:
                    out_path = installed_dir
                    result += 'SetOutPath ' + out_path + '\n'
            if install:
                result += 'File ' + pkg_file + '\n'
            else:
                result += 'Delete ' + wpath(os.path.join('$INSTDIR', rel_file)) + '\n'

        # at the end of a delete, just rmdir all the directories
        if not install:
            deleted_file_dirs = [os.path.dirname(pair[1].replace(self.get_dst_prefix()+os.path.sep,'')) for pair in self.file_list]
            # find all ancestors so that we don't skip any dirs that happened to have no non-dir children
            deleted_dirs = []
            for d in deleted_file_dirs:
                deleted_dirs.extend(path_ancestors(d))
            # sort deepest hierarchy first
            deleted_dirs.sort(lambda a,b: cmp(a.count(os.path.sep),b.count(os.path.sep)) or cmp(a,b))
            deleted_dirs.reverse()
            prev = None
            for d in deleted_dirs:
                if d != prev:   # skip duplicates
                    result += 'RMDir ' + wpath(os.path.join('$INSTDIR', os.path.normpath(d))) + '\n'
                prev = d

        return result

    def package_finish(self):
        # a standard map of strings for replacing in the templates
        substitution_strings = {
            'version' : '.'.join(self.args['version']),
            'version_short' : '.'.join(self.args['version'][:-1]),
            'version_dashes' : '-'.join(self.args['version']),
            'final_exe' : self.final_exe(),
            'flags':'',
            'app_name':self.app_name(),
            'app_name_oneword':self.app_name_oneword()
            }

        substitution_strings = self.fs_splice_grid_substitution_strings( substitution_strings ) #<FS:ND/> Add grid args

        # <FS:ND> Properly name OS version, also add Phoenix- in front of installer name
        #installer_file = self.installer_base_name() + '_Setup.exe'
        installer_file = "%(app_name)s-%(version_dashes)s_Setup.exe" % substitution_strings
        # </FS:ND>
        
        substitution_strings['installer_file'] = installer_file
        
        version_vars = """
        !define INSTEXE  "%(final_exe)s"
        !define VERSION "%(version_short)s"
        !define VERSION_LONG "%(version)s"
        !define VERSION_DASHES "%(version_dashes)s"
        """ % substitution_strings
        
        if self.channel_type() == 'release':
            substitution_strings['caption'] = CHANNEL_VENDOR_BASE
        else:
            substitution_strings['caption'] = self.app_name() + ' ${VERSION}'

        inst_vars_template = """
            OutFile "%(installer_file)s"
            !define INSTNAME   "%(app_name_oneword)s"
            !define SHORTCUT   "%(app_name)s"
            !define URLNAME   "secondlife"
            Caption "%(caption)s"
            """

        tempfile = "secondlife_setup_tmp.nsi"
        
        self.fs_sign_win_binaries() # <FS:ND/> Sign files, step one. Sign compiled binaries

        if not self.fs_is_64bit_build():
          # the following replaces strings in the nsi template
          # it also does python-style % substitution
          self.replace_in("installers/windows/installer_template.nsi", tempfile, {
                  "%%VERSION%%":version_vars,
                  "%%SOURCE%%":self.get_src_prefix(),
                  "%%INST_VARS%%":inst_vars_template % substitution_strings,
                  "%%INSTALL_FILES%%":self.nsi_file_commands(True),
                  "%%DELETE_FILES%%":self.nsi_file_commands(False)})

          # We use the Unicode version of NSIS, available from
          # http://www.scratchpaper.com/
          # Check two paths, one for Program Files, and one for Program Files (x86).
          # Yay 64bit windows.
          NSIS_path = os.path.expandvars('${ProgramFiles}\\NSIS\\Unicode\\makensis.exe')
          if not os.path.exists(NSIS_path):
              NSIS_path = os.path.expandvars('${ProgramFiles(x86)}\\NSIS\\Unicode\\makensis.exe')

          installer_created=False
          nsis_attempts=3
          nsis_retry_wait=15
          while (not installer_created) and (nsis_attempts > 0):
            try:
                nsis_attempts-=1;
                self.run_command('"' + proper_windows_path(NSIS_path) + '" /V2 ' + self.dst_path_of(tempfile))
                installer_created=True # if no exception was raised, the codesign worked
            except ManifestError, err:
                if nsis_attempts:
                    print >> sys.stderr, "nsis failed, waiting %d seconds before retrying" % nsis_retry_wait
                    time.sleep(nsis_retry_wait)
                    nsis_retry_wait*=2
                else:
                    print >> sys.stderr, "Maximum nsis attempts exceeded; giving up"
                    raise
          # self.remove(self.dst_path_of(tempfile))
        else:
          installer_file = "%(app_name)s-%(version_dashes)s_Setup.msi" % substitution_strings
          createMSI = "installers/windows_x64/build.bat"
          createMSI  = self.dst_path_of( "../../../indra/newview/" + createMSI)
          settingsFile = "settings_%s_v4.xml" % self.app_name()

          substitution_strings['installer_file'] = installer_file

          channelSuffix = self.channel().replace( CHANNEL_VENDOR_BASE, "" ).strip()

          self.run_command('"' + createMSI + '" ' + self.dst_path_of( "" ) +
                           " " + self.channel() + " " + substitution_strings[ 'version' ] +
                           " " + settingsFile + " " + installer_file[:-4] + " " + " ".join( substitution_strings[ 'version' ].split(".") ) +
                           " " + self.args['upgradecodes'].split(",")[0] + " " + self.args['upgradecodes'].split(",")[1] + " " + channelSuffix  )
        self.fs_sign_win_installer( substitution_strings ) # <FS:ND/> Sign files, step two. Sign installer.

        self.fs_save_windows_symbols( substitution_strings )


# If we're on a build machine, sign the code using our Authenticode certificate. JC
 
#        sign_py = os.path.expandvars("${SIGN}")
#        if not sign_py or sign_py == "${SIGN}":
#            sign_py = 'C:\\buildscripts\\code-signing\\sign.py'
#        else:
#            sign_py = sign_py.replace('\\', '\\\\\\\\')
#        python = os.path.expandvars("${PYTHON}")
#        if not python or python == "${PYTHON}":
#            python = 'python'
#        if os.path.exists(sign_py):
#            self.run_command("%s %s %s" % (python, sign_py, self.dst_path_of(installer_file).replace('\\', '\\\\\\\\')))
#        else:
#            print "Skipping code signing,", sign_py, "does not exist"


        self.created_path(self.dst_path_of(installer_file))
        self.package_file = installer_file


class DarwinManifest(ViewerManifest):
    def is_packaging_viewer(self):
        # darwin requires full app bundle packaging even for debugging.
        return True

    def construct(self):
        # copy over the build result (this is a no-op if run within the xcode script)
        self.path(self.args['configuration'] + "/CtrlAltStudio Viewer.app", dst="")

        if self.prefix(src="", dst="Contents"):  # everything goes in Contents
            self.path("Info.plist", dst="Info.plist")

            # copy additional libs in <bundle>/Contents/MacOS/
            self.path("../packages/lib/release/libndofdev.dylib", dst="Resources/libndofdev.dylib")
            self.path("../packages/lib/release/libhunspell-1.3.0.dylib", dst="Resources/libhunspell-1.3.0.dylib")

            if self.prefix(dst="MacOS"):
                self.path2basename("../viewer_components/updater/scripts/darwin", "*.py")
                self.end_prefix()

            # Growl Frameworks
            self.path("../packages/Frameworks/Growl", dst="Frameworks/Growl")

            # most everything goes in the Resources directory
            if self.prefix(src="", dst="Resources"):
                super(DarwinManifest, self).construct()

                if self.prefix("cursors_mac"):
                    self.path("*.tif")
                    self.end_prefix("cursors_mac")

                self.path("licenses-mac.txt", dst="licenses.txt")
                self.path("featuretable_mac.txt")
                self.path("VivoxAUP.txt")

                icon_path = self.icon_path()
                if self.prefix(src=icon_path, dst="") :
                    self.path("casviewer_icon.icns")
                    self.end_prefix(icon_path)

                self.path("Firestorm.nib")
                
                # Translations
                self.path("English.lproj/language.txt")
                self.replace_in(src="English.lproj/InfoPlist.strings",
                                dst="English.lproj/InfoPlist.strings",
                                searchdict={'%%VERSION%%':'.'.join(self.args['version'])}
                                )
                self.path("German.lproj")
                self.path("Japanese.lproj")
                self.path("Korean.lproj")
                self.path("da.lproj")
                self.path("es.lproj")
                self.path("fr.lproj")
                self.path("hu.lproj")
                self.path("it.lproj")
                self.path("nl.lproj")
                self.path("pl.lproj")
                self.path("pt.lproj")
                self.path("ru.lproj")
                self.path("tr.lproj")
                self.path("uk.lproj")
                self.path("zh-Hans.lproj")

                def path_optional(src, dst):
                    """
                    For a number of our self.path() calls, not only do we want
                    to deal with the absence of src, we also want to remember
                    which were present. Return either an empty list (absent)
                    or a list containing dst (present). Concatenate these
                    return values to get a list of all libs that are present.
                    """
                    if self.path(src, dst):
                        return [dst]
                    print "Skipping %s" % dst
                    return []

                libdir = "../packages/lib/release"
                # dylibs is a list of all the .dylib files we expect to need
                # in our bundled sub-apps. For each of these we'll create a
                # symlink from sub-app/Contents/Resources to the real .dylib.
                # Need to get the llcommon dll from any of the build directories as well.
                libfile = "libllcommon.dylib"
                dylibs = path_optional(self.find_existing_file(os.path.join(os.pardir,
                                                               "llcommon",
                                                               self.args['configuration'],
                                                               libfile),
                                                               os.path.join(libdir, libfile)),
                                       dst=libfile)

                for libfile in (
                                "libcollada14dom.dylib",
                                "libapr-1.0.dylib",
                                "libaprutil-1.0.dylib",
                                "libexpat.1.5.2.dylib",
                                "libexception_handler.dylib",
                                "libfmodexL.dylib",
                                "libGLOD.dylib",
                                "libgrowl.dylib",
                                "libgrowl++.dylib",
                                "libLeap.dylib",
                                ):
                    dylibs += path_optional(os.path.join(libdir, libfile), libfile)

                # SLVoice and vivox lols, no symlinks needed
                for libfile in (
                                'libalut.dylib',
                                'libopenal.dylib',
                                'libortp.dylib',
                                'libsndfile.dylib',
                                'libvivoxoal.dylib',
                                'libvivoxsdk.dylib',
                                'libvivoxplatform.dylib',
                                'ca-bundle.crt',
                                'SLVoice',
                                ):
                     self.path2basename(libdir, libfile)

                # dylibs that vary based on configuration
                if self.args['configuration'].lower() == 'debug':
                    for libfile in (
                                "libfmodexL.dylib",
                                ):
                        dylibs += path_optional(os.path.join("../packages/lib/debug",
                                                             libfile), libfile)
                else:
                    for libfile in (
                                "libfmodex.dylib",
                                ):
                        dylibs += path_optional(os.path.join("../packages/lib/release",
                                                             libfile), libfile)
                
                # our apps
                for app_bld_dir, app in (("mac_crash_logger", "mac-crash-logger.app"),
                                         # plugin launcher
                                         (os.path.join("llplugin", "slplugin"), "SLPlugin.app"),
                                         ):
                    self.path2basename(os.path.join(os.pardir,
                                                    app_bld_dir, self.args['configuration']),
                                       app)

                    # our apps dependencies on shared libs
                    # for each app, for each dylib we collected in dylibs,
                    # create a symlink to the real copy of the dylib.
                    resource_path = self.dst_path_of(os.path.join(app, "Contents", "Resources"))
                    for libfile in dylibs:
                        symlinkf(os.path.join(os.pardir, os.pardir, os.pardir, libfile),
                                 os.path.join(resource_path, libfile))

                # SLPlugin.app/Contents/Resources gets those Qt4 libraries it needs.
                if self.prefix(src="", dst="SLPlugin.app/Contents/Resources"):
                    for libfile in ('libQtCore.4.dylib',
                                    'libQtCore.4.7.1.dylib',
                                    'libQtGui.4.dylib',
                                    'libQtGui.4.7.1.dylib',
                                    'libQtNetwork.4.dylib',
                                    'libQtNetwork.4.7.1.dylib',
                                    'libQtOpenGL.4.dylib',
                                    'libQtOpenGL.4.7.1.dylib',
                                    'libQtSvg.4.dylib',
                                    'libQtSvg.4.7.1.dylib',
                                    'libQtWebKit.4.dylib',
                                    'libQtWebKit.4.7.1.dylib',
                                    'libQtXml.4.dylib',
                                    'libQtXml.4.7.1.dylib'):
                        self.path2basename("../packages/lib/release", libfile)
                    self.end_prefix("SLPlugin.app/Contents/Resources")

                # Qt4 codecs go to llplugin.  Not certain why but this is the first
                # location probed according to dtruss so we'll go with that.
                if self.prefix(src="../packages/plugins/codecs/", dst="llplugin/codecs"):
                    self.path("libq*.dylib")
                    self.end_prefix("llplugin/codecs")

                # Similarly for imageformats.
                if self.prefix(src="../packages/plugins/imageformats/", dst="llplugin/imageformats"):
                    self.path("libq*.dylib")
                    self.end_prefix("llplugin/imageformats")

                # SLPlugin plugins proper
                if self.prefix(src="", dst="llplugin"):
                    self.path2basename("../media_plugins/quicktime/" + self.args['configuration'],
                                       "media_plugin_quicktime.dylib")
                    self.path2basename("../media_plugins/webkit/" + self.args['configuration'],
                                       "media_plugin_webkit.dylib")

                    self.end_prefix("llplugin")

                self.end_prefix("Resources")

            self.end_prefix("Contents")

        # NOTE: the -S argument to strip causes it to keep enough info for
        # annotated backtraces (i.e. function names in the crash log).  'strip' with no
        # arguments yields a slightly smaller binary but makes crash logs mostly useless.
        # This may be desirable for the final release.  Or not.
        if ("package" in self.args['actions'] or 
            "unpacked" in self.args['actions']):
            self.run_command('strip -S %(viewer_binary)r' %
                             { 'viewer_binary' : self.dst_path_of('Contents/MacOS/CtrlAltStudio Viewer')})


    def copy_finish(self):
        # Force executable permissions to be set for scripts
        # see CHOP-223 and http://mercurial.selenic.com/bts/issue1802
        for script in 'Contents/MacOS/update_install.py',:
            self.run_command("chmod +x %r" % os.path.join(self.get_dst_prefix(), script))

    def package_finish(self):
        global CHANNEL_VENDOR_BASE
        substitution_strings = self.fs_get_substitution_strings()         # <FS:AO> Copied from windows manifest, since we're starting to use many of the same vars

#Comment out for now. FS:TM
#Added from LL signing for OSX 10.8 
#        # Sign the app if requested.
#        if 'signature' in self.args:
#            identity = self.args['signature']
#            if identity == '':
#                identity = 'Developer ID Application'
#
#            # Look for an environment variable set via build.sh when running in Team City.
#            try:
#                build_secrets_checkout = os.environ['build_secrets_checkout']
#            except KeyError:
#                pass
#            else:
#                # variable found so use it to unlock keyvchain followed by codesign
#                home_path = os.environ['HOME']
#                keychain_pwd_path = os.path.join(build_secrets_checkout,'code-signing-osx','password.txt')
#                keychain_pwd = open(keychain_pwd_path).read().rstrip()
#
#                self.run_command('security unlock-keychain -p "%s" "%s/Library/Keychains/viewer.keychain"' % ( keychain_pwd, home_path ) )
#                signed=False
#                sign_attempts=3
#                sign_retry_wait=15
#                while (not signed) and (sign_attempts > 0):
#                    try:
#                        sign_attempts-=1;
#                        self.run_command(
#                           'codesign --verbose --force --keychain "%(home_path)s/Library/Keychains/viewer.keychain" --sign %(identity)r %(bundle)r' % {
#                               'home_path' : home_path,
#                               'identity': identity,
#                               'bundle': self.get_dst_prefix()
#                               })
#                        signed=True # if no exception was raised, the codesign worked
#                    except ManifestError, err:
#                        if sign_attempts:
#                            print >> sys.stderr, "codesign failed, waiting %d seconds before retrying" % sign_retry_wait
#                            time.sleep(sign_retry_wait)
#                            sign_retry_wait*=2
#                        else:
#                            print >> sys.stderr, "Maximum codesign attempts exceeded; giving up"
#                            raise

        imagename = (self.app_name() + '-' + '-'.join(self.args['version']))

        # MBW -- If the mounted volume name changes, it breaks the .DS_Store's background image and icon positioning.
        #  If we really need differently named volumes, we'll need to create multiple DS_Store file images, or use some other trick.

        #volname=CHANNEL_VENDOR_BASE+" Installer"  # DO NOT CHANGE without understanding comment above
        #[FS:CR] Understood and disregarded!
        volname = (self.app_name() + " " + '.'.join(self.args['version']) + " Installer")

        #imagename = self.installer_base_name()

        sparsename = imagename + ".sparseimage"
        finalname = imagename + ".dmg"
        # make sure we don't have stale files laying about
        self.remove(sparsename, finalname)

        self.run_command('hdiutil create %(sparse)r -volname %(vol)r -fs HFS+ -type SPARSE -megabytes 1000 -layout SPUD' % {
                'sparse':sparsename,
                'vol':volname})

        # mount the image and get the name of the mount point and device node
        hdi_output = self.run_command('hdiutil attach -private %r' % sparsename)
        try:
            devfile = re.search("/dev/disk([0-9]+)[^s]", hdi_output).group(0).strip()
            volpath = re.search('HFS\s+(.+)', hdi_output).group(1).strip()

            if devfile != '/dev/disk1':
                # adding more debugging info based upon nat's hunches to the
                # logs to help track down 'SetFile -a V' failures -brad
                print "WARNING: 'SetFile -a V' command below is probably gonna fail"

            # Copy everything in to the mounted .dmg

            app_name = self.app_name()

            # Hack:
            # Because there is no easy way to coerce the Finder into positioning
            # the app bundle in the same place with different app names, we are
            # adding multiple .DS_Store files to svn. There is one for release,
            # one for release candidate and one for first look. Any other channels
            # will use the release .DS_Store, and will look broken.
            # - Ambroff 2008-08-20
            # If the channel is "ctrlaltstudio-viewer-private-"anything, then use the
            #  private folder for .DS_Store and the background image. -- TS
            template_chan = app_name.lower()
            if template_chan.startswith("ctrlaltstudio-viewer-private"):
                template_chan = "ctrlaltstudio-viewer-private"
            elif template_chan.startswith("firestormos-private"):
                template_chan = "firestormos-private"
            dmg_template = os.path.join(
                'installers', 'darwin', '%s-dmg' % template_chan)

            if not os.path.exists (self.src_path_of(dmg_template)):
                dmg_template = os.path.join ('installers', 'darwin', 'casviewer-release-dmg')

            for s,d in {self.get_dst_prefix():app_name + ".app",
                        os.path.join(dmg_template, "_VolumeIcon.icns"): ".VolumeIcon.icns",
                        os.path.join(dmg_template, "background.png"): "background.png",
                        os.path.join(dmg_template, "VivoxAUP.txt"): "Vivox Acceptable Use Policy.txt",
                        os.path.join(dmg_template, "LGPL-license.txt"): "LGPL License.txt"}.items():
                        #os.path.join(dmg_template, "_DS_Store"): ".DS_Store"}.items():
                print "Copying to dmg", s, d
                self.copy_action(self.src_path_of(s), os.path.join(volpath, d))

            # <FS:TS> The next two commands *MUST* execute before the loop
            #         that hides the files. If not, packaging will fail.
            #         YOU HAVE BEEN WARNED. 
            # Create the alias file (which is a resource file) from the .r
            self.run_command('Rez %r -o %r' %
                             (self.src_path_of("installers/darwin/casviewer-release-dmg/Applications-alias.r"),
                              os.path.join(volpath, "Applications")))

            # Set up the installer disk image: set icon positions, folder view
            #  options, and icon label colors. This must be done before the
            #  files are hidden.
            self.run_command('osascript %r %r' % 
                             (self.src_path_of("installers/darwin/installer-dmg.applescript"),
                             volname))

            # Hide the background image, DS_Store file, and volume icon file (set their "visible" bit)
            for f in ".VolumeIcon.icns", "background.png", ".DS_Store":
                pathname = os.path.join(volpath, f)
                # We've observed mysterious "no such file" failures of the SetFile
                # command, especially on the first file listed above -- yet
                # subsequent inspection of the target directory confirms it's
                # there. Timing problem with copy command? Try to handle.
                for x in xrange(3):
                    if os.path.exists(pathname):
                        print "Confirmed existence: %r" % pathname
                        break
                    print "Waiting for %s copy command to complete (%s)..." % (f, x+1)
                    sys.stdout.flush()
                    time.sleep(1)
                # If we fall out of the loop above without a successful break, oh
                # well, possibly we've mistaken the nature of the problem. In any
                # case, don't hang up the whole build looping indefinitely, let
                # the original problem manifest by executing the desired command.
                self.run_command('SetFile -a V %r' % pathname)

            # Set the alias file's alias and custom icon bits
            self.run_command('SetFile -a AC %r' % os.path.join(volpath, "Applications"))

            # Set the disk image root's custom icon bit
            self.run_command('SetFile -a C %r' % volpath)
        finally:
            # Unmount the image even if exceptions from any of the above 
            self.run_command('hdiutil detach -force %r' % devfile)

        print "Converting temp disk image to final disk image"
        self.run_command('hdiutil convert %(sparse)r -format UDZO -imagekey zlib-level=9 -o %(final)r' % {'sparse':sparsename, 'final':finalname})
        self.run_command('hdiutil internet-enable -yes %(final)r' % {'final':finalname})
        # get rid of the temp file
        self.package_file = finalname
        self.remove(sparsename)

        #AO: Try to package up symbols
        # New Method, for reading cross platform stack traces on a linux/mac host
        if (os.path.exists("%s/casviewer-symbols-darwin.tar.bz2" % self.args['configuration'].lower())):
            # Rename to add version numbers
            os.rename("%s/casviewer-symbols-darwin.tar.bz2" % self.args['configuration'].lower(),
                      "%s/%s_%s_%s_symbols-darwin.tar.bz2" % (self.args['configuration'].lower(),
                                                                      self.fs_channel_legacy_oneword(),
                                                                      substitution_strings['version_dashes'],
                                                                      self.args['viewer_flavor']))


class Darwin_i386_Manifest(DarwinManifest):
    def construct(self):
        super(Darwin_i386_Manifest, self).construct()


class Darwin_universal_Manifest(DarwinManifest):
    def construct(self):
        super(Darwin_universal_Manifest, self).construct()

        if(self.prefix(src="../packages/bin_x86", dst="Contents/Resources/")):
            self.path("slplugin.app")
			
            if self.prefix(src = "llplugin", dst="llplugin"):
                self.path("media_plugin_quicktime.dylib")
                self.path("media_plugin_webkit.dylib")
                self.path("libllqtwebkit.dylib")
            self.end_prefix("llplugin")

        self.end_prefix("../packages/bin_x86");


class Darwin_x86_64_Manifest(DarwinManifest):
    def construct(self):
        super(Darwin_x86_64_Manifest, self).construct()

        if(self.prefix("../packages/bin_x86", dst="Contents/Resources/")):
            self.path("slplugin.app", "slplugin.app")
	
            if self.prefix(src = "llplugin", dst="llplugin"):
                self.path("media_plugin_quicktime.dylib", "media_plugin_quicktime.dylib")
                self.path("media_plugin_webkit.dylib", "media_plugin_webkit.dylib")
                self.path("libllqtwebkit.dylib", "libllqtwebkit.dylib")
            self.end_prefix("llplugin")

        self.end_prefix("../packages/bin_x86");


class LinuxManifest(ViewerManifest):
    def construct(self):
        super(LinuxManifest, self).construct()
        self.path("licenses-linux.txt","licenses.txt")
        self.path("VivoxAUP.txt")
        self.path("res/casviewer_icon.png","casviewer_icon.png")
        if self.prefix("linux_tools", dst=""):
            self.path("client-readme.txt","README-linux.txt")
	    self.path("CASVIEWER_DESKTOPINSTALL.txt","CASVIEWER_DESKTOPINSTALL.txt")
            self.path("client-readme-voice.txt","README-linux-voice.txt")
            self.path("client-readme-joystick.txt","README-linux-joystick.txt")
            self.path("wrapper.sh","casviewer")
            if self.prefix(src="", dst="etc"):
                self.path("handle_secondlifeprotocol.sh")
                self.path("register_secondlifeprotocol.sh")
                self.path("refresh_desktop_app_entry.sh")
                self.path("launch_url.sh")
                self.end_prefix("etc")
            self.path("install.sh")
            self.end_prefix("linux_tools")

        if self.prefix(src="", dst="bin"):
            self.path("casviewer-bin","do-not-directly-run-casviewer-bin")
            self.path("../linux_crash_logger/linux-crash-logger","linux-crash-logger.bin")
            self.path2basename("../llplugin/slplugin", "SLPlugin")
            self.path2basename("../viewer_components/updater/scripts/linux", "update_install")
            self.end_prefix("bin")

        if self.prefix("res-sdl"):
            self.path("*")
            # recurse
            self.end_prefix("res-sdl")

        # Get the icons based on the channel type
        icon_path = self.icon_path()
        print "DEBUG: icon_path '%s'" % icon_path
        if self.prefix(src=icon_path, dst="") :
            self.path("casviewer_256.png","casviewer_48.png")
            if self.prefix(src="",dst="res-sdl") :
                self.path("casviewer_256.BMP","ll_icon.BMP")
                self.end_prefix("res-sdl")
            self.end_prefix(icon_path)

        # plugins
        if self.prefix(src="", dst="bin/llplugin"):
            self.path2basename("../media_plugins/webkit", "libmedia_plugin_webkit.so")
            self.path("../media_plugins/gstreamer010/libmedia_plugin_gstreamer010.so", "libmedia_plugin_gstreamer.so")
            self.end_prefix("bin/llplugin")

        if not self.path("../llcommon/libllcommon.so", "lib/libllcommon.so"):
            print "Skipping llcommon.so (assuming llcommon was linked statically)"

        self.path("featuretable_linux.txt")
        
        if self.is_packaging_viewer():
          if self.prefix("../packages/lib/release", dst="lib"):
            self.path("libapr-1.so*")
            self.path("libaprutil-1.so*")
            self.path("libboost_context-mt.so*")
            self.path("libboost_filesystem-mt.so*")
            self.path("libboost_program_options-mt.so*")
            self.path("libboost_regex-mt.so*")
            self.path("libboost_signals-mt.so*")
            self.path("libboost_system-mt.so*")
            self.path("libboost_thread-mt.so*")
            self.path("libboost_chrono-mt.so*") #<FS:TM> FS spcific
            self.path("libboost_date_time-mt.so*") #<FS:TM> FS spcific
            self.path("libboost_wave-mt.so*") #<FS:TM> FS spcific
            self.path("libcollada14dom.so*")
            self.path("libdb*.so*")
            self.path("libcrypto.so*")
            self.path("libexpat.so*")
            self.path("libssl.so*")
            self.path("libGLOD.so")
            self.path("libminizip.so")
            self.path("libuuid.so*")
            self.path("libSDL-1.2.so*")
            self.path("libdirectfb*.so*")
            self.path("libfusion*.so*")
            self.path("libdirect*.so*")
            self.path("libopenjpeg.so*")
            self.path("libhunspell-1.3.so*")
            self.path("libalut.so*")
            self.path("libpng15.so.15") #use provided libpng to workaround incompatible system versions on some distros
            self.path("libpng15.so.15.13.0") #use provided libpng to workaround incompatible system versions on some distros
            self.path("libpng15.so.15.1.0") #use provided libpng to workaround incompatible system versions on some distros
            self.path("libopenal.so", "libopenal.so.1") # Install as versioned file in case it's missing from the 3p- and won't get copied below
            self.path("libopenal.so*")
            #self.path("libnotify.so.1.1.2", "libnotify.so.1") # LO - uncomment when testing libnotify(growl) on linux
            self.path("libpangox-1.0.so*")

            # KLUDGE: As of 2012-04-11, the 'fontconfig' package installs
            # libfontconfig.so.1.4.4, along with symlinks libfontconfig.so.1
            # and libfontconfig.so. Before we added support for library-file
            # wildcards, though, this self.path() call specifically named
            # libfontconfig.so.1.4.4 WITHOUT also copying the symlinks. When I
            # (nat) changed the call to self.path("libfontconfig.so.*"), we
            # ended up with the libfontconfig.so.1 symlink in the target
            # directory as well. But guess what! At least on Ubuntu 10.04,
            # certain viewer fonts look terrible with libfontconfig.so.1
            # present in the target directory. Removing that symlink suffices
            # to improve them. I suspect that means we actually do better when
            # the viewer fails to find our packaged libfontconfig.so*, falling
            # back on the system one instead -- but diagnosing and fixing that
            # is a bit out of scope for the present project. Meanwhile, this
            # particular wildcard specification gets us exactly what the
            # previous call did, without having to explicitly state the
            # version number.
            self.path("libfontconfig.so.*.*")

            try:
                self.path("libtcmalloc.so*") #formerly called google perf tools
                pass
            except:
                print "tcmalloc files not found, skipping"
                pass

            self.end_prefix("lib")

            # Vivox runtimes
            # Currentelly, the 32-bit ones will work with a 64-bit client.
            if self.prefix(src="../packages/lib/release", dst="bin"):
                    self.path("SLVoice")
                    self.end_prefix()
            if self.prefix(src="../packages/lib/release", dst="lib"):
                    self.path("libortp.so")
                    self.path("libsndfile.so.1")
                    #self.path("libvivoxoal.so.1") # no - we'll re-use the viewer's own OpenAL lib
                    self.path("libvivoxsdk.so")
                    self.path("libvivoxplatform.so")
                    self.end_prefix("lib")

    def copy_finish(self):
        # Force executable permissions to be set for scripts
        # see CHOP-223 and http://mercurial.selenic.com/bts/issue1802
        for script in 'firestorm', 'bin/update_install':
            self.run_command("chmod +x %r" % os.path.join(self.get_dst_prefix(), script))

    def package_finish(self):
        # a standard map of strings for replacing in the templates
        installer_name_components = ['Phoenix',self.app_name(),self.args.get('arch'),'.'.join(self.args['version'])]
        installer_name = "_".join(installer_name_components)
        #installer_name = self.installer_base_name()

        self.fs_delete_linux_symbols() # <FS:ND/> Delete old syms
        self.strip_binaries()
        self.fs_save_linux_symbols() # <FS:ND/> Package symbols, add debug link

        # Fix access permissions
        self.run_command("""
                find %(dst)s -type d | xargs --no-run-if-empty chmod 755;
                find %(dst)s -type f -perm 0700 | xargs --no-run-if-empty chmod 0755;
                find %(dst)s -type f -perm 0500 | xargs --no-run-if-empty chmod 0555;
                find %(dst)s -type f -perm 0600 | xargs --no-run-if-empty chmod 0644;
                find %(dst)s -type f -perm 0400 | xargs --no-run-if-empty chmod 0444;
                true""" %  {'dst':self.get_dst_prefix() })
        self.package_file = installer_name + '.tar.bz2'

        # temporarily move directory tree so that it has the right
        # name in the tarfile
        self.run_command("mv %(dst)s %(inst)s" % {
            'dst': self.get_dst_prefix(),
            'inst': self.build_path_of(installer_name)})
        try:
            # only create tarball if it's a release build.
            if self.args['buildtype'].lower() == 'release':
                # --numeric-owner hides the username of the builder for
                # security etc.
                self.run_command('tar -C %(dir)s --numeric-owner %(fs_excludes)s -cjf '
                                 '%(inst_path)s.tar.bz2 %(inst_name)s' % {
                        'dir': self.get_build_prefix(),
                        'inst_name': installer_name,
                        'inst_path':self.build_path_of(installer_name),
                        'fs_excludes':self.fs_linux_tar_excludes()
                        })
            else:
                print "Skipping %s.tar.bz2 for non-Release build (%s)" % \
                      (installer_name, self.args['buildtype'])
        finally:
            self.run_command("mv %(inst)s %(dst)s" % {
                'dst': self.get_dst_prefix(),
                'inst': self.build_path_of(installer_name)})

    def strip_binaries(self):
        if self.args['buildtype'].lower() == 'release' and self.is_packaging_viewer():
            print "* Going strip-crazy on the packaged binaries, since this is a RELEASE build"
            self.run_command(r"find %(d)r/bin %(d)r/lib -type f \! -name update_install | xargs --no-run-if-empty strip -S" % {'d': self.get_dst_prefix()} ) # makes some small assumptions about our packaged dir structure


class Linux_i686_Manifest(LinuxManifest):
    def construct(self):
        super(Linux_i686_Manifest, self).construct()

        if self.is_packaging_viewer():
          if self.prefix("../packages/lib/release", dst="lib"):

            self.path("libopenal.so", "libvivoxoal.so.1") # vivox's sdk expects this soname

            try:
                    self.path("libfmodex-*.so")
                    self.path("libfmodex.so")
                    pass
            except:
                    print "Skipping libfmodex.so - not found"
                    pass

            self.end_prefix("lib")

          self.prefix(src="../packages/lib/release/x86", dst="lib")
          try:
              self.path("libLeap.so")
          except:
              print "Leap Motion library not found"
          self.end_prefix("lib")
          open( os.path.join(self.get_dst_prefix(), "FS_No_LD_Hacks.txt" ), "w" ).write( "1" )


class Linux_x86_64_Manifest(LinuxManifest):
    def construct(self):
        super(Linux_x86_64_Manifest, self).construct()

        if self.is_packaging_viewer():
          if self.prefix("../packages/lib/release", dst="lib"):

            # vivox 32-bit hack.
            # one has to extract libopenal.so from the 32-bit openal package, or official LL viewer, and rename it to libopenal32.so
            # and place it in the prebuilt lib/release directory
            self.path("libopenal32.so", "libvivoxoal.so.1") # vivox's sdk expects this soname

            try:
                    self.path("libfmodex64-*.so")
                    self.path("libfmodex64.so")
                    pass
            except:
                    print "Skipping libfmodex.so - not found"
                    pass

            self.end_prefix("lib")

          self.prefix(src="../packages/lib/release/x64", dst="lib")
          try:
              self.path("libLeap.so")
          except:
              print "Leap Motion library not found"
          self.end_prefix("lib")

        # support file for valgrind debug tool
        self.path("secondlife-i686.supp")

################################################################

def symlinkf(src, dst):
    """
    Like ln -sf, but uses os.symlink() instead of running ln.
    """
    try:
        os.symlink(src, dst)
    except OSError, err:
        if err.errno != errno.EEXIST:
            raise
        # We could just blithely attempt to remove and recreate the target
        # file, but that strategy doesn't work so well if we don't have
        # permissions to remove it. Check to see if it's already the
        # symlink we want, which is the usual reason for EEXIST.
        if not (os.path.islink(dst) and os.readlink(dst) == src):
            # Here either dst isn't a symlink or it's the wrong symlink.
            # Remove and recreate. Caller will just have to deal with any
            # exceptions at this stage.
            os.remove(dst)
            os.symlink(src, dst)

if __name__ == "__main__":
    main()
