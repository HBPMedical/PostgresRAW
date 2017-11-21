/* $PostgreSQL: pgsql/contrib/earthdistance/uninstall_earthdistance.sql,v 1.3 2007/11/13 04:24:27 momjian Exp $ */

-- Adjust this setting to control where the objects get dropped.
SET search_path = public;

DROP OPERATOR <@> (point, point);

DROP FUNCTION geo_distance (point, point);

DROP FUNCTION earth_box(earth, float8);

DROP FUNCTION earth_distance(earth, earth);

DROP FUNCTION longitude(earth);

DROP FUNCTION latitude(earth);

DROP FUNCTION ll_to_earth(float8, float8);

DROP FUNCTION gc_to_sec(float8);

DROP FUNCTION sec_to_gc(float8);

DROP DOMAIN earth;

DROP FUNCTION earth();
