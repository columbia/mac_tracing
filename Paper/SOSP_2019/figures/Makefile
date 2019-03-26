GNUPLOT := `which gnuplot`
EPS2PDF := `which epstopdf`

GPTFILES = $(wildcard *.gp)
EPSFILES = $(GPTFILES:%.gp=%.eps) $(wildcard *.eps)
PDFFILES = $(EPSFILES:%.eps=%.pdf)

all: $(PDFFILES) Makefile

%.pdf: %.eps
	$(EPS2PDF) $*.eps

%.eps: %.gp %.dat
	$(GNUPLOT) $*.gp > $*.eps

clean:
	@rm -f $(GPTFILES:%.gp=%.eps)

cleanall:
	@rm -f $(PDFFILES)


