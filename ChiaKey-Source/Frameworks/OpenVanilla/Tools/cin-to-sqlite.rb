#!/usr/bin/ruby
$KCODE = "u"

def insert_value(line, table_name, prefix = "")
  if !(line =~ /(\S+)\s+(.+)/)
    a = line.split(/\s+/)
    k = a[0] || ""
    v = a[1] || ""
  else
    k = $1 || ""
    v = $2 || ""  
  end
  
  k.gsub!(/'/, "''")
  v.gsub!(/'/, "''")
  puts "INSERT INTO '#{table_name}' VALUES('#{prefix}#{k}', '#{v}');"
end

if ARGV.size < 2
  $stderr.puts "usage: cin-to-sqlite.rb .cin table-name --chardef-only"
  exit 1
end

filename = ARGV.shift
table_name = ARGV.shift
chardef_only = (ARGV.shift == "--chardef-only")

begin
  f = File.open(filename, "r")
rescue
  $stderr.puts "cannot open: #{filename}"
  exit 1
end

puts "BEGIN;"

while line = f.gets
  line.chomp!
  
  if line =~ /^%chardef\s+begin/i
    while line = f.gets
      line.chomp!
      break if line =~ /^%chardef\s+end/i
      insert_value(line, table_name)
    end
  elsif line =~ /^%keyname\s+begin/i && !chardef_only
    while line = f.gets
      line.chomp!
      break if line =~ /^%keyname\s+end/i
      insert_value(line, table_name, "__property_keyname-")
    end
  elsif line =~ /^%/ && !chardef_only
    insert_value(line[1..line.size-1], table_name, "__property_")
  end  
end

f.close

puts "COMMIT;"