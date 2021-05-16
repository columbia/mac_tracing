MISC = add-lineno.pl lgrindef Makefile

# enable comments in /* */ in C++ using our own lgrindef
LGRIND = lgrind -d ./lgrindef

CODE := $(wildcard *.c *.cpp *.C *.csh)

CODE_LINENO = $(CODE:=.lineno)
CODE_TEX = $(CODE:=.tex)
CODE_LINENO_TEX = $(CODE:=.lineno.tex)


#all: $(CODE_TEX) $(CODE_LINENO_TEX) $(MISC)

%.c.tex: %.c ./lgrindef
	$(LGRIND) -i -lc $< > $@

%.c.lineno.tex: %.c ./lgrindef
	./add-lineno.pl $< > $(<:.c=.c.lineno)
	$(LGRIND) -i -lc $(<:.c=.c.lineno) > $@

%.csh.tex: %.csh ./lgrindef
	$(LGRIND) -i -lcsh $< > $@

%.csh.lineno.tex: %.csh ./lgrindef
	./add-lineno.pl $< > $(<:.csh=.csh.lineno)
	$(LGRIND) -i -lcsh $< > $@

%.cpp.tex: %.cpp ./lgrindef
	$(LGRIND) -i -lcc $< > $@

%.cpp.lineno.tex: %.cpp ./lgrindef
	./add-lineno.pl $< > $(<:.cpp=.cpp.lineno)
	$(LGRIND) -i -lcc $(<:.cpp=.cpp.lineno) > $@

%.C.tex: %.C ./lgrindef
	$(LGRIND) -i -lcc $< > $@

%.C.lineno.tex: %.C ./lgrindef
	./add-lineno.pl $< > $(<:.C=.C.lineno)
	$(LGRIND) -i -lcc $(<:.C=.C.lineno) > $@

clean:
	@rm -f $(CODE_TEX) $(CODE_LINENO) $(CODE_LINENO_TEX)

cleanall:
