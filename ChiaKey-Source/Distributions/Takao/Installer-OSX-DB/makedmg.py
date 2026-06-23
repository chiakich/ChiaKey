#!/usr/bin/env python
# encoding: utf-8

import sys
import os
import shutil
import subprocess

version = "1.1.2528"
current_folder = os.path.abspath(os.path.dirname(__file__))
disk = "YahooKeyKey-OSX-DB-" + version
pkg = disk + ".pkg"
dmg = disk + ".dmg"

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

def build_package():
	
	source_filename = "Supplement.db"
	source_path = "../CookedDatabase"
	source = os.path.abspath(os.path.join(current_folder, source_path, source_filename))
	target = os.path.abspath(os.path.join(current_folder, "tmp"))
	try:
		os.system("sudo rm -rf " + target)
	except Exception: # as e:
		# print str(e)
		pass
	try:
		os.mkdir(target)
	except Exception: # as e:
		# print str(e)
		pass

	shutil.copy(source, os.path.join(target, source_filename))
	os.system("sudo chown -R root " + os.path.join(target, source_filename))
	
	package_maker = " ../../../ExternalLibraries/PackageMaker.app/Contents/MacOS/PackageMaker"
	arg = " -build -ds -v -proj ./KeyKeyDB.pmproj -p " + pkg
	print package_maker + arg
	os.system(package_maker + arg)	
	os.system("sudo rm -rf tmp")

def make_dmg():
	size = 16 * 1024 * 2;
	command = "/usr/bin/hdiutil"
	arg = [command, "create",  "-sectors", str(size), "-volname", disk, "-attach", "-fs", "HFS+", "YahooKeyKey-OSX-DB.dmg"]
	
	result = subprocess.Popen(arg, stdout=subprocess.PIPE).communicate()
	lines = result[0].split("\n")
	device = ""
	for line in lines:
		if line.find("Apple_HFS") > -1:
			space = line.find(" ")
			device = line[:12]
	shutil.copytree(pkg, os.path.join("/Volumes", disk , pkg))

	subprocess.Popen(["osascript", "-e", applescript])

	os.system("sleep 1")
	
	os.system("hdiutil detach " + device )
	command = "hdiutil convert YahooKeyKey-OSX-DB.dmg -format UDZO -imagekey zlib-level=9 -o " + dmg
	os.system(command)

def main():
	os.system("sudo rm -rf *.dmg")
	os.system("sudo rm -rf *.pkg")
	
	print "Building Package..."
	build_package()
	print "Done.."
	
	print "Building DMG..."
	make_dmg()
	os.system("sudo rm -rf " + pkg)
	os.system("sudo rm -rf YahooKeyKey-OSX-DB.dmg")
	
	print "Done.."
	pass


if __name__ == '__main__':
	main()

