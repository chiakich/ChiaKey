#!/usr/bin/ruby
#
#Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
#Copyrights licensed under the New BSD License. See the accompanying LICENSE
#file for terms.
#

$KCODE = "U"
require "native_bopomofo"
require "mandarin_knowledge"
require "benchmark"

def multiply_array(arrays, join_char = "")
  a = arrays.shift.dup
  while b = arrays.shift
    c = []
    a.each do |x|
      b.each do |y|
        c << [x, y].join(join_char)
      end
    end  
    a = c
  end  
  c
end

def find_term(key, term_hash)
  return term_hash[key] if term_hash[key]
    
  keys = key.unpack("U*").map {|k| [k].pack("U") }
  readings = []
  
  keys.each do |k|    
    reading = term_hash[k]
    return [] if !reading
    # puts "#{k} => #{reading.join(",")}"
    readings << [reading[0]]
  end
  
  multiply_array(readings)
end

def mark_term(term, term_hash)
  if term =~ /<s>/ || term =~ /<\/s>/ || term =~ /<unk>/
    return [term]
  else
    marked = find_term(term, term_hash)
    return marked.map {|m| "#{term}-#{m}"}
  end
end

if ARGV.size < 5
  $stderr.puts "usage: lm-tonemarker lm-file bpmf-cin bpmf-src output-lm output-alias_chars"
  exit 1
end

input_lm = ARGV.shift
bpmf_cin = ARGV.shift
bpmf_src = ARGV.shift
output_lm = ARGV.shift
output_alias_chars = ARGV.shift
terms = {}
alias_chars = {}

# output the alias_chars file, also compile the alias_chars table
file_output_alias_chars = File.open(output_alias_chars, "w")

while t = CHARS_WITH_ALIAS.shift
  p = CHARS_WITH_ALIAS.shift.split(/,/)
  a = NativeBopomofo.bopomofo_to_absolute_order_string(p[0]) + '#'
  terms[t] = [a]
  alias_chars[t] = a
  
  p.each do |ph|
    file_output_alias_chars.puts "    m[\"#{NativeBopomofo.bopomofo_to_absolute_order_string(ph).gsub(/\//, "\\\\")}\"] = \"#{a}\";"
  end
end
file_output_alias_chars.close

# read the entries that we need to duplicate in the unigram
while t = DUPLICATE_ENTRIES_IN_UNIGRAM.shift
  p = DUPLICATE_ENTRIES_IN_UNIGRAM.shift.split(/,/).map {|ph| NativeBopomofo.bopomofo_to_absolute_order_string(ph) }
  terms[t] = p
  # puts "#{t} #{p.join(" * ")}"
end

# and the entries that we only use the first sound
while n = FIRST_ENTRY_ONLY.shift
  t = FIRST_ENTRY_ONLY.shift
  p = FIRST_ENTRY_ONLY.shift.split(/,/).map {|ph| NativeBopomofo.bopomofo_to_absolute_order_string(ph) }
  terms[t] = [p[n.to_i]] || [p[0]]
  # puts "#{t} #{terms[t]}"
end

# now we read BPMF.cin
bpmf = {}

$stderr.puts "reading bpmf.cin"
b = Benchmark.measure do
file_bpmf_cin = File.open(bpmf_cin, "r")
while line = file_bpmf_cin.gets
  if line =~ /%chardef/
    while line = file_bpmf_cin.gets
      break if line =~ /%chardef/
      d = line.chomp.split(/\s+/)
      
      ph = NativeBopomofo.standard_layout_string_to_bopomofo(d[0])
      ph = NativeBopomofo.bopomofo_to_absolute_order_string(ph)

      # puts "#{d[1]} => #{d[0]} => #{ph}"      
      
      if bpmf[d[1]]
        bpmf[d[1]] << ph
      else
        bpmf[d[1]] = [ph]
      end
    end
  end
end
file_bpmf_cin.close
end

# $stderr.puts "鮮 = #{bpmf["鮮"].join(",")}"
$stderr.puts "bpmf.cin read: #{b.real} secs"

bpmf.merge!(terms)
# $stderr.puts "鮮 = #{bpmf["鮮"].join(",")}"

# bpmf.keys.sort.each do |k|
#   if bpmf[k].size > 1
#     p = bpmf[k].map {|ph| NativeBopomofo.absolute_order_string_to_bopomofo(ph) }
#     puts "#{k} => #{p.join("*")}"
#   end
# end

terms = bpmf

# puts "xyz => #{find_term("xyz", terms)}"
# k = "一代不如一代吧"
# puts "#{k} => #{find_term(k, terms).join(" , ")}"
# exit 0

$stderr.puts "reading bpmf mapping source"
b = Benchmark.measure do
file_bpmf_src = File.open(bpmf_src, "r")
while line = file_bpmf_src.gets
  d = line.chomp.split(/\s+/)
  key = d.shift  
  keys = key.unpack("U*").map {|k| [k].pack("U") }
  
  next if keys.size < 2
  
  freq = d.shift
  sound = ""
  
  (0..(d.size-1)).each do |i|
    # print "#{i} -> #{d[i]}"
    k = keys[i]
    
    # only alias the first sound-char
    if alias_chars[k] && i == 0
      sound += alias_chars[k]
    else
      sound += NativeBopomofo.bopomofo_to_absolute_order_string(d[i])
    end
  end

  if terms[key]
    if !terms[key].include? sound
      terms[key] << sound
    end
  else
    terms[key] = [sound]
  end
  
  # puts "#{key} => #{d.join(",")} => #{sound}"
end
file_bpmf_src.close
end
$stderr.puts "bpmf source read: #{b.real} secs"

# k = "一代不如一代"
# puts "#{k} => #{terms[k].join(",")}"


# start reading the lm file
$stderr.puts "start reading the lm file"
b = Benchmark.measure do
  file_input_lm = File.open(input_lm, "r")
  file_output_lm = File.open(output_lm, "w")

  while line = file_input_lm.gets
    if line =~ /\\1-grams:/
      file_output_lm.puts line

      while line = file_input_lm.gets
        if line == "\n"
          file_output_lm.puts line
          break
        end
        
        (freq, term, backoff) = line.chomp.split(/\t/)
        mark_term(term, terms).each do |m|
          file_output_lm.puts "#{freq}\t#{m}" + (backoff ? "\t#{backoff}" : "")
        end
      end
    elsif line =~ /\\2-grams:/
      file_output_lm.puts line

      while line = file_input_lm.gets
        if line == "\n"
          file_output_lm.puts line
          break
        end

        (freq, term12) = line.chomp.split(/\t/)
        (term1, term2) = term12.split(/ /)
        mt1 = mark_term(term1, terms)
        mt2 = mark_term(term2, terms)
        
        if mt1.size > 0 && mt2.size > 0
          combined = multiply_array([mt1, mt2], " ")
          combined.each do |m|
            file_output_lm.puts "#{freq}\t#{m}"
          end
        end
      end    
    else
      file_output_lm.puts line
    end
  end

  file_input_lm.close
  file_output_lm.close
end
$stderr.puts "marked the lm file: #{b.real} secs"
