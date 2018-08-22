#!/usr/bin/env perl
use v5.24.00;
use strict;
use warnings;

use Cwd qw/abs_path/;
use FindBin;
use Template;

my $tt = Template->new({
        INCLUDE_PATH => abs_path($FindBin::Bin)
    });

my $i = 0;
for my $points (qw/WHITE STRATIFIED JITTERED HEX_JITTERED GRID HEX_GRID/) {
    for my $weights (qw/UNIFORM BERNOULLI NONE/) {
        for my $prng (qw/LCG XOROSHIRO HASH NONE/) {
            my $fn = sprintf("%03d_%s-%s-%s",
                $i++, map({ lc } $points, $weights, $prng));
            say $fn;

            open my $fh, '>', "$FindBin::Bin/$fn.t";
            $tt->process("test.tt", {
                    points => $points,
                    weights => $weights,
                    prng => $prng,
                    fn => $fn
                }, $fh) || die $tt->error(), "\n";
            close $fh;
            chmod 0755, "$FindBin::Bin/$fn.t";
        }
    }
}
