#!/usr/bin/ruby

def slurp_into_hash_then_close(filename, hash, attr_when_exist, attr_when_not_exist)
  begin
    file = File.open(filename, "r")
  rescue
    STDERR.puts "Failed to open #{filename}"
    exit 1
  end
  
  while line = file.gets
    line.chomp!
    data = line.split
    word = data[0]
    
    hash[word] = hash[word] ? attr_when_exist : attr_when_not_exist      
  end
  
  file.close
end

if ARGV.size < 2
  STDERR.puts "usage: SelectionDiff file1 file2"
  exit 1
end

hash = {}

slurp_into_hash_then_close(ARGV[0], hash, nil, "D")
slurp_into_hash_then_close(ARGV[1], hash, "U", "A")

added = []
updated = []
deleted = []

hash.keys.sort.each do |k|
  case (v = hash[k])
    when "D"
      deleted << k
    when "U"
      updated << k
    when "A"
      added << k
  end
end

puts "== DELETED: "
puts deleted
puts "== ADDED: "
puts added




