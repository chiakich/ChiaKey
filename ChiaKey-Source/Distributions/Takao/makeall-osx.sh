#!/bin/sh
echo pre-sudo for auto build
sudo echo ok, done

WORKDIR=$PWD
cd ./Installer-OSX-IMK
perl makedmg.pl

cd $WORKDIR
cd ./Installer-OSX-TSM
perl makedmg.pl

cd $WORKDIR
cd ./Installer-OSX-DB
./makedmg.py

cd $WORKDIR
cd ./Installer-OSX-ExtraModules
./makedmg.py

cd $WORKDIR
mkdir -p Staging-OSX
mv ../../build/Release64/*.dmg Staging-OSX/
mv ../../build/Release/*.dmg Staging-OSX/
mv ./Installer-OSX-DB/*.dmg Staging-OSX/
mv ./Installer-OSX-ExtraModules/*.dmg Staging-OSX/

