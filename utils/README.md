## set trace codes

cp Beachball/System/TraceCodes/trace.codes.elcapitan.mod /usr/share/misc/trace.codes

## raw parsing
input:
	XXX.trace_procs_pre
	XXX.trace_procs_post
	XXX.trace

./raw\_parse XXXX.trace XXX.log

## analysis

input:
  XXX.log
  libinfo.log copy from file in directory tmp

./analyze input/XXX.log App\_name live\_app\_in\_cur\_system
