eval 'exec perl -w -S $0 ${1+"$@"}'
    if 0;
$num = `wc -l $ARGV[0] | cut -f 1 -d \' \'`;
#warn "total lines: $num\n";

while(<>) {
	$cnt++;
	if($cnt < 10 && $num >= 10) {
		$space = " ";
	} else {
		$space = "";
	}

        if(/\/\*%%.*%%\*\/NOLINE/ || /\/\/NOLINE/) {
            $cnt--;
            s/NOLINE//;
            print "   $_";
        } else {
            print "$cnt$space: $_";
        }
}
