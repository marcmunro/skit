-- This must return exactly one value which is a string representation
-- of the postgres catalog version.
select substring(pg_catalog.version() from E'\([0-9]+\.[0-9]+\(\.[0-9]+\)?\)') 
	as version;
