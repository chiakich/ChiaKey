#!/usr/bin/ruby
#
#Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
#Copyrights licensed under the New BSD License. See the accompanying LICENSE
#file for terms.
#

$KCODE="u"
require "iconv"
require "rexml/document"
require "benchmark"

half_width = 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890@#$%^&*-=_+[]{}:";\',.<>/\\'
full_width = "ａｂｃｄｅｆｇｈｉｊｋｌｍｎｏｐｑｒｓｔｕｖｗｘｙｚＡＢＣＤＥＦＧＨＩＪＫＬＭＮＯＰＱＲＳＴＵＶＷＸＹＺ１２３４５６７８９０＠＃＄％＾＆＊－＝＿＋〔〕｛｝：”；’，．＜＞／＼"

puncuations_replace = "（）？。，！、…∥「」『』《》〔〕｛｝〈』—．﹖↑﹐‧【】﹔︰·ㄅㄆㄇㄈㄉㄊㄋㄌㄍㄎㄏㄐㄑㄒㄓㄔㄕㄖㄗㄘㄙㄧㄨㄩㄚㄛㄜㄝㄞㄟㄠㄛㄢㄣㄤㄥㄦˊˇˋ˙αβγγκμⅢⅣ─■◢-〉【】〤㎜㎝◢︰﹁﹃﹑﹒﹔﹛﹝﹞"
full_width += puncuations_replace

half_width = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890-"
excluded_strings = full_width + half_width


excluded_chars = excluded_strings.unpack("U*").map {|u| [u].pack("U") }
excluded_exp = Regexp.new("(" + excluded_chars.join("|") + "|\x8|\\.|\\*|\/)")

full_width_space_exp = /　/

if ARGV.size < 1
  $stderr.puts "usage: extract.rb xml..."
  exit 1
end

files = 0
articles = 0
sentences = 0
bmark = Benchmark.measure do
while filename = ARGV.shift
  orig_text = IO.read(filename)
  if orig_text
    $stderr.puts "#{filename}"
    files += 1
    
    begin
      text = Iconv.conv("UTF-8", "BIG5-2003", orig_text)
    rescue
      $stderr.puts "Faulty iconv at #{filename}"
    end
    if text
      text.gsub!(/encoding=\"Big5\"/, "encoding=\"utf-8\"")      
      
      begin
        xml_doc = REXML::Document.new(text)
      rescue
        $stderr.puts "Faulty XML parsing at #{filename}"
      end
      
      xml_doc.root.elements.each do |element|
        if element.name == "article"
          articles += 1
          element.elements.each do |subelement|
            if subelement.name == "text"
              subelement.elements.each do |textelement|
                if textelement.name == "sentence"
                  sentences += 1
                  
                  s = textelement.text.gsub(/\(.+?\)/, "").gsub(/\[.+?\]/, "").gsub(excluded_exp, "").gsub(full_width_space_exp, " ")
                  
                  s.gsub!(/\s+/, " ")
                  s.gsub!(/^\s+/, "")
                  s.gsub!(/\s+$/, "")
                  
                  puts s if s.length > 0
                end
              end
            end
          end
        end
      end
    end
  end
end
end # Benchmark

$stderr.puts "Files: #{files}, articles: #{articles}, sentences: #{sentences}, elapsed time %.4f seconds" % bmark.real
