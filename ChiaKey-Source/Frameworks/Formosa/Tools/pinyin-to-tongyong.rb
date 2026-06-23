#!/usr/bin/ruby
while line = gets
  line.chomp!
  data = line.split
  bpmf = data[0]
  next if data[1] !~ /([a-z]+(\d*))/
  pinyin = $1
  tone = $2
  tongyong = pinyin

  # feng -> fong
  tongyong.gsub!(/feng/, "fong")

  # ue/yue -> yue, uan/yuan -> uan, un/yun -> yun, iong/yong -> yong
  tongyong.gsub!(/ue$/, "yue") if tongyong !~ /yue$/ && bpmf =~ /ㄩ/
  tongyong.gsub!(/uan$/, "yuan") if tongyong !~ /yuan$/ && bpmf =~ /ㄩ/
  tongyong.gsub!(/un$/, "yun") if tongyong !~ /yun$/ && bpmf =~ /ㄩ/
  tongyong.gsub!(/iong$/, "yong") if tongyong !~ /yong$/ && bpmf =~ /ㄩ/
  
  # ui -> uei
  tongyong.gsub!(/ui$/, "uei")
  
  # wen -> wun
  tongyong.gsub!(/wen/, "wun")
  
  # weng -> wong
  tongyong.gsub!(/weng/, "wong")
  
  # iu -> iou
  tongyong.gsub!(/iu$/, "iou")

  # ü/u/yu -> yu
  tongyong.gsub!(/^nv$/, "nyu")
  tongyong.gsub!(/^nue$/, "nyue")
  tongyong.gsub!(/^ju/, "jyu")
  tongyong.gsub!(/^qu/, "cyu")
  tongyong.gsub!(/^xu/, "sy")

  # j -> ji, q -> ci, x -> si
  tongyong.gsub!(/j/, "ji") if tongyong !~ /^ji/ && tongyong !~ /jy/
  tongyong.gsub!(/q/, "ci") if tongyong !~ /^qi/ && tongyong !~ /^qy/
  tongyong.gsub!(/qy/, "cy")
  tongyong.gsub!(/qi/, "ci")
  tongyong.gsub!(/xy/, "sy")
  tongyong.gsub!(/xi/, "si")
  tongyong.gsub!(/x/, "si")
  
  # zhi, chi, shi, ri, zi, ci, si + h
  tongyong.gsub!(/^zhi$/, "jhih")
  tongyong.gsub!(/^chi$/, "chih")
  tongyong.gsub!(/^shi$/, "shih")
  tongyong.gsub!(/^ri$/, "rih")
  tongyong.gsub!(/^zi$/, "zih")
  tongyong.gsub!(/^ci$/, "cih")
  tongyong.gsub!(/^si$/, "sih")
  
  # jh -> zh
  tongyong.gsub!(/^zh/, "jh")
    
  
  puts "#{bpmf} #{tongyong + tone}"
end
  