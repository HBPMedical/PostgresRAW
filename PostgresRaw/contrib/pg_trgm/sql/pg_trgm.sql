--
-- first, define the datatype.  Turn off echoing so that expected file
-- does not depend on contents of pg_tgrm.sql.
--
SET client_min_messages = warning;
\set ECHO none
\i pg_trgm.sql
\set ECHO all
RESET client_min_messages;

select show_trgm('');
select show_trgm('(*&^$@%@');
select show_trgm('a b c');
select show_trgm(' a b c ');
select show_trgm('aA bB cC');
select show_trgm(' aA bB cC ');
select show_trgm('a b C0*%^');

select similarity('wow','WOWa ');
select similarity('wow',' WOW ');

CREATE TABLE test_trgm(t text);

\copy test_trgm from 'data/trgm.data

select t,similarity(t,'qwertyu0988') as sml from test_trgm where t % 'qwertyu0988' order by sml desc, t;
select t,similarity(t,'gwertyu0988') as sml from test_trgm where t % 'gwertyu0988' order by sml desc, t;
select t,similarity(t,'gwertyu1988') as sml from test_trgm where t % 'gwertyu1988' order by sml desc, t;

create index trgm_idx on test_trgm using gist (t gist_trgm_ops);
set enable_seqscan=off;

select t,similarity(t,'qwertyu0988') as sml from test_trgm where t % 'qwertyu0988' order by sml desc, t;
select t,similarity(t,'gwertyu0988') as sml from test_trgm where t % 'gwertyu0988' order by sml desc, t;
select t,similarity(t,'gwertyu1988') as sml from test_trgm where t % 'gwertyu1988' order by sml desc, t;

drop index trgm_idx;
create index trgm_idx on test_trgm using gin (t gin_trgm_ops);
set enable_seqscan=off;

select t,similarity(t,'qwertyu0988') as sml from test_trgm where t % 'qwertyu0988' order by sml desc, t;
select t,similarity(t,'gwertyu0988') as sml from test_trgm where t % 'gwertyu0988' order by sml desc, t;
select t,similarity(t,'gwertyu1988') as sml from test_trgm where t % 'gwertyu1988' order by sml desc, t;

