#!/usr/bin/ruby
#
#Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
#Copyrights licensed under the New BSD License. See the accompanying LICENSE
#file for terms.
#

require "digest/sha1"
require "openssl"
require "base64"

if ARGV.size < 2
  $stderr.puts "usage: digestsign file key"
  exit 1
end

begin
  input = IO.read(ARGV[0])
rescue
  $stderr.puts "cannot open #{ARGV[0]}"
  exit 1
end

begin
  keyfile = IO.read(ARGV[1])
rescue
  $stderr.puts "cannot open #{ARGV[1]}"
  exit 1
end

rsa = OpenSSL::PKey::RSA.new(keyfile)

digest = Digest::SHA1.digest(input) # + "\n"

# digest
signed_digest = rsa.private_encrypt(digest)

print signed_digest
