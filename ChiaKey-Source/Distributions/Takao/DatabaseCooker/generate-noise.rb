#!/usr/bin/ruby
require "rubygems"
require "uuidtools"

iteration = 1000

if ARGV.size > 0
  iteration = ARGV[0].to_i
end

puts "DROP TABLE IF EXISTS noise;";
puts "CREATE TABLE noise(text);"

puts "BEGIN;"

iteration.times do 
uuid = UUIDTools::UUID::random_create.to_s
puts "INSERT INTO noise (text) VALUES ('%s');" % uuid
end

puts "COMMIT;"
puts "VACUUM;"
