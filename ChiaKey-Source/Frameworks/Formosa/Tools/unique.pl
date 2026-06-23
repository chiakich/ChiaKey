#!/usr/bin/perl
while(<>) {
    chomp;
    s/3|4|6|7//g;
    split;
    $k{$_[0]} = 1;
}

for $i (sort keys %k) {
    print "$i\n";
}
