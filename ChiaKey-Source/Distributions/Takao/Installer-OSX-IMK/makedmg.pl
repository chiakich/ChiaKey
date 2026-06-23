#!/usr/bin/perl

# residing in ./Distributions/<Product>/OSX-IMK, so three layers up
$ENV{"BUILT_PRODUCTS_DIR"} = "../../../build/Release64/";

$ENV{WRAPPER_NAME} = "Yahoo! KeyKey.app";
$ENV{PACKAGH_NAME} = "KeyKey.pkg";
$dir = `pwd`;
chomp($dir);
$artwork_directory = $dir . "/Artwork";
$string_directory = $dir . "/strings";
$build_products_directory = $ENV{"BUILT_PRODUCTS_DIR"};
$project_target_name = "Takao (Loader OSX-IMK)";
$tmp_pkg_name = "KeyKey.pkg";
$installer_ui_name = "Installer.app";

sub mySystem
{
   ($cmd) = @_;
   print "$cmd\n";
   system($cmd) == 0 or die "System failed!";
}

#Make clean
mySystem "xcodebuild -sdk macosx10.5 -project ../../../Takao.xcodeproj -configuration Release64 clean";

#Make install
mySystem "xcodebuild -sdk macosx10.5 -project ../../../Takao.xcodeproj -configuration Release64 -target \"" . $project_target_name . "\" build";

mySystem "cc openUp.c -o openUp";
mySystem "sudo rm -rf ./tmp";
mySystem "mkdir ./tmp";
mySystem "cp -R \"" . $build_products_directory . "/" . $ENV{WRAPPER_NAME} . "\" ./tmp/.";
mySystem "sudo chown -R root:admin ./tmp/*";
mySystem "../../../ExternalLibraries/PackageMaker.app/Contents/MacOS/PackageMaker -build -ds -v -proj KeyKey25.pmproj -p \"" . $tmp_pkg_name . "\"";
mySystem "sudo rm -rf ./tmp";

################################################################################
open(INFOPLIST, "$build_products_directory/$ENV{WRAPPER_NAME}/Contents/Info.plist") or die "Can't open Info.plist: $!";

while(<INFOPLIST>)
{
   if($_ =~ /CFBundleVersion/)
   {
       <INFOPLIST> =~ />(.*)</;
       $version = $1;
   }
}

$version or die "Couldn't get version number";

print "version = $version\n";
print "-----Creating DMG\n";

@filesToInstall = ($ENV{PACKAGH_NAME});

$totalSize = 0;
foreach $file (@filesToInstall)
{
   #($sizeString) = `du -k -d0 $build_products_directory/$file`;
   ($sizeString) = `du -k -d0 \"$file\"`;
   $sizeString =~ /([0-9]+)[\s].*/;
   $size = $1;
   $totalSize += $size;
}

$imageSize = $totalSize*2 + 1.2*1024*2; #overestimate size by 5MB

system("rm -f \"$build_products_directory/Yahoo! KeyKey.dmg\"");
@hdiutilOutput = `hdiutil create -sectors $imageSize -volname "Yahoo! KeyKey for Mac OS X Leopard.localized" -attach -fs HFS+ "$build_products_directory/Yahoo! KeyKey.dmg"`;

foreach (@hdiutilOutput)
{
   if($_ =~ /Apple_HFS[\s]*([^\s].*[^\s])[\s]*$/)
   {
       $diskLocation = $1;
   }
}

$diskLocation or die "Couldn't parse hdiutil output";
if ($diskLocation =~ /\/Volumes\/(.+)/) {
   $volumeName = $1;
}
print $volumeName;

mySystem("mkdir \"$diskLocation/.Package\"");
mySystem("cp -R \"$ENV{PACKAGH_NAME}\" \"$diskLocation/.Package\"");
mySystem("mkdir \"$diskLocation/.background\"");
mySystem("cp $artwork_directory/DmgBackground.jpg \"$diskLocation/.background\"");
mySystem("cp $artwork_directory/YahooDMG.icns \"$diskLocation/.VolumeIcon.icns\"");
mySystem("SetFile -a C \"$diskLocation/.\"");
mySystem "cp -R \"" . $build_products_directory . "/" . $installer_ui_name . "\" \"$diskLocation\"";

#($date) = `date +\"\%d \%B \%Y\"`;
#chomp $date;
#$sedcommand = "sed 's/__VERSION__/$version/g
#s/__DATE__/$date/g'";
#$systemString = $sedcommand . "< $ld_directory/README.html > \"$diskLocation/README.html\"";
#mySystem($systemString);

$windowScript = 'on run -- for testing in script editor
       process_disk_image("' . $volumeName . '", "' . $artwork_directory . '")
end run

on process_disk_image(volumeName, artPath)
       tell application "Finder"
               tell disk (volumeName as string)

                       open
                       set volumeName to "Yahoo! KeyKey"
                       set theXOrigin to 40
                       set theYOrigin to 60
                       set theWidth to 450
                       set theHeight to 368

                       set theBottomRightX to (theXOrigin + theWidth)
                       set theBottomRightY to (theYOrigin + theHeight)
                       set dsStore to "\"" & "/Volumes/" & volumeName & "/" & ".DS_STORE\""
--                     do shell script "rm " & dsStore

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

                       set background picture of opts to file ".background:DmgBackground.jpg"

                       set position of item "Installer.app" to {230, 200}

--                      my copyIconOfTo(artPath & "/betaIcon.icns", "/Volumes/" & volumeName & "/OpenVanilla.app")
--                      my copyIconOfTo(artPath & "/iPhotoIcon.icns", "/Volumes/" & volumeName & "/Drop Here!")

                       tell container window
                               set statusbar visible to true
                               set statusbar visible to false
                               set the bounds to {theXOrigin + 1, theYOrigin, theBottomRightX, theBottomRightY}
                               set the bounds to {theXOrigin, theYOrigin, theBottomRightX, theBottomRightY}
                       end tell

                       update without registering applications
					   delay 2

--                     set waitTime to 0
--                     set ejectMe to false
--                     repeat while ejectMe is false
--                             delay 1
--                             set waitTime to waitTime + 1
--                             if (do shell script "[ -f " & dsStore & " ]; echo $?") = "0" then set ejectMe to true
--                     end repeat
--                     log "waited " & waitTime & " seconds for .DS_STORE to be created."
               end tell
       end tell
end process_disk_image

on copyIconOfTo(aFileOrFolderWithIcon, aFileOrFolder)
       tell application "Finder" to set f to POSIX file aFileOrFolderWithIcon as alias
       -- grab the icon
       my CopyOrPaste(f, "c")
       -- now the icon is in the clipboard
       tell application "Finder" to set c to POSIX file aFileOrFolder as alias
       my CopyOrPaste(result, "v")
end copyIconOfTo

on CopyOrPaste(i, cv)
       tell application "Finder"
               activate
               open information window of i
       end tell
       tell application "System Events" to tell process "Finder" to tell window 1
               keystroke tab -- select icon button
               keystroke (cv & "w") using command down (* (copy or paste) + close window *)
       end tell -- window 1 then process Finder then System Events
end CopyOrPaste';

system("osascript -e '$windowScript'") == 0 or die "setting window attributes in Finder failed";

mySystem("mkdir \"$diskLocation/.localized\"");
mySystem("cp $string_directory/* \"$diskLocation/.localized/.\"");

mySystem("./openUp " . "\"$diskLocation\"");
mySystem("bless --openfolder " . "\"$diskLocation\"");
system("sleep 1");
mySystem("hdiutil detach " . "\"$diskLocation\"");

print "-----Converting DMG\n";
system("rm -f \"$build_products_directory/YahooKeyKey-OSX-Leopard-" . $version . ".dmg\"");
mySystem("hdiutil convert \"$build_products_directory/Yahoo! KeyKey.dmg\" -format UDZO -imagekey zlib-level=9 -o \"$build_products_directory/YahooKeyKey-OSX-Leopard-" .  $version . ".dmg\"");
system("rm -f \"$build_products_directory/Yahoo! KeyKey.dmg\"");
print "-----Done building.\n";
system("rm openUp");
system("rm -rf \"$ENV{PACKAGH_NAME}\"");
