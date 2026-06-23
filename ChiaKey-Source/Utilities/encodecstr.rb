#!/usr/bin/ruby
#
#Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
#Copyrights licensed under the New BSD License. See the accompanying LICENSE
#file for terms.
#

while c = STDIN.getc
  if c >= 0x80
    printf "\\x%02x" % c
  else
    putc c
  end
end
