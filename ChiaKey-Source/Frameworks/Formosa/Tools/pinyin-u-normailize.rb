#!/usr/bin/ruby
while line = gets
  line.chomp!
  if line =~ /nu(\d|$)/ && line =~ /ㄋㄩ/
    line.gsub!(/nu/, "nv")
  elsif line =~ /lu(\d|$)/ && line =~ /ㄌㄩ/
    line.gsub!(/lu/, "lv")
  end
  
  puts line
end
