.PHONY: all clean

-include dep.mk

OUT ?= out

dep.mk:
	@latexmk -silent -xelatex -dependents tikz.tex -MF dep.mk -outdir=${OUT}

clean:
	@latexmk -silent -C -outdir=${OUT}
	@rm dep.mk
	@rm -rf ${OUT}

out/main.pdf:
	latexmk -xelatex tikz.tex -outdir=out

all: clean out/main.pdf
