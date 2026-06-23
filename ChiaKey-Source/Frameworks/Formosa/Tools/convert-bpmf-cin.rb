#!/usr/bin/ruby
require "native_bopomofo"

while line = $stdin.gets
  line.chomp!
  
  if line =~ /%chardef\s+begin/
    puts line
    while line = $stdin.gets
      line.chomp!
      if line =~ /%chardef\s+end/
        puts line
        break
      end
      
      data = line.split(/\s+/)
      k = NativeBopomofo.bopomofo_to_absolute_order_string(NativeBopomofo.standard_layout_string_to_bopomofo(data[0]))
      
      puts "#{k} #{data[1]}"
    end
  else
    puts line
  end
end