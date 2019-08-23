#!/usr/bin/env perl
use v5.24.00;
use strict;
use warnings;
use Term::ProgressBar;
use IO::File;

package GnTest {
    use Storable;

    use constant {
        PARAM_SIZE => 1024,
        PARAM_F0 => 32,
        PARAM_TILE_SIZE => 32,
    };

    our $cache = (-f 'build/cache.dat' ? Storable::retrieve('build/cache.dat') : {});
    END { Storable::store($cache, 'build/cache.dat'); }

    sub new {
        my $class = shift;
        return bless {
            f0 => PARAM_F0,
            tile_size => PARAM_TILE_SIZE,
            _private => {
                's' => PARAM_SIZE,
            },
        }, $class;
    }

    sub _opt {
        my ($self, $opt, $value) = @_;
        if (defined $value) {
            $self->{_private}->{$opt} = $value;
            return $self;
        } else {
            return $self->{_private}->{$opt};
        }
    }

    sub samples {
        my ($self, $value) = @_;
        $self->_opt('n', $value)
    }

    sub lut {
        my ($self, $value) = @_;
        $self->_opt('-lut', $value)
    }

    sub output {
        my ($self, $value) = @_;
        $self->_opt('-output', $value)
    }

    sub run {
        my ($self) = shift;
        my @cmd = qw(build/gn_perf -Q -r);

        for my $k (sort keys %{$self}) {
            next if $k eq "_private";
            my $v = $self->{$k};
            my $pn = uc $k;
            push @cmd, "-D$pn=$v";
        }

        for my $k (sort keys %{$self->{_private}}) {
            my $v = $self->{_private}->{$k};
            my $kopt = $k =~ /^-/ ? "-$k " : "-$k";

            if (ref $v eq 'HASH') {
                for my $sk (sort keys %$v) {
                    my $sv = $v->{$sk};
                    push @cmd, "$kopt$sk" if $sv;
                }
            } else {
                push @cmd, "$kopt$v";
            }
        }

        my $cachekey = "@cmd";
        if (exists $cache->{$cachekey}) {
            #say STDERR "$cachekey found in cache";
            return $cache->{$cachekey} if $cache->{$cachekey}->[0] !~ m/nan/;
        }

        #say STDERR "@cmd";
        while (1) {
            my $out = `@cmd 2>&1`;
            my $ec = $? >> 8;
            my ($t_ms) = split /\n/, $out;

            if ($ec != 0 || !defined $t_ms) {
                say STDERR "@cmd failed with code $ec";
                return undef;
            }

            my ($name, $avg, $sdd, $sdp, $min, $max) = split /\t/, $t_ms;
            return $cache->{$cachekey} = [ $avg, $sdd, $min, $max ] if $avg !~ m/nan/;
        }
    }

    sub AUTOLOAD {
        my $method_missing = our $AUTOLOAD;
        $method_missing =~ s/.*:://;

        my ($self, $value) = @_;

        if ($method_missing =~ m/set_(.*)$/) {
            if (defined $value) {
                $self->{_private}->{D}->{uc $1} = $value;
                return $self;
            } else {
                return $self->{_private}->{D}->{uc $1};
            }
        } else {
            if (defined $value) {
                #say STDERR "set $method_missing: $value";
                $self->{$method_missing} = $value;
                return $self;
            } else {
                return $self->{$method_missing};
            }
        }
    }
}

#
##### Define outputs
#

my %outputs = (
    ptdist => IO::File->new("build/ptdist-perf.csv", "w"),
    wkern => IO::File->new("build/wkern-perf.csv", "w"),
    final => IO::File->new("build/final.csv", "w"),
    boot => IO::File->new("build/boot.csv", "w"),
);

#
##### Define computations
#

my @samples = ();

push @samples, {
    raw => qq{N\tGrid\t"Hex. grid"\tJittered\t"Hex. jittered"\tWhite\tStratified\n},
    dest => 'ptdist',
}, {
    raw => qq{N\tUniform\tBernoulli\t"Random phase"\n},
    dest => 'wkern',
}, {
    raw => qq{N\t"Uniform Poisson"\t"Bernoulli strat. Poisson"\n},
    dest => 'final',
}, {
    raw => qq{N\t"Uniform Poisson"\t"Bernoulli strat. Poisson"\t"Uniform Poisson (no LUT)"\t"Bernoulli strat. Poisson (no LUT)"\n},
    dest => 'boot',
};

for (my $i = 1; $i <= 30; ++$i) {
    for my $pointdist (qw/GRID HEX_GRID JITTERED HEX_JITTERED WHITE STRATIFIED/) {
        push @samples, {
            rowid => $i,
            test => GnTest->new
                          ->points("POINTS_$pointdist")
                          ->splats($i)
                          ->random_seed('iFrame')
                          ->samples(100)
                          ->weights('WEIGHTS_UNIFORM'),
            dest => 'ptdist'
        };
    }

    for my $weights (qw/UNIFORM BERNOULLI NONE/) {
        push @samples, {
            rowid => $i,
            test => GnTest->new
                          ->points("POINTS_WHITE")
                          ->splats($i)
                          ->random_seed('iFrame')
                          ->samples(100)
                          ->weights("WEIGHTS_$weights"),
            dest => 'wkern',
        };

        if ($weights eq 'NONE') {
            $samples[-1]->{test}->set_random_phase(1)->set_ksin(1);
        }
    }

    push @samples, {
        rowid => $i,
        test => GnTest->new->points("POINTS_WHITE")->splats($i)->random_seed('iFrame')->samples(500)->weights('WEIGHTS_UNIFORM'),
        dest => 'final',
    };

    push @samples, {
        rowid => $i,
        test => GnTest->new->points("POINTS_STRATIFIED")->splats($i)->random_seed('iFrame')->samples(500)->weights('WEIGHTS_BERNOULLI'),
        dest => 'final',
    };
}

for (my $i = 1; $i <= 90; ++$i) {
    push @samples, {
        rowid => $i,
        test => GnTest->new->points("POINTS_WHITE")->splats($i)->random_seed('iFrame')->samples(500)->weights('WEIGHTS_UNIFORM')->set_preset_boot(1)->lut('lut/boot.png')->output(sprintf 'build/boot-white-%02d', $i)->f0(64.)->tile_size(32)->random_seed(1267),
        dest => 'boot',
    };

    push @samples, {
        rowid => $i,
        test => GnTest->new->points("POINTS_STRATIFIED")->splats($i)->random_seed('iFrame')->samples(500)->weights('WEIGHTS_BERNOULLI')->set_preset_boot(1)->lut('lut/boot.png')->output(sprintf 'build/boot-strat-%02d', $i)->f0(64.)->tile_size(32)->random_seed(1267),
        dest => 'boot',
    };

    push @samples, {
        rowid => $i,
        test => GnTest->new->points("POINTS_WHITE")->splats($i)->random_seed('iFrame')->samples(500)->weights('WEIGHTS_UNIFORM')->set_preset_boot(1)->output(sprintf 'build/boot-nolut-white-%02d', $i)->f0(64.)->tile_size(32)->random_seed(1267),
        dest => 'boot',
    };

    push @samples, {
        rowid => $i,
        test => GnTest->new->points("POINTS_STRATIFIED")->splats($i)->random_seed('iFrame')->samples(500)->weights('WEIGHTS_BERNOULLI')->set_preset_boot(1)->output(sprintf 'build/boot-nolut-strat-%02d', $i)->f0(64.)->tile_size(32)->random_seed(1267),
        dest => 'boot',
    };
}

#
##### Run computations, output CSVs
#

sub print_sample {
    my ($sample, $row) = @_;
    my $fh = $outputs{$sample->{dest}};
    if (defined $row) {
        printf $fh "%s\t%s\n", $sample->{rowid}, join("\t", @$row);
    } else {
        print $fh $sample->{raw};
    }
}

my $progress = Term::ProgressBar->new({ name => "Measurements", count => scalar @samples, ETA => 'linear' });
my %rows = ();
my %last_samples = ();

my $nu = 0;
my $si = 0;
my $exit = 0;

$SIG{INT} = sub { $progress->message("Aborted!"); $exit = 1; };

for my $sample (@samples) {
    if (exists $sample->{raw}) {
        print_sample($sample);
    } else {
        my $row         = ($rows{$sample->{dest}} //= []);
        my $data        = $sample->{test}->run;
        my $last_sample = $last_samples{$sample->{dest}};

        if (defined $last_sample && $last_sample->{rowid} != $sample->{rowid}) {
            print_sample($last_sample, $row);
            ($row = $rows{$sample->{dest}} = []);
        }

        push @$row, $data->[0];
        $last_samples{$sample->{dest}} = $sample;
    }

    last if $exit;
    $nu = $progress->update($si) if $si++ >= $nu;
}

$progress->update($si) unless $exit;

while (my ($k, $v) = each %last_samples) {
    print_sample($v, $rows{$k});
}

#
##### Close outputs
#

while (my ($name, $io) = each %outputs) {
    say STDERR "Wrote $name";
    $io->close;
}
