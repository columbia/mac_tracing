# the source .tex
SRC = paper
SRCTEX = $(SRC).tex
SRCBIB = bib/*.bib
# additional dependencies
ADD = 
# misc dependencies
MISC = Makefile $(wildcard *.cls)

-include Makefile.user

# commands
BIBTEX ?= bibtex
DVILATEX ?= latex
DVIPDF ?= dvipdfm
DVIPS ?= dvips
PDFLATEX ?= pdflatex
#PDF2PS ?= pdf2ps
PS2EPS ?= ps2eps
#GNUPLOT ?= gnuplot

RM = rm -f

WHICHPS = $(DVIPS)
WHICHPSOPTS = -P cmz -o $@ -t letter
WHICHLATEX = $(PDFLATEX)

# force a dependency on all data files.
DEP := $(wildcard *.tex)
FIGURES := $(wildcard figures/*.pdf)
CODE := $(wildcard code/*.tex)

PDFDEPEND = $(DEP) $(FIGURES) $(CODE)
DVIDEPEND = $(DEP) $(FIGURES:%.pdf=%.eps)

FORCE = .silly_file_to_force_rebuild

.PHONY: all check_tools clean cleanall force do_force pdf

#all: check_tools $(FORCE) pdf
all: check_tools $(FORCE) pdf

$(FORCE): check_tools
	@if [ ! -e $(FORCE) ]; then echo "" > $(FORCE); fi

check_tools: TOOLS = $(BIBTEX) $(DVILATEX) $(DVIPDF) $(PDF2PS) $(PS2EPS) $(WHICHPS) $(WHICHLATEX) $(GNUPLOT)
check_tools:
	@echo "Checking build tools..."
	$(foreach tool,$(TOOLS),$(if $(shell which $(tool)),,$(error You seem to be missing $(tool)!)))

do_force: check_tools
	@echo "Forcing re-build"
	@touch Makefile $(DEPEND) $(SRC).bib
	@date > $(FORCE)

force: do_force all
	@echo "Re build complete."

do_subdirs:
	make -C figures
	make -C code

pdf: do_subdirs $(SRC).pdf

$(SRC).pdf: $(FORCE) $(PDFDEPEND) $(SRC).bbl $(MISC)
	$(WHICHLATEX) -synctex=1 $(SRC)
	test -e p.pdf || ln -s paper.pdf p.pdf

$(SRC).dvi: $(FORCE) $(DVIDEPEND) $(SRC).bbl $(MISC)
	$(DVILATEX) $(SRC)

$(SRC).ps: $(FORCE) $(SRC).dvi $(SRC).bbl $(MISC)
	$(WHICHPS) $(WHICHPSOPTS) $(SRC)

$(SRC).bbl: $(FORCE) $(SRCBIB) $(MISC)
	@echo "[$(WHICHLATEX)]: first pass to generate *.aux files"
	@$(WHICHLATEX) $(SRC) | tee $(SRC).build.log
	@echo "[$(BIBTEX)] $(SRC)"
	@$(BIBTEX) $(SRC) | tee $(SRC).build.log
	@echo "[$(WHICHLATEX)]: second pass to generate *.bib files"
	@$(WHICHLATEX) $(SRC) | tee $(SRC).build.log
	@echo "[$(WHICHLATEX)]: third pass to get cross-referecne work"
	@$(WHICHLATEX) $(SRC) | tee $(SRC).build.log

pdf: $(SRC).pdf
dvi: $(SRC).dvi
ps: $(SRC).ps
bib: $(SRC).bbl

# clean up everything except .pdf
clean:
	$(RM) *.aux *.dvi *.log *.bbl *.blg *.build.log *~ *.out
	$(RM) $(SRC).pdfsync $(SRC).synctex.gz
	make -C figures clean
	make -C code clean

# clean up everything
cleanall: clean
	make -C figures cleanall
	make -C code cleanall
	$(RM) $(SRC).pdf $(SRC).ps p.pdf

.PHONY: pull push
pull:
	git pull && cd ../bib && git pull
push:
	git commit -am "edits" && git push \
	&& cd ../bib && git commit -am "more bib entries" && git push
