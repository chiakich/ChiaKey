#!/usr/bin/ruby
#
#Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
#Copyrights licensed under the New BSD License. See the accompanying LICENSE
#file for terms.
#
# ignore the first two lines
$stdin.gets
$stdin.gets

while line = $stdin.gets
  line.chomp!
  data = line.split
  freq = data.shift
  
  # we only take the terms that (1) contain no non-Chinese character, (2) only one term, (3) <= 4 chars
  if data.size == 1 && data[0] !~ /[\x01-\x7f]+/ && data[0].size <= 12 && data[0].size > 3 && data[0] !~ /　/
    puts "#{data[0]} #{freq}"
  end  
end
