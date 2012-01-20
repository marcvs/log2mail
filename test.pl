#!/usr/bin/perl -w

for ($i=1;$i<10000;$i++) {
    open(MYF, ">>logf");
    print MYF "hlo\n";
    close MYF;
    sleep 1;
}
