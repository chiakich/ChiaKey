#!/usr/bin/ruby
#
#Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
#Copyrights licensed under the New BSD License. See the accompanying LICENSE
#file for terms.
#
if ARGV.size < 1
  STDERR.puts "usage: bundle-clean bundle"
  exit 1
end

headers = Dir.glob(File.join(ARGV[0], "**", "*.h"))

removed_count = 0
headers.each do |f|
  File.unlink(f)
  removed_count += 1
end

puts "Removed #{removed_count} bundled headers" if removed_count > 0

# ARGV[0] =~ /.+\/(.+?)\..+/
# binary = File.join(ARGV[0], "Contents", "MacOS", "#{$1}")
# system "strip '#{binary}'"
# puts "Stripped: \"#{binary}\""
