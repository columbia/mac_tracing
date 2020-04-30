#!/usr/bin/python
#pyton version is 2.7.10
import re
import sys
import getopt
import os
from sets import Set

def combine(infile, pofile, outfile):

	with open(infile, "r") as f_read:
		lines = f_read.readlines()
		last_line = lines[-1]
		f_read.close()
	fields = re.split("[\s]+", last_line);
	last_time = float(fields[0])
	
	with open(pofile, "r") as f_read2:
		data = f_read2.read()
		f_read2.close()

	with open(outfile, "w") as f_write:
		for line in lines:
			fields = re.split("[\s]+", line);
			time = float(fields[0]) - last_time - 1;
			timestamp ="{:.2f}".format(time)
			newline = line.replace(fields[0], timestamp)
			f_write.write(newline)
		f_write.write(data)

	f_write.close()

class Usage(Exception):
	"""
	-i : input file
	-j : input file
	-o : output file
	-h : print help info
	"""
	def __init__(self, msg):
		self.msg  = msg

def main(argv = None):
	if argv is None:
		argv = sys.argv
	try:
		try:
			opts, args = getopt.getopt(argv[1:], "i:j:o:h", ["help"])
		except getopt.error, msg:
			raise Usage(msg)
	except Usage, err:
		print  sys.stderr, err.msg
		print  sys.stderr, "for help -h"
		return 2

	infile = None
	outfile = None
	pofile = None

	for opt, val in opts:
		if opt in ("-h", "--help"):
			print Usage("").__doc__
			return 1
		if opt == "-i":
			infile = val
		if opt == "-j" :
			pofile = val
		if opt == "-o":
			outfile = val
	combine(infile, pofile, outfile)

if __name__ == "__main__":
	sys.exit(main())
