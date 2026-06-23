#!/usr/bin/env python
# -*- coding: utf-8 -*-
import sys
    
pinyin = {}
while True:
    line = sys.stdin.readline()
    if not line:
        break    
        
    x = line.strip().split(" ")
    if len(x) > 1:
        if not pinyin.has_key(x[0]):
            pinyin[x[0]] = True
            print("%s" % x[0])