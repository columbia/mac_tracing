.PHONY: all clean

-include dep.mk

OUT ?= out

dep.mk:
	@latexmk -silent -xelatex -dependents eurosys18.tex -MF dep.mk -outdir=${OUT}

clean:
	@latexmk -silent -C -outdir=${OUT}
	@rm dep.mk
	@rm -rf ${OUT}

out/main.pdf:
	latexmk -xelatex eurosys18.tex -outdir=out

all: out/main.pdf
