# reads all lines and only print out unique lines, sorted
#
#Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
#Copyrights licensed under the New BSD License. See the accompanying LICENSE
#file for terms.
#

while(<>) {
chomp;
$a{$_} = 1;
}

for $x (sort(keys(%a))) {
    print "$x\n";
}
