Takao Distribution Build Note
=============================

1.  Build PhraseTool (from Xcode project)

    Alternatively, use the files contained in PercompiledTools.zip

2.  Build sqlite-cerod-see and cerod_build, then copy them to:

    ./Distributions/Takao/PrecompiledTools/
    
    Alternatively, use the files contained in PercompiledTools.zip
    
3.  Install Ruby uuidtools

    sudo gem install uuidtools
    
    On 10.5, alternatively:
    
    curl -LO http://rubyforge.org/frs/download.php/27632/uuidtools-1.0.2.gem
    sudo gem install uuidtools-1.0.2.gem
    
    On 10.5, make sure you remove the "UUIDTools::" namespace in generate-noise.rb

4.  Copy Sinica Corpus *.xml to 

    ./Distributions/Takao/DataSource/SinicaCorpus/
    
5.  Copy Yahoo! search terms data to

    ./Distribution/Takao/DataSource/YahooSearchTerms/

6.  Copy tsi.src (for BPMF marking) to

    ./Distribution/Takao/DataSource/TsiSrc/
    
7.  Run ./Utilities/reform-auto-complete.pl:

    cat <search_term.tsv> | ./Utilities/reform-auto-complete.pl  > current
    
8.  Copy KeyRings to:

    ./Distribution/Takao/Keyrings
    
9.  Go to ./Distribution/Takao/DatabaseCooker, run:

    make ; make -f Makefile.SmartMandarin
    
