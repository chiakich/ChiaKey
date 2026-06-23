#!/usr/bin/env python
# encoding: utf-8

import sys
import os
import shutil
import subprocess

version = "1.1.2528"
current_folder = os.path.abspath(os.path.dirname(__file__))
pkg_name = "YahooKeyKey-OSX-ExtraModules-" + version
disk = "Yahoo! KeyKey Extra Modules.localized"
pkg = pkg_name + ".pkg"
dmg = pkg_name + ".dmg"
artwork_folder = "Artwork"
dmg_icns = os.path.join(artwork_folder, "YahooDMG.icns")

applescript  = "tell application \"Finder\"\n\
		tell disk \"" + disk + "\"\n" + """
			open
			set theXOrigin to 40
			set theYOrigin to 60
			set theWidth to 450
			set theHeight to 368
			set theBottomRightX to (theXOrigin + theWidth)
			set theBottomRightY to (theYOrigin + theHeight)
            tell container window
                    set current view to icon view
                    set toolbar visible to false
                    set statusbar visible to false
                    set the bounds to {theXOrigin, theYOrigin, theBottomRightX, theBottomRightY}
                    set statusbar visible to false
            end tell

            set opts to the icon view options of container window
            tell opts
                    set icon size to 112
                    set arrangement to not arranged
            end tell
			update without registering applications
			delay 2
		end tell
	end tell
"""

package_maker = os.path.abspath("../../../ExternalLibraries/PackageMaker.app/Contents/MacOS/PackageMaker")
xcode_command = "xcodebuild build -sdk macosx10.5 -project ../../../Takao.xcodeproj -configuration Release64 -target ";
xcode_clean = "xcodebuild clean -sdk macosx10.5 -project ../../../Takao.xcodeproj -configuration Release64 -target ";
ModuleSign = "../../../build/Release64/ModuleSign"
key = "../Keyring/ModulePrivateKey.pem"

modules = ["YKReverseLookup", "YKAFPhraseConverter", "YKAFWordCount"]

def build_module_sign_tool():
	command = xcode_clean + "ModuleSign"
	os.system(command)
	command = xcode_command + "ModuleSign"
	os.system(command)

def build_module(module_name):
	command = xcode_clean + module_name
	os.system(command)
	command = xcode_command + module_name
	os.system(command)
	
	
def build_module_package(module_name):
	
	source_path = "../../../build/Release64/"
	bundle = module_name + ".bundle"
	source = os.path.abspath(os.path.join(current_folder, source_path, bundle))
	sign_command = ModuleSign + " signmp " + source + " " + key
	os.system(sign_command)
	print "Sign the bundle"
	print sign_command
	
	target = os.path.abspath(os.path.join(current_folder, "tmp"))
	try:
		os.system("sudo rm -rf " + target)
	except Exception:
		# print str(e)
		pass
	try:
		os.mkdir(target)
	except Exception:
		# print str(e)
		pass

	shutil.copytree(source, os.path.join(target, bundle))
	os.system("sudo chown -R root " + os.path.join(target, bundle))
	
	pmproj = module_name + ".pmproj"
	pkg = module_name + ".pkg"
	arg = " -build -ds -v -proj " + pmproj + " -p " + pkg
	os.system(package_maker + arg)

def build_package():
	arg = " -build -ds -v -proj ./Modules.pmproj -p " + pkg
	print package_maker + arg
	os.system(package_maker + arg)

	pkg_path = os.path.join(os.path.abspath(pkg), "Contents/Packages");
	try:
		os.mkdir(pkg_path)
	except:
		pass
		
	for module_name in modules:
		sub_pkg = module_name + ".pkg"
		shutil.copytree(sub_pkg, os.path.join(pkg_path, sub_pkg))
		os.system("sudo rm -rf " + sub_pkg)	
	pass

def make_dmg():
	size = 2 * 1024 * 2;
	command = "/usr/bin/hdiutil"
	arg = [command, "create",  "-sectors", str(size), "-volname", disk, "-attach", "-fs", "HFS+", "Modules.dmg"]
	
	result = subprocess.Popen(arg, stdout=subprocess.PIPE).communicate()
	lines = result[0].split("\n")
	device = ""
	for line in lines:
		if line.find("Apple_HFS") > -1:
			space = line.find(" ")
			device = line[:12]
	shutil.copytree(pkg, os.path.join("/Volumes", disk , pkg))
	localized = os.path.join("/Volumes", disk, ".localized")
	os.mkdir(localized)
	shutil.copy(os.path.join(current_folder , "strings", "en.strings"), os.path.join(localized, "en.strings"))
	shutil.copy(os.path.join(current_folder , "strings", "zh_CN.strings"), os.path.join(localized, "zh_CN.strings"))
	shutil.copy(os.path.join(current_folder , "strings", "zh_TW.strings"), os.path.join(localized, "zh_TW.strings"))
	shutil.copy(os.path.join(current_folder , dmg_icns), os.path.join("/Volumes", disk, ".VolumeIcon.icns"))
	command = "SetFile -a C \"" + os.path.join("/Volumes", disk) + "/.\""
	os.system(command)

	subprocess.Popen(["osascript", "-e", applescript])

	os.system("sleep 1")
	
	os.system("hdiutil detach " + device )
	command = "hdiutil convert Modules.dmg -format UDZO -imagekey zlib-level=9 -o " + dmg
	os.system(command)

def main():
	
	build_module_sign_tool()

	os.system("sudo rm -rf *.dmg")
	os.system("sudo rm -rf *.pkg")
	os.system("sudo rm -rf tmp")

	print "Building Package..."

	for module_name in modules:
		build_module(module_name)
		build_module_package(module_name)

	build_package()
	print "Done.."

	print "Building DMG..."
	make_dmg()
	os.system("sudo rm -rf " + pkg)
	os.system("sudo rm -rf Modules.dmg")
	os.system("sudo rm -rf tmp")
	print "Done.."


if __name__ == '__main__':
	main()

