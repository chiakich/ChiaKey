#
#Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
#Copyrights licensed under the New BSD License. See the accompanying LICENSE
#file for terms.
#

TMP=./Intermediate

PRIVATE_KEY = $(TMP)/private.pem
PUBLIC_KEY = $(TMP)/public.pem

# 20 MB
RANDOM_SIZE = 20971520
RANDOM_FILE = $(TMP)/random.bin
RANDOM_SIG = $(TMP)/random.sig

SYMMETRIC_KEY_PHRASE = "helloworld"
SYMMETRIC_KEY_PHRASE_FILE = $(TMP)/symmetric.txt
SYMMETRIC_KEY_PHRASE_ENCODED_FILE = $(TMP)/symmetric.bin

all: $(TMP) $(PRIVATE_KEY) $(PUBLIC_KEY) $(RANDOM_SIG) $(SYMMETRIC_KEY_PHRASE_ENCODED_FILE) PublicDecrypt ValidateFile

$(TMP): 
	mkdir -p $@

$(PRIVATE_KEY):
	openssl genrsa -out $@ 1024
	
$(PUBLIC_KEY): $(PRIVATE_KEY)
	openssl rsa -in $< -pubout -out $@
	
$(RANDOM_FILE):
	openssl rand -out $@ $(RANDOM_SIZE)
	
$(RANDOM_SIG): $(RANDOM_FILE)
	ruby ../tools/digestsign.rb $< $(PRIVATE_KEY) > $@

$(SYMMETRIC_KEY_PHRASE_ENCODED_FILE): $(SYMMETRIC_KEY_PHRASE_FILE)
	ruby ../tools/plainsign.rb $< $(PRIVATE_KEY) > $@
	
$(SYMMETRIC_KEY_PHRASE_FILE):
	/bin/echo -n $(SYMMETRIC_KEY_PHRASE) > $@

PublicDecrypt: PublicDecrypt.cpp
	g++ -I../Headers -F../../../build/Debug -o $@ $< ../Source/Minotaur.cpp -lcrypto -lOpenVanilla -L../../../build/Debug 

ValidateFile: ValidateFile.cpp
	g++ -I../Headers -F../../../build/Debug -o $@ $< ../Source/Minotaur.cpp -lcrypto -lOpenVanilla -L../../../build/Debug 

clean:
	rm -rf $(TMP) PublicDecrypt ValidateFile
