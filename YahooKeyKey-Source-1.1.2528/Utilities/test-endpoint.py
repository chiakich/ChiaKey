#!/usr/bin/env python
# encoding: utf-8
#
#Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
#Copyrights licensed under the New BSD License. See the accompanying LICENSE
#file for terms.
#

import sys
import os
import urllib
import unittest

web_site = "http://tw.widget.yahoo.com/keykey/help/"
phrase_editor_help = "http://tw.widget.yahoo.com/keykey/help/phraseeditor.html"

VERSION_FOR_INSTALLER="2382"
beacon_start = "http://tw.rd.yahoo.com/referurl/keykey/client/installer/start/%(VERSION_FOR_INSTALLER)s%(OS)s/*http://tw.yimg.com/i/tw/download/ykkimg/blank.gif"

version_info = "http://tw.widget.yahoo.com/ykk/VersionInfo.php"
version_info_sig = "http://tw.widget.yahoo.com/ykk/VersionInfo.sig"
canned_message = "http://tw.widget.yahoo.com/ykk/CannedMessages.plist"
dict_gen = "http://tw.widget.yahoo.com/keykey/dict_gen_xmls.php?p=Test"

class TestAPI(unittest.TestCase):
	def doTestURL(self, url):
		try:
			f = urllib.urlopen(url)
			self.assertTrue(f, "")
			self.assertTrue(len(f.read()) > 0, "")
		except Exception as e:
			self.fail(str(e))
	def testAPI(self):
		self.doTestURL(web_site)
		self.doTestURL(phrase_editor_help)
		self.doTestURL(version_info)
		self.doTestURL(version_info_sig)
		self.doTestURL(canned_message)
		self.doTestURL(dict_gen)
		self.doTestURL(beacon_start % {"VERSION_FOR_INSTALLER": VERSION_FOR_INSTALLER, "OS": "M105"})
		self.doTestURL(beacon_start % {"VERSION_FOR_INSTALLER": VERSION_FOR_INSTALLER, "OS": "M104"})
		self.doTestURL(beacon_start % {"VERSION_FOR_INSTALLER": VERSION_FOR_INSTALLER, "OS": "W32"})
		self.doTestURL(beacon_start % {"VERSION_FOR_INSTALLER": VERSION_FOR_INSTALLER, "OS": "W64"})


def main():
	unittest.main()
	pass


if __name__ == '__main__':
	main()
