/* buffercache_tools--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION buffercache_tools" to load this file. \quit

--
-- pg_show_relation_buffers()
--
CREATE FUNCTION pg_show_relation_buffers(IN relname text,
    OUT blocknumber integer,
    OUT buffernumber integer,
    OUT dirty bool,
    OUT usagecount smallint,
    OUT pinning integer,
    OUT fork text)
RETURNS SETOF RECORD
AS 'MODULE_PATHNAME', 'pg_show_relation_buffers'
LANGUAGE C STRICT;

--
-- pg_read_page_into_buffer()
--
CREATE FUNCTION pg_read_page_into_buffer(
    IN relname text, 
    IN fork text, 
    IN BluckNumber integer) 
RETURNS integer 
AS 'MODULE_PATHNAME', 'pg_read_page_into_buffer'
LANGUAGE C STRICT;

--
-- pg_flush_buffer()
--
CREATE FUNCTION pg_flush_buffer(
    IN buffer integer)
RETURNS bool 
AS 'MODULE_PATHNAME', 'pg_flush_buffer'
LANGUAGE C STRICT;

--
-- pg_mark_buffer_dirty()
--
CREATE FUNCTION pg_mark_buffer_dirty(
    IN buffer integer)
RETURNS bool 
AS 'MODULE_PATHNAME', 'pg_mark_buffer_dirty'
LANGUAGE C STRICT;

--
-- pg_flush_relation_fork_buffers()
--
CREATE FUNCTION pg_flush_relation_fork_buffers(
    IN relname text, 
    IN fork text)
RETURNS bool 
AS 'MODULE_PATHNAME', 'pg_flush_relation_fork_buffers'
LANGUAGE C STRICT;

--
-- pg_flush_relation_buffers()
--
CREATE FUNCTION pg_flush_relation_buffers(
    IN relname text)
RETURNS bool 
AS 'MODULE_PATHNAME', 'pg_flush_relation_buffers'
LANGUAGE C STRICT;

--
-- pg_flush_database_buffers()
--
CREATE FUNCTION pg_flush_database_buffers(
    IN dboid oid)
RETURNS bool
AS 'MODULE_PATHNAME', 'pg_flush_database_buffers'
LANGUAGE C STRICT;