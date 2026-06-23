/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/

#include <UnitTest++.h>

#include <iostream>

#include "Mandarin.h"

using namespace std;
using namespace Formosa::Mandarin;

TEST(BopomofoSyllable) {
  BPMF b(BPMF::B);
  CHECK_EQUAL("ㄅ", b.composedString());

  BPMF i(BPMF::I);
  CHECK_EQUAL("ㄧ", i.composedString());

  BPMF tone3(BPMF::Tone3);
  CHECK_EQUAL("ˇ", tone3.composedString());

  BPMF bi3;
  bi3 += i;
  bi3 += tone3;
  bi3 += b;
  CHECK_EQUAL("ㄅㄧˇ", bi3.composedString());

  BPMF another = BPMF::FromComposedString("ㄅㄧˇ");
  CHECK_EQUAL(bi3, another);
}

TEST(BopomofoKeyboardLayouts) {
  BPMF leng3 = BPMF(BPMF::L) + BPMF(BPMF::ENG) + BPMF(BPMF::Tone3);

  const BopomofoKeyboardLayout* layout =
      BopomofoKeyboardLayout::StandardLayout();
  CHECK_EQUAL('1', layout->componentToKey(BPMF::B));
  CHECK_EQUAL((BPMF::Component)BPMF::B, layout->keyToComponents('1')[0]);
  CHECK_EQUAL("x/3", layout->keySequenceFromSyllable(leng3));

  layout = BopomofoKeyboardLayout::ETenLayout();
  CHECK_EQUAL('b', layout->componentToKey(BPMF::B));
  CHECK_EQUAL((BPMF::Component)BPMF::B, layout->keyToComponents('b')[0]);
  CHECK_EQUAL("l-3", layout->keySequenceFromSyllable(leng3));

  layout = BopomofoKeyboardLayout::ETen26Layout();
  CHECK_EQUAL('m', layout->componentToKey(BPMF::M));
  CHECK_EQUAL('m', layout->componentToKey(BPMF::AN));
  CHECK_EQUAL((BPMF::Component)BPMF::M, layout->keyToComponents('m')[0]);
  CHECK_EQUAL((BPMF::Component)BPMF::AN, layout->keyToComponents('m')[1]);
  CHECK_EQUAL("llj", layout->keySequenceFromSyllable(leng3));

  layout = BopomofoKeyboardLayout::HsuLayout();
  CHECK_EQUAL('l', layout->componentToKey(BPMF::L));
  CHECK_EQUAL('l', layout->componentToKey(BPMF::ENG));
  CHECK_EQUAL('l', layout->componentToKey(BPMF::ERR));
  CHECK_EQUAL((BPMF::Component)BPMF::L, layout->keyToComponents('l')[0]);
  CHECK_EQUAL((BPMF::Component)BPMF::ENG, layout->keyToComponents('l')[1]);
  CHECK_EQUAL((BPMF::Component)BPMF::ERR, layout->keyToComponents('l')[2]);
  CHECK_EQUAL("llf", layout->keySequenceFromSyllable(leng3));
}

struct RoundTripFixture {
  RoundTripFixture() {
    // we get this "unique BPMF standard layout combinations by using the
    // following Perl against the %chardef section of bpmf-ext.cin (public
    // domain):
    //
    //  while(<>) { chomp; split; $m{$_[0]} = $_[1]; }
    //  print "const char* keys[", scalar keys %m, "] = {\n";
    //  for $k (sort keys %m) { print "\"$k\","; if ($c++ > 10) { print "\n"; $c
    //  = 0; }  } print "}\n;"

    static char* sequences[] = {
        ",",    ",4",   "-",    "-3",   "-4",   "-6",   ".",    ".3",   ".4",
        ".6",   "/",    "0",    "03",   "04",   "06",   "1",    "1/",   "1/3",
        "1/4",  "1/6",  "10",   "103",  "104",  "18",   "183",  "184",  "186",
        "187",  "19",   "193",  "194",  "196",  "1;",   "1;3",  "1;4",  "1i",
        "1i3",  "1i4",  "1i6",  "1i7",  "1j",   "1j3",  "1j4",  "1j6",  "1j7",
        "1l",   "1l3",  "1l4",  "1l6",  "1o",   "1o3",  "1o4",  "1p",   "1p3",
        "1p4",  "1u",   "1u,",  "1u,3", "1u,4", "1u,6", "1u/",  "1u/3", "1u/4",
        "1u0",  "1u03", "1u04", "1u3",  "1u4",  "1u6",  "1ul",  "1ul3", "1ul4",
        "1up",  "1up3", "1up4", "2",    "2.",   "2.3",  "2.4",  "2/",   "2/3",
        "2/4",  "20",   "203",  "204",  "28",   "283",  "284",  "286",  "287",
        "29",   "293",  "294",  "2;",   "2;3",  "2;4",  "2j",   "2j/",  "2j/3",
        "2j/4", "2j0",  "2j03", "2j04", "2j3",  "2j4",  "2j6",  "2ji",  "2ji3",
        "2ji4", "2ji6", "2jo",  "2jo3", "2jo4", "2jp",  "2jp3", "2jp4", "2jp6",
        "2k6",  "2k7",  "2l",   "2l3",  "2l4",  "2l6",  "2o3",  "2p4",  "2u",
        "2u,",  "2u,3", "2u,4", "2u,6", "2u.",  "2u/",  "2u/3", "2u/4", "2u/6",
        "2u0",  "2u03", "2u04", "2u3",  "2u4",  "2u6",  "2u8",  "2u83", "2u;",
        "2ul",  "2ul3", "2ul4", "3",    "4",    "5",    "5.",   "5.3",  "5.4",
        "5.6",  "5/",   "5/3",  "5/4",  "50",   "503",  "504",  "53",   "54",
        "56",   "58",   "583",  "584",  "586",  "59",   "593",  "594",  "596",
        "5;",   "5;3",  "5;4",  "5j",   "5j/",  "5j/3", "5j/4", "5j0",  "5j03",
        "5j04", "5j3",  "5j4",  "5j6",  "5j8",  "5j83", "5j9",  "5j93", "5j94",
        "5j;",  "5j;3", "5j;4", "5ji",  "5ji3", "5ji4", "5ji6", "5jo",  "5jo3",
        "5jo4", "5jp",  "5jp3", "5jp4", "5k",   "5k3",  "5k4",  "5k6",  "5k7",
        "5l",   "5l3",  "5l4",  "5l6",  "5o4",  "5p",   "5p3",  "5p4",  "6",
        "7",    "8",    "83",   "84",   "86",   "87",   "9",    "93",   "94",
        "96",   ";",    ";3",   ";4",   ";6",   "a",    "a.3",  "a.4",  "a.6",
        "a/3",  "a/4",  "a/6",  "a03",  "a04",  "a06",  "a8",   "a83",  "a84",
        "a86",  "a87",  "a93",  "a94",  "a96",  "a;3",  "a;4",  "a;6",  "ai",
        "ai3",  "ai4",  "ai6",  "ai7",  "aj3",  "aj4",  "aj6",  "ak7",  "al",
        "al3",  "al4",  "al6",  "ao3",  "ao4",  "ao6",  "ap",   "ap3",  "ap4",
        "ap6",  "ap7",  "au",   "au,",  "au,4", "au,6", "au.",  "au.4", "au.6",
        "au/3", "au/4", "au/6", "au03", "au04", "au06", "au3",  "au4",  "au6",
        "aul",  "aul3", "aul4", "aul6", "aup3", "aup6", "b",    "b.3",  "b.4",
        "b.6",  "b/",   "b/3",  "b/4",  "b/6",  "b03",  "b06",  "b4",   "b;3",
        "b;4",  "b;6",  "bj/3", "bj/4", "bj/6", "bj03", "bj04", "bj06", "bj3",
        "bj4",  "bj6",  "bji4", "bji6", "bjo3", "bjo4", "bjo6", "bjp",  "bjp3",
        "bjp4", "bjp6", "bk3",  "bk4",  "bl3",  "bl4",  "bl6",  "bp3",  "bp4",
        "bp6",  "c",    "c.",   "c.3",  "c.4",  "c.6",  "c/",   "c/4",  "c/6",
        "c/7",  "c0",   "c03",  "c04",  "c06",  "c8",   "c83",  "c86",  "c87",
        "c9",   "c93",  "c94",  "c96",  "c;",   "c;3",  "c;4",  "c;6",  "cj",
        "cj/",  "cj/3", "cj/4", "cj/6", "cj0",  "cj03", "cj04", "cj06", "cj3",
        "cj4",  "cj6",  "cj8",  "cj83", "cj84", "cj86", "cj94", "cj96", "cj;",
        "cj;3", "cj;4", "cj;6", "cj;7", "cji",  "cji3", "cji4", "cji6", "cji7",
        "cjo",  "cjo3", "cjo4", "cjo6", "cjp",  "cjp3", "cjp4", "cjp6", "ck",
        "ck3",  "ck4",  "ck6",  "cl",   "cl3",  "cl4",  "cl6",  "co",   "co3",
        "cp3",  "cp4",  "cp6",  "d",    "d.",   "d.3",  "d.4",  "d/",   "d/3",
        "d0",   "d03",  "d04",  "d8",   "d83",  "d84",  "d9",   "d93",  "d94",
        "d;",   "d;3",  "d;4",  "d;6",  "dj",   "dj/",  "dj/3", "dj/4", "dj0",
        "dj03", "dj3",  "dj4",  "dj8",  "dj83", "dj84", "dj9",  "dj93", "dj94",
        "dj;",  "dj;3", "dj;4", "dj;6", "dji3", "dji4", "djo",  "djo3", "djo4",
        "djo6", "djp",  "djp3", "djp4", "dk",   "dk3",  "dk4",  "dk6",  "dl",
        "dl3",  "dl4",  "dp3",  "dp4",  "e",    "e.",   "e.3",  "e.4",  "e.6",
        "e/",   "e/3",  "e/4",  "e0",   "e03",  "e04",  "e8",   "e83",  "e84",
        "e86",  "e87",  "e9",   "e93",  "e94",  "e;",   "e;3",  "e;4",  "ej",
        "ej/",  "ej/3", "ej/4", "ej0",  "ej03", "ej04", "ej3",  "ej4",  "ej6",
        "ej8",  "ej83", "ej84", "ej86", "ej9",  "ej93", "ej94", "ej;",  "ej;3",
        "ej;4", "eji",  "eji3", "eji4", "eji6", "eji7", "ejo",  "ejo3", "ejo4",
        "ejp3", "ejp4", "ek",   "ek3",  "ek4",  "ek6",  "ek7",  "el",   "el3",
        "el4",  "eo3",  "ep",   "ep3",  "ep4",  "ep6",  "f",    "f7",   "fm",
        "fm,",  "fm,4", "fm,6", "fm/",  "fm/3", "fm/4", "fm/6", "fm0",  "fm03",
        "fm04", "fm06", "fm3",  "fm4",  "fm6",  "fmp",  "fmp3", "fmp4", "fmp6",
        "fu",   "fu,",  "fu,3", "fu,4", "fu,6", "fu.",  "fu.3", "fu.4", "fu.6",
        "fu/",  "fu/3", "fu/4", "fu/6", "fu0",  "fu03", "fu04", "fu06", "fu3",
        "fu4",  "fu6",  "fu8",  "fu83", "fu84", "fu86", "fu;",  "fu;3", "fu;4",
        "fu;6", "ful",  "ful3", "ful4", "ful6", "fup",  "fup3", "fup4", "fup6",
        "g",    "g.",   "g.3",  "g.4",  "g.6",  "g/",   "g/3",  "g/4",  "g/6",
        "g0",   "g03",  "g04",  "g06",  "g3",   "g4",   "g6",   "g7",   "g8",
        "g83",  "g84",  "g86",  "g9",   "g93",  "g94",  "g;",   "g;3",  "g;4",
        "g;7",  "gj",   "gj0",  "gj04", "gj3",  "gj4",  "gj6",  "gj8",  "gj83",
        "gj84", "gj9",  "gj93", "gj94", "gj;",  "gj;3", "gj;4", "gji",  "gji4",
        "gjo3", "gjo4", "gjo6", "gjp3", "gjp4", "gk",   "gk3",  "gk4",  "gk6",
        "gl",   "gl3",  "gl4",  "gl6",  "go6",  "gp",   "gp3",  "gp4",  "gp6",
        "h",    "h.4",  "h.6",  "h/",   "h/3",  "h/4",  "h/6",  "h0",   "h03",
        "h04",  "h06",  "h3",   "h4",   "h6",   "h8",   "h83",  "h84",  "h9",
        "h93",  "h94",  "h96",  "h;",   "h;3",  "h;4",  "h;6",  "hj",   "hj/",
        "hj/3", "hj/4", "hj/6", "hj0",  "hj04", "hj06", "hj3",  "hj4",  "hj6",
        "hji",  "hji3", "hji4", "hji6", "hjo",  "hjo3", "hjo4", "hjo6", "hjp",
        "hjp3", "hjp4", "hjp6", "hk4",  "hl",   "hl3",  "hl4",  "hl6",  "hp",
        "hp6",  "i",    "i3",   "i4",   "i6",   "j",    "j/",   "j/3",  "j/4",
        "j0",   "j03",  "j04",  "j06",  "j3",   "j4",   "j6",   "j8",   "j83",
        "j84",  "j86",  "j87",  "j9",   "j93",  "j94",  "j;",   "j;3",  "j;4",
        "j;6",  "ji",   "ji3",  "ji4",  "jo",   "jo3",  "jo4",  "jo6",  "jp",
        "jp3",  "jp4",  "jp6",  "k",    "k3",   "k4",   "k6",   "l",    "l3",
        "l4",   "l6",   "m",    "m,",   "m,3",  "m,4",  "m/",   "m/3",  "m/4",
        "m/6",  "m0",   "m03",  "m04",  "m06",  "m3",   "m4",   "m6",   "mp",
        "mp3",  "mp4",  "mp6",  "n",    "n.",   "n.3",  "n.4",  "n/",   "n/4",
        "n0",   "n03",  "n04",  "n3",   "n4",   "n7",   "n8",   "n83",  "n84",
        "n87",  "n9",   "n93",  "n94",  "n;",   "n;3",  "n;4",  "nj",   "nj/",
        "nj/3", "nj/4", "nj/6", "nj0",  "nj03", "nj04", "nj3",  "nj4",  "nj6",
        "nji",  "nji3", "nji4", "nji6", "njo",  "njo3", "njo4", "njo6", "njp",
        "njp3", "njp4", "nk4",  "nl",   "nl3",  "nl4",  "no",   "np",   "np3",
        "o",    "o4",   "p",    "p3",   "p4",   "q",    "q.",   "q.3",  "q.4",
        "q.6",  "q/",   "q/3",  "q/4",  "q/6",  "q0",   "q03",  "q04",  "q06",
        "q8",   "q83",  "q84",  "q86",  "q87",  "q9",   "q93",  "q94",  "q96",
        "q;",   "q;3",  "q;4",  "q;6",  "qi",   "qi3",  "qi4",  "qi6",  "qj",
        "qj3",  "qj4",  "qj6",  "ql",   "ql3",  "ql4",  "ql6",  "qo",   "qo3",
        "qo4",  "qo6",  "qp",   "qp3",  "qp4",  "qp6",  "qu",   "qu,",  "qu,3",
        "qu,4", "qu/",  "qu/3", "qu/4", "qu/6", "qu0",  "qu03", "qu04", "qu06",
        "qu3",  "qu4",  "qu6",  "qul",  "qul3", "qul4", "qul6", "qup",  "qup3",
        "qup4", "qup6", "r",    "rm",   "rm,",  "rm,3", "rm,4", "rm,6", "rm/",
        "rm/3", "rm/4", "rm0",  "rm03", "rm04", "rm3",  "rm4",  "rm6",  "rmp",
        "rmp3", "rmp4", "ru",   "ru,",  "ru,3", "ru,4", "ru,6", "ru.",  "ru.3",
        "ru.4", "ru/",  "ru/3", "ru/4", "ru0",  "ru03", "ru04", "ru3",  "ru4",
        "ru6",  "ru8",  "ru83", "ru84", "ru86", "ru;",  "ru;3", "ru;4", "rul",
        "rul3", "rul4", "rul6", "rup",  "rup3", "rup4", "s",    "s.3",  "s.4",
        "s.6",  "s.7",  "s/3",  "s/4",  "s/6",  "s0",   "s03",  "s04",  "s06",
        "s83",  "s84",  "s86",  "s87",  "s93",  "s94",  "s96",  "s;",   "s;3",
        "s;4",  "s;6",  "s;7",  "sj/3", "sj/4", "sj/6", "sj03", "sj04", "sj06",
        "sj3",  "sj4",  "sj6",  "sji3", "sji4", "sji6", "sjp4", "sjp6", "sk",
        "sk4",  "sk6",  "sk7",  "sl",   "sl3",  "sl4",  "sl6",  "sm,4", "sm3",
        "sm4",  "sm6",  "so3",  "so4",  "so6",  "sp3",  "sp4",  "su,",  "su,3",
        "su,4", "su,6", "su.",  "su.3", "su.4", "su.6", "su/3", "su/4", "su/6",
        "su03", "su04", "su06", "su3",  "su4",  "su6",  "su86", "su;3", "su;4",
        "su;6", "sul3", "sul4", "sup3", "sup4", "sup6", "t",    "t.",   "t.3",
        "t.4",  "t.6",  "t/",   "t/3",  "t/4",  "t/6",  "t0",   "t03",  "t04",
        "t06",  "t3",   "t4",   "t6",   "t8",   "t83",  "t84",  "t86",  "t9",
        "t93",  "t94",  "t96",  "t;",   "t;3",  "t;4",  "t;6",  "tj",   "tj/",
        "tj/3", "tj/4", "tj/6", "tj0",  "tj03", "tj04", "tj06", "tj3",  "tj4",
        "tj6",  "tj8",  "tj83", "tj84", "tj93", "tj94", "tj96", "tj;",  "tj;3",
        "tj;4", "tj;6", "tji",  "tji4", "tjo",  "tjo3", "tjo4", "tjo6", "tjp",
        "tjp3", "tjp6", "tk",   "tk3",  "tk4",  "tk6",  "tl",   "tl3",  "tl4",
        "tl6",  "tp",   "tp3",  "tp4",  "tp6",  "u",    "u,",   "u,3",  "u,4",
        "u,6",  "u.",   "u.3",  "u.4",  "u.6",  "u/",   "u/3",  "u/4",  "u/6",
        "u0",   "u03",  "u04",  "u06",  "u3",   "u4",   "u6",   "u8",   "u83",
        "u84",  "u86",  "u96",  "u;",   "u;3",  "u;4",  "u;6",  "ui",   "ui7",
        "ul",   "ul3",  "ul4",  "ul6",  "up",   "up3",  "up4",  "up6",  "v",
        "vm",   "vm,",  "vm,3", "vm,4", "vm,6", "vm/",  "vm/3", "vm/4", "vm/6",
        "vm0",  "vm03", "vm04", "vm06", "vm3",  "vm4",  "vm6",  "vmp",  "vmp4",
        "vmp6", "vu",   "vu,",  "vu,3", "vu,4", "vu,6", "vu.",  "vu.3", "vu.4",
        "vu/",  "vu/3", "vu/4", "vu/6", "vu/7", "vu0",  "vu03", "vu04", "vu06",
        "vu3",  "vu4",  "vu6",  "vu8",  "vu83", "vu84", "vu86", "vu;",  "vu;3",
        "vu;4", "vu;6", "vul",  "vul3", "vul4", "vul6", "vup",  "vup3", "vup4",
        "vup6", "w",    "w.",   "w.3",  "w.4",  "w.6",  "w.7",  "w/4",  "w/6",
        "w0",   "w03",  "w04",  "w06",  "w8",   "w83",  "w84",  "w86",  "w9",
        "w93",  "w94",  "w96",  "w;",   "w;3",  "w;4",  "w;6",  "wj",   "wj/",
        "wj/3", "wj/4", "wj/6", "wj0",  "wj03", "wj04", "wj06", "wj3",  "wj4",
        "wj6",  "wji",  "wji3", "wji4", "wji6", "wjo",  "wjo3", "wjo4", "wjo6",
        "wjp",  "wjp3", "wjp4", "wjp6", "wk4",  "wl",   "wl3",  "wl4",  "wl6",
        "wl7",  "wu",   "wu,",  "wu,3", "wu,4", "wu/",  "wu/3", "wu/4", "wu/6",
        "wu0",  "wu03", "wu04", "wu06", "wu3",  "wu4",  "wu6",  "wul",  "wul3",
        "wul4", "wul6", "x",    "x.",   "x.3",  "x.4",  "x.6",  "x.7",  "x/3",
        "x/4",  "x/6",  "x03",  "x04",  "x06",  "x8",   "x83",  "x84",  "x86",
        "x87",  "x93",  "x94",  "x96",  "x;",   "x;3",  "x;4",  "x;6",  "xi7",
        "xj",   "xj/3", "xj/4", "xj/6", "xj03", "xj04", "xj06", "xj3",  "xj4",
        "xj6",  "xji",  "xji3", "xji4", "xji6", "xjp",  "xjp3", "xjp4", "xjp6",
        "xk4",  "xk7",  "xl",   "xl3",  "xl4",  "xl6",  "xm,4", "xm03", "xm06",
        "xm3",  "xm4",  "xm6",  "xmp6", "xo",   "xo3",  "xo4",  "xo6",  "xu",
        "xu,",  "xu,3", "xu,4", "xu,6", "xu.",  "xu.3", "xu.4", "xu.6", "xu.7",
        "xu/",  "xu/3", "xu/4", "xu/6", "xu03", "xu04", "xu06", "xu3",  "xu4",
        "xu6",  "xu7",  "xu83", "xu;3", "xu;4", "xu;6", "xul",  "xul3", "xul4",
        "xul6", "xup3", "xup4", "xup6", "y",    "y.",   "y.3",  "y.4",  "y/",
        "y/3",  "y/4",  "y0",   "y03",  "y04",  "y06",  "y3",   "y4",   "y7",
        "y8",   "y83",  "y86",  "y9",   "y93",  "y94",  "y;",   "y;3",  "y;4",
        "yj",   "yj/",  "yj/3", "yj/4", "yj0",  "yj03", "yj04", "yj3",  "yj4",
        "yj6",  "yji3", "yji4", "yji6", "yjo",  "yjo3", "yjo4", "yjp",  "yjp3",
        "yjp4", "yk3",  "yk4",  "yk6",  "yl",   "yl3",  "yl4",  "yl6",  "yo6",
        "yp3",  "yp4",  "z",    "z.3",  "z.4",  "z.6",  "z/",   "z/3",  "z/4",
        "z/6",  "z0",   "z03",  "z04",  "z06",  "z8",   "z83",  "z84",  "z86",
        "z;",   "z;3",  "z;4",  "z;6",  "zi4",  "zi6",  "zj",   "zj/4", "zj3",
        "zj4",  "zj6",  "zo",   "zo3",  "zo4",  "zo6",  "zp",   "zp3",  "zp4",
        "zp6",  "zul4"};

    standardSequences = sequences;
    sequenceCount = sizeof(sequences) / sizeof(char*);

    ETen26ExcludedSequences[","] = true;   // ㄝ(ETen26)
    ETen26ExcludedSequences[",4"] = true;  // ㄝˋ(ETen26)
    ETen26ExcludedSequences["3"] = true;   // ˇ(ETen26)
    ETen26ExcludedSequences["4"] = true;   // ˋ(ETen26)
    ETen26ExcludedSequences["6"] = true;   // ˊ(ETen26)
    ETen26ExcludedSequences["7"] = true;   // ˙(ETen26)
    ETen26ExcludedSequences["a"] = true;   // ㄇ(ETen26)
    ETen26ExcludedSequences["c"] = true;   // ㄏ(ETen26)
    ETen26ExcludedSequences["f"] = true;   // ㄑ(ETen26)
    ETen26ExcludedSequences["f7"] = true;  // ㄑ˙(ETen26)
    ETen26ExcludedSequences["o"] = true;   // ㄟ(ETen26)
    ETen26ExcludedSequences["o4"] = true;  // ㄟˋ(ETen26)
    ETen26ExcludedSequences["q"] = true;   // ㄆ(ETen26)
    ETen26ExcludedSequences["r"] = true;   // ㄐ(ETen26)
    ETen26ExcludedSequences["s"] = true;   // ㄋ(ETen26)
    ETen26ExcludedSequences["v"] = true;   // ㄒ(ETen26)
    ETen26ExcludedSequences["w"] = true;   // ㄊ(ETen26)
    ETen26ExcludedSequences["x"] = true;   // ㄌ(ETen26)
    HsuExcludedSequences[","] = true;      // ㄝ (Hsu)
    HsuExcludedSequences[",4"] = true;     // ㄝˋ (Hsu)
    HsuExcludedSequences["/"] = true;      // ㄥ (Hsu)
    HsuExcludedSequences["3"] = true;      // ˇ (Hsu)
    HsuExcludedSequences["4"] = true;      // ˋ (Hsu)
    HsuExcludedSequences["6"] = true;      // ˊ (Hsu)
    HsuExcludedSequences["7"] = true;      // ˙ (Hsu)
    HsuExcludedSequences["a"] = true;      // ㄇ (Hsu)
    HsuExcludedSequences["c"] = true;      // ㄏ (Hsu)
    HsuExcludedSequences["d"] = true;      // ㄎ (Hsu)
    HsuExcludedSequences["e"] = true;      // ㄍ (Hsu)
    HsuExcludedSequences["f"] = true;      // ㄑ (Hsu)
    HsuExcludedSequences["f7"] = true;     // ㄑ˙ (Hsu)
    HsuExcludedSequences["o"] = true;      // ㄟ (Hsu)
    HsuExcludedSequences["o4"] = true;     // ㄟˋ (Hsu)
    HsuExcludedSequences["r"] = true;      // ㄐ (Hsu)
    HsuExcludedSequences["s"] = true;      // ㄋ (Hsu)
    HsuExcludedSequences["v"] = true;      // ㄒ (Hsu)
    HsuExcludedSequences["x"] = true;      // ㄌ (Hsu)
  }

  ~RoundTripFixture() {}

  map<string, bool> HsuExcludedSequences;
  map<string, bool> ETen26ExcludedSequences;
  char** standardSequences;
  size_t sequenceCount;
};

TEST_FIXTURE(RoundTripFixture, StandardLayoutRoundTrip) {
  for (size_t index = 0; index < sequenceCount; ++index) {
    string sequenceString = standardSequences[index];
    const BopomofoKeyboardLayout* layout =
        BopomofoKeyboardLayout::StandardLayout();

    BPMF syllable;
    syllable = layout->syllableFromKeySequence(sequenceString);

    string sequence;
    sequence = layout->keySequenceFromSyllable(syllable);

    string composedSyllable;
    composedSyllable = syllable.composedString();

    BPMF back;
    back = BPMF::FromComposedString(composedSyllable);

    CHECK_EQUAL(sequenceString, sequence);
    CHECK_EQUAL(syllable, back);
  }
}

TEST_FIXTURE(RoundTripFixture, ETenLayoutRoundTrip) {
  for (size_t index = 0; index < sequenceCount; ++index) {
    string sequenceString = standardSequences[index];
    const BopomofoKeyboardLayout* stdLayout =
        BopomofoKeyboardLayout::StandardLayout();
    BPMF syllable;
    syllable = stdLayout->syllableFromKeySequence(sequenceString);

    const BopomofoKeyboardLayout* layout = BopomofoKeyboardLayout::ETenLayout();

    string sequence;
    sequence = layout->keySequenceFromSyllable(syllable);

    BPMF newSyllable;
    newSyllable = layout->syllableFromKeySequence(sequence);

    CHECK_EQUAL(syllable, newSyllable);
  }
}

TEST_FIXTURE(RoundTripFixture, ETen26LayoutRoundTrip) {
  for (size_t index = 0; index < sequenceCount; ++index) {
    string sequenceString = standardSequences[index];

    if (ETen26ExcludedSequences[sequenceString]) continue;

    const BopomofoKeyboardLayout* stdLayout =
        BopomofoKeyboardLayout::StandardLayout();
    BPMF syllable;
    syllable = stdLayout->syllableFromKeySequence(sequenceString);

    const BopomofoKeyboardLayout* layout =
        BopomofoKeyboardLayout::ETen26Layout();

    string sequence;
    sequence = layout->keySequenceFromSyllable(syllable);

    BPMF newSyllable;
    newSyllable = layout->syllableFromKeySequence(sequence);

    CHECK_EQUAL(syllable, newSyllable);
  }
}

TEST_FIXTURE(RoundTripFixture, HsuLayoutRoundTrip) {
  for (size_t index = 0; index < sequenceCount; ++index) {
    string sequenceString = standardSequences[index];
    if (HsuExcludedSequences[sequenceString]) continue;

    const BopomofoKeyboardLayout* stdLayout =
        BopomofoKeyboardLayout::StandardLayout();
    BPMF syllable;
    syllable = stdLayout->syllableFromKeySequence(sequenceString);

    const BopomofoKeyboardLayout* layout = BopomofoKeyboardLayout::HsuLayout();

    string sequence;
    sequence = layout->keySequenceFromSyllable(syllable);

    BPMF newSyllable;
    newSyllable = layout->syllableFromKeySequence(sequence);

    CHECK_EQUAL(syllable, newSyllable);
  }
}

TEST_FIXTURE(RoundTripFixture, AbsoluteOrderRoundTrip) {
  for (size_t index = 0; index < sequenceCount; ++index) {
    string sequenceString = standardSequences[index];

    const BopomofoKeyboardLayout* stdLayout =
        BopomofoKeyboardLayout::StandardLayout();
    BPMF syllable;
    syllable = stdLayout->syllableFromKeySequence(sequenceString);
    cerr << syllable.composedString();

    short order = syllable.absoluteOrder();
    cerr << ", " << order;
    BPMF newSyllable = BPMF::FromAbsoluteOrder(order);

    CHECK_EQUAL(syllable, newSyllable);

    string orderString = syllable.absoluteOrderString();
    cerr << ", " << orderString << endl;
    newSyllable = BPMF::FromAbsoluteOrderString(orderString);
    CHECK_EQUAL(syllable, newSyllable);
  }
}
