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

random1 = []
random2 = []

offset = (1024 + (rand * 2048).to_i)
offset.times do
  random1 << (rand * 256).to_i
end

(1024 + (rand * 2048).to_i).times do
  random2 << (rand * 256).to_i
end

block = random1

realsize = 0
while c = STDIN.getc
  block << c
  realsize += 1
end

block += random2

# make the padding
(block.size % 16).times do
  block << (rand * 256).to_i
end

sizemask1 = 0x80000000 + (rand * 0xfffffff).to_i
sizemask2 = realsize ^ sizemask1
sizemask1 = sizemask1 ^ 0xffddffaa;
offsetmask1 = 0x80000000 + (rand * 0xfffffff).to_i
offsetmask2 = offset ^ offsetmask1;
offsetmask1 = offsetmask1 ^ 0xaaddffff;

padding1 = (0x80000000 + (rand * 0xfffffff).to_i) ^ 0xffffffff
padding2 = (0x80000000 + (rand * 0xfffffff).to_i) ^ 0xaaaaaaaa
padding3 = (0x80000000 + (rand * 0xfffffff).to_i) ^ 0xdddddddd
padding4 = (0x80000000 + (rand * 0xfffffff).to_i) ^ 0xffaaffdd


STDERR.puts("real size = #{realsize}, offset = #{offset}")

puts "#include <stdlib.h>"
puts "extern char #{ARGV[0]}[];"
puts "extern size_t #{ARGV[0]}SizeMask1;"
puts "extern size_t #{ARGV[0]}SizeMask2;"
puts "extern size_t #{ARGV[0]}OffsetMask1;"
puts "extern size_t #{ARGV[0]}OffsetMask2;"
puts
puts "size_t #{ARGV[0]}SizeMask1 = 0x%08x;" % sizemask1
puts "size_t #{ARGV[0]}Padding1 = 0x%08x;" % padding1

puts "size_t #{ARGV[0]}SizeMask2 = 0x%08x;" % sizemask2
puts "size_t #{ARGV[0]}Padding2 = 0x%08x;" % padding2

puts "size_t #{ARGV[0]}OffsetMask1 = 0x%08x;" % offsetmask1
puts "size_t #{ARGV[0]}Padding3 = 0x%08x;" % padding3

puts "size_t #{ARGV[0]}OffsetMask2 = 0x%08x;" % offsetmask2
puts "size_t #{ARGV[0]}Padding4 = 0x%08x;" % padding4

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
