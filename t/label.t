#!/usr/bin/perl

# Tests for ngx_http_label_module.

###############################################################################

use warnings;
use strict;

use Test::More;

BEGIN { use FindBin; chdir($FindBin::Bin); }

use Test::Nginx qw/ :DEFAULT http_content /;

###############################################################################

select STDERR; $| = 1;
select STDOUT; $| = 1;

my $t = Test::Nginx->new()->has(qw/http ngx_http_label_module/)->plan(4);

$t->write_file_expand('nginx.conf', <<'EOF');

%%TEST_GLOBALS%%

daemon off;

events {
}

http {
    %%TEST_GLOBALS_HTTP%%

    label Environment production;
    label cluster_id edge-01;
    labels_hash_max_size 128;
    labels_hash_bucket_size 64;

    server {
        listen       127.0.0.1:8080;
        server_name  localhost;

        location /one {
            return 200 "$label_environment";
        }

        location /case {
            return 200 "$label_Environment";
        }

        location /all {
            return 200 "$labels";
        }

        location /missing {
            return 200 "<$label_unknown>";
        }
    }
}

EOF

$t->run();

###############################################################################

is(http_content(http_get('/one')), 'production', 'label variable');
is(http_content(http_get('/case')), 'production',
	'label variable name is case insensitive');
is(http_content(http_get('/all')),
	'environment=production&cluster_id=edge-01', 'all labels');
is(http_content(http_get('/missing')), '<>', 'unknown label');

###############################################################################
