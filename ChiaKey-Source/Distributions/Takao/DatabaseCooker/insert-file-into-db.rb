#!/usr/bin/ruby
require "rubygems"
require "sqlite3"

if ARGV.size < 4
  STDERR.puts "usage: insert-file-into-db.rb <db> <table> <key> <filename>"
  exit 1
end

db = SQLite3::Database.new(ARGV[0])
file = IO.read(ARGV[3])
db.execute("INSERT INTO '#{ARGV[1]}' (key, value) VALUES (?, ?)", ARGV[2], file)

tsk = ARGV[2] + "_timestamp"
db.execute("INSERT INTO '#{ARGV[1]}' (key, value) VALUES (?, ?)", tsk, Time.now.to_i)
