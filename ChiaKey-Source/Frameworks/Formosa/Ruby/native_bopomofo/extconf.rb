require 'mkmf'
require 'yaml'

# replace TRY_LINK's CC into CXX because we want to use g++ as linker
CONFIG['LDSHARED'].gsub!(/cc/, "g++")
CONFIG['LDSHARED'].gsub!(/\(CC\)/, "(CXX)")
CONFIG['CXXFLAGS'] = "-DMANDARIN_USE_MINIMAL_OPENVANILLA -I../../Headers -I../../../OpenVanilla/Headers"


create_makefile "native_bopomofo/native_bopomofo"
makefile = IO.read("Makefile")
makefile.gsub!(/OBJS = /, "OBJS = Mandarin.o ")

makefile += "\nMandarin.o: ../../Source/Mandarin.cpp\n\t$(CXX) $(INCFLAGS) $(CPPFLAGS) $(CXXFLAGS) -c $<"

f = File.open("Makefile", "w")
f.puts makefile
f.close
