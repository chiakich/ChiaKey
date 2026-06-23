#!/usr/bin/perl
#
#Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
#Copyrights licensed under the New BSD License. See the accompanying LICENSE
#file for terms.
#

# skip the first line
@phrases = ();

while(<>) {
    chomp;    
    split(/\t/);
    # print "$_[0] => $_[1]\n";
    
    push(@phrases, [$_[0], $_[1]]);
}

@phrases = sort { int($b->[0]) <=> int($a->[0]) } @phrases;

# remove the first line which is the column header
shift @phrases;

for $x (@phrases) {
    print $x->[0], " ", $x->[1], "\n";
}

