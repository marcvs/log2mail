#!/usr/bin/perl -w

for ($i=1;$i<10000;$i++) {
    open(MYF, ">>log2");
    if ($i % 2) {
	print MYF "123\n";
    } else {
	print MYF "6abz\n";
    }
    close MYF;
    sleep 1;
}
