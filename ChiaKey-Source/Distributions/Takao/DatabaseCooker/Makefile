KEYPHRASE = $(shell cat ../Keyring/DevVendorDBKeyPhrase.txt)
# KEYPARAM = -key '$(KEYPHRASE)'
KEYPARAM =
# SQLITE3 = ../PrecompiledTools/sqlite3-cerod-see $(KEYPARAM)
SQLITE3 = /opt/local/bin/sqlite3

INSERTER = ./insert-file-into-db.rb
PREPOPULATE_TABLE_NAME = prepopulated_service_data
CMSG_SERVICE_KEY = canned_messages
CMSG_DATA_SOURCE = ../OnlineData/CannedMessages.plist

# DB = ../CookedDatabase/Yahoo!\ KeyKey.db
DB = ../CookedDatabase/ChiaKeySource.db

TMPDIR = ./Intermediates

CONVERT_TOOL = ../../../Frameworks/Formosa/Tools/convert-bpmf-cin.rb
NATIVE_BOPOMOFO_DIR = ../../../Frameworks/Formosa/Ruby/native_bopomofo
NATIVE_BOPOMOFO_BUNDLE = $(NATIVE_BOPOMOFO_DIR)/native_bopomofo.bundle
NATIVE_BOPOMOFO_CPP = $(NATIVE_BOPOMOFO_DIR)/native_bopomofo.cpp
NATIVE_BOPOMOFO_EXTCONF = $(NATIVE_BOPOMOFO_DIR)/extconf.rb
NATIVE_BOPOMOFO_MAKEFILE = $(NATIVE_BOPOMOFO_DIR)/Makefile

SOURCEDIR = ../../../DataTables
SRC = $(TMPDIR)/bpmf-ext-absorder.cin \
	$(SOURCEDIR)/bpmf-punctuations.cin \
	$(SOURCEDIR)/cj-ext.cin \
	$(SOURCEDIR)/simplex-ext.cin \
	$(SOURCEDIR)/cj-punctuations-halfwidth.cin \
	$(SOURCEDIR)/cj-punctuations-mixedwidth.cin \
	$(SOURCEDIR)/bopomofo-correction.cin
	
TOOL = ../../../Frameworks/OpenVanilla/Tools/cin-to-sqlite.rb

# creates the db file
all: createdir $(DB)

createdir:
	mkdir -p $(TMPDIR)

$(DB): $(SRC)
	rm -f $(DB)
	cat Schema.sql | $(SQLITE3) $(DB)
	$(TOOL) $(TMPDIR)/bpmf-ext-absorder.cin Mandarin-bpmf-cin | $(SQLITE3) $(DB)
	$(TOOL) $(SOURCEDIR)/bpmf-punctuations.cin Mandarin-bpmf-cin --chardef-only | $(SQLITE3) $(DB)
	$(TOOL) $(SOURCEDIR)/cj-ext.cin Generic-cj-cin | $(SQLITE3) $(DB)
	$(TOOL) $(SOURCEDIR)/simplex-ext.cin Generic-simplex-cin | $(SQLITE3) $(DB)
	$(TOOL) $(SOURCEDIR)/cj-punctuations-halfwidth.cin Punctuations-cj-halfwidth-cin | $(SQLITE3) $(DB)
	$(TOOL) $(SOURCEDIR)/cj-punctuations-mixedwidth.cin Punctuations-cj-mixedwidth-cin | $(SQLITE3) $(DB)
	$(TOOL) $(SOURCEDIR)/bopomofo-correction.cin BopomofoCorrection-bopomofo-correction-cin | $(SQLITE3) $(DB)
	$(INSERTER) $(DB) $(PREPOPULATE_TABLE_NAME) $(CMSG_SERVICE_KEY) $(CMSG_DATA_SOURCE)	

$(TMPDIR)/bpmf-ext-absorder.cin: createdir $(SOURCEDIR)/bpmf-ext.cin $(CONVERT_TOOL) $(NATIVE_BOPOMOFO_BUNDLE)
	ruby -I $(NATIVE_BOPOMOFO_DIR) $(CONVERT_TOOL) < $(SOURCEDIR)/bpmf-ext.cin > $(TMPDIR)/bpmf-ext-absorder.cin
	
$(NATIVE_BOPOMOFO_BUNDLE): $(NATIVE_BOPOMOFO_MAKEFILE) $(NATIVE_BOPOMOFO_CPP)
	make -C $(NATIVE_BOPOMOFO_DIR)
	
$(NATIVE_BOPOMOFO_MAKEFILE): $(NATIVE_BOPOMOFO_EXTCONF)
	ruby -C $(NATIVE_BOPOMOFO_DIR) extconf.rb

clean:
	rm $(DB)
	rm -rf $(TMPDIR)
