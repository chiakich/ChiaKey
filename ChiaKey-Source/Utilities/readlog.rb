#!/usr/bin/ruby
#
#Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
#Copyrights licensed under the New BSD License. See the accompanying LICENSE
#file for terms.
#
if ARGV.size < 1
  $stderr.puts "usage: readlog.rb file acculog"
  exit 1
end

filename = ARGV.shift
acculog = ARGV.shift
accu = nil

if acculog
  accu = File.open(acculog, "a")
end

skipcount = 0

while 1
  sleep 1
  
  # we need to use this to flush some stuff in Ruby, otherwise File.open may not get the latest new file...
  system "ls > /dev/null"
  
  begin
    f = File.open(filename, "r")
    linecount = 0
    while line = f.gets
      linecount += 1
      if linecount > skipcount
        puts line
        if accu
          accu.puts line
        end
      end
    end
    f.close
    skipcount = linecount

    begin
      File.unlink(filename);
      skipcount = 0
      puts "=" * 78
      accu.puts "=" * 78
    rescue
    end
  rescue
  end  
end

accu.close
