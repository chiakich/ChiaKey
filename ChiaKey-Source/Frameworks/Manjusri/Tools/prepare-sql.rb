#!/usr/bin/ruby
#
#Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
#Copyrights licensed under the New BSD License. See the accompanying LICENSE
#file for terms.
#

$KCODE="u"
require "benchmark"

def get_qstring_and_text_from_source(str)
  if str == "<s>"
    qstring = BOS_QSTRING
    text = ""
  elsif str == "</s>"
    qstring = EOS_QSTRING
    text = ""
  elsif str == "<unk>"
    qstring = UNK_QSTRING
    text = ""
  else
    (text, qstring) = str.split(/-/)
  end
  
  [qstring, text]
end

# query characters for Begin of String, End of String and Unknown
BOS_QSTRING = "!"
EOS_QSTRING = "$"
UNK_QSTRING = "*"

SCHEMA = <<EOS
DROP INDEX IF EXISTS unigrams_index;
DROP TABLE IF EXISTS unigrams;
DROP INDEX IF EXISTS bigrams_index;
DROP TABLE IF EXISTS bigrams;
VACUUM;
CREATE TABLE unigrams (qstring, current, probability, backoff);
CREATE TABLE bigrams (qstring, previous, current, probability);
CREATE INDEX unigrams_index ON unigrams (qstring);
CREATE INDEX unigrams_current_index ON unigrams(current);
CREATE INDEX bigrams_index ON bigrams (qstring);
EOS

if ARGV.size != 1
  $stderr.puts "usage: prepare-sql.rb lm-file"
  exit 1
end

begin
  lm = File.open(filename = ARGV.shift, "r")
rescue
  $stderr.puts "cannot open #{filename}"
  exit 1
end

puts SCHEMA
puts "BEGIN;"

unigrams = 0
bigrams = 0

bmark = Benchmark.measure do
while line = lm.gets
  line.chomp!
  
  if line =~ /1-grams:/
    while line = lm.gets
      line.chomp!
      break if line.length == 0      
      unigrams += 1
      
      (sprob, str, sbackoff) = line.split(/\t/)
      prob = sprob.to_f
      backoff = sbackoff ? sbackoff.to_f : 0.0      
      
      (qstring, text) = get_qstring_and_text_from_source(str)      
      
      if text and text.unpack("U*").size < 8
        puts "INSERT INTO unigrams VALUES ('#{qstring}', '#{text}', #{prob}, #{backoff});"
      end
    end    
  elsif line =~ /2-grams:/
    while line = lm.gets
      line.chomp!
      break if line.length == 0      
      bigrams += 1
      
      (sprob, bigramstr) = line.split(/\t/)
      prob = sprob.to_f
      b = bigramstr.split(/ /)
      (qstring1, text1) = get_qstring_and_text_from_source(b[0])
      (qstring2, text2) = get_qstring_and_text_from_source(b[1])
      
      if qstring1 and qstring2 and text1.unpack("U*").size < 4 and text2.unpack("U*").size < 4
        qstring = qstring1 + " " + qstring2
        puts "INSERT INTO bigrams VALUES ('#{qstring}', '#{text1}', '#{text2}', #{prob});"
      end
    end    
  end
end
end

lm.close
puts "COMMIT;"

$stderr.puts "conversion done, unigrams: #{unigrams}, bigrams: #{bigrams}, elapsed time #{bmark.real}"
