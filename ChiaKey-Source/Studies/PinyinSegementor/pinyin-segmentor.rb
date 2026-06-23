PINYIN_SOURCE = %W{a ai an ang ao ba bai ban bang bao bei ben beng bi bian biao bie bin bing bo bu ca cai can cang cao ce cen ceng cha chai chan chang chao che chen cheng chi chong chou chu chuai chuan chuang chui chun chuo ci cong cou cu cuan cui cun cuo da dai dan dang dao de deng di dia dian diao die ding diu dong dou du duan dui dun duo e ei en er fa fan fang fei fen feng fu ga gai gan gang gao ge gei gen geng gong gou gu gua guai guan guang gui gun guo ha hai han hang hao he hei hen heng hong hou hu hua huai huan huang hui hun huo ji jia jian jiang jiao jie jin jing jiong jiu ju juan jue jun ka kai kan kang kao ke ken keng kong kou ku kua kuai kuan kuang kui kun kuo la lai lan lang lao le lei leng li lia lian liang liao lie lin ling liu long lou lu luan lue lun luo ma mai man mang mao me mei men meng mi mian miao mie min ming mo mou mu na nai nan nang nao ne nei nen neng ni nian niang niao nie nin ning niu nong nu nuan nue nuo o ou pa pai pan pang pao pei pen peng pi pian piao pie pin ping po pou pu qi qia qian qiang qiao qie qin qing qiong qiu qu quan que qun ran rang rao re ren reng ri rong rou ru ruan rui run ruo sa sai san sang sao se sen seng sha shai shan shang shao she shen sheng shi shou shu shua shuai shuan shuang shui shun shuo si song sou su suan sui sun suo ta tai tan tang tao te teng ti tian tiao tie ting tong tou tu tuan tui tun tuo w wa wai wan wang wei wen weng wo wu x xi xia xian xiang xiao xie xin xing xinzhong xiong xiu xu xuan xue xun ya yan yang yao ye yi yin ying yo yong you yu yuan yue yun za zai zan zang zao ze zei zen zeng zha zhai zhan zhang zhao zhe zhen zheng zhi zhong zhou zhu zhua zhuai zhuan zhuang zhui zhun zhuo zi zong zou zu zuan zui zun zuo}

PINYIN_MAP = {}
PINYIN_SOURCE.each do | syllable |
  PINYIN_MAP[syllable] = true
end


$level = 0

def segmentor(source)
  $level += 1
  # puts " " * $level + "segmentor: #{source}, source length = #{source.length}"
  
  if !source || source.length < 1
    $level -= 1
    return [[]]
  end
  
  result = [[]]
  
  length = 0
  
  while length <= 6 && length < source.length
    head = (0 .. length)
    tail = (length + 1 .. source.length)
    
    head_string = source[head]
    tail_string = source[tail]

    # puts " " * $level + "length: #{length}, head: #{head_string}, tail: #{tail_string}"    
    if PINYIN_MAP[head_string]
      if tail_string && tail_string.length > 0
        tail_results = segmentor(tail_string)
        tail_results.each do | tail_result |
          result << tail_result.unshift(head_string)
        end
      else
        result << [head_string]
      end
    end
    
    length += 1
  end

  $level -=1
  result
end

input = gets.chomp
result = segmentor(input)
puts "results:"

result.each do |sentence|
  puts sentence.join(",")
end