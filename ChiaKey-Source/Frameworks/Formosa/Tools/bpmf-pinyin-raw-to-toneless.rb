dict = {}

while line = gets
  line.chomp!  
  line.gsub!(/ˇ|ˊ|ˋ|˙/, "")

  x = line.split[0]
  next if dict[x]
  dict[x] = true
  
  puts line
end