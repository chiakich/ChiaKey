#!/usr/bin/ruby

require "native_bopomofo"

if ARGV.size < 1
  STDERR.puts "usage: UniqueChardef <cin file>"
  exit 1
end

begin
  f = File.open(ARGV[0], "r")
rescue
  STDERR.puts "cannot open: #{ARGV[0]}"
  exit 1
end

chardef = {}

while line = f.gets
  if line =~ /%chardef\s+begin/
    while line = f.gets
      break if line =~ /%chardef\s+end/
      
      chardef[NativeBopomofo.standard_layout_string_to_bopomofo((line.split)[0])] = 1
    end
  end
end

f.close
puts chardef.keys
