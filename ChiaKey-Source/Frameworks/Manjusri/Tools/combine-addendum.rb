#!/usr/bin/ruby
#
#Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
#Copyrights licensed under the New BSD License. See the accompanying LICENSE
#file for terms.
#

if ARGV.size < 3
  STDERR.puts "usage: combine-addendum terms limit file1 [file2 ...]"
  exit 1
end

terms = ARGV.shift
limit = ARGV.shift.to_i

count_total = 0
lines = 0

begin
  input = File.open(terms, "r")
rescue
  STDERR.puts "Cannot open: #{terms}"
  exit 1
end

while line = input.gets
  lines += 1
  break if lines > limit
  
  data = line.split(/\s+/)
  count_total += data[1].to_i
  
  puts line
end

input.close

average = count_total / limit
STDERR.puts "total: #{count_total}, average: #{average}"

while addendum = ARGV.shift
  file = File.open(addendum, "r")
  while line = file.gets
    data = line.split(/\s+/)
    if data.size == 1 && data[0].length > 1
      puts "%s %d" % [data[0], average]
    elsif data.size > 1
      puts "%s %s" % [data[0], data[1]]
    end
  end
  file.close
end
