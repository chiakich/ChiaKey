#!/usr/bin/ruby
#
#Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
#Copyrights licensed under the New BSD License. See the accompanying LICENSE
#file for terms.
#

if ARGV.size < 1
  STDERR.puts "usage: bin2c blockname"
  exit 1
end

block = []

while c = STDIN.getc
  block << c
end

puts "#include <stdlib.h>"
puts "extern char #{ARGV[0]}[];"
puts "extern size_t #{ARGV[0]}Size;"
puts
puts "size_t #{ARGV[0]}Size = #{block.size};"
puts "char #{ARGV[0]}[#{block.size}] = {"

cntr = 0
last = block.size

print "\t"
block.each do |b|
  cntr += 1;
  print "0x%02x" % b
  print ", " if cntr != last
    
  if (cntr % 10) == 0
    puts 
    print "\t" if cntr != last 
  end
end

puts if (cntr % 10) != 0
puts "};"
puts 
