while line = gets
  line.chomp!  
  line += "3" if line =~ /ˇ/
  line += "2" if line =~ /ˊ/
  line += "4" if line =~ /ˋ/
  line += "5" if line =~ /˙/
  puts line
end