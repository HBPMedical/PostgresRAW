--
-- first, define the datatype.  Turn off echoing so that expected file
-- does not depend on contents of btree_gist.sql.
--
SET client_min_messages = warning;
\set ECHO none
\i btree_gist.sql
\set ECHO all
RESET client_min_messages;
