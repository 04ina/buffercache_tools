/* buffercache_tools--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION buffercache_tools" to load this file. \quit

--
-- pg_mark_dirty_buf()
--
CREATE FUNCTION pg_mark_dirty_buf(
    IN buffer integer)
RETURNS bool 
AS 'MODULE_PATHNAME', 'pg_mark_dirty_buf'
LANGUAGE C STRICT;

--
-- pg_flush_buf()
--
CREATE FUNCTION pg_flush_buf(
    IN buffer integer)
RETURNS bool 
AS 'MODULE_PATHNAME', 'pg_flush_buf'
LANGUAGE C STRICT;

--
-- pg_mark_dirty_relation_fork_buf()
--
CREATE FUNCTION pg_mark_dirty_relation_fork_buf(
    IN relname text, 
    IN fork text)
RETURNS bool 
AS 'MODULE_PATHNAME', 'pg_mark_dirty_relation_fork_buf'
LANGUAGE C STRICT;

--
-- pg_flush_relation_fork_buf()
--
CREATE FUNCTION pg_flush_relation_fork_buf(
    IN relname text, 
    IN fork text)
RETURNS bool 
AS 'MODULE_PATHNAME', 'pg_flush_relation_fork_buf'
LANGUAGE C STRICT;

--
-- pg_mark_dirty_relation_buf()
--
CREATE FUNCTION pg_mark_dirty_relation_buf(
    IN relname text)
RETURNS bool 
AS 'MODULE_PATHNAME', 'pg_mark_dirty_relation_buf'
LANGUAGE C STRICT;

--
-- pg_flush_relation_buf()
--
CREATE FUNCTION pg_flush_relation_buf(
    IN relname text)
RETURNS bool 
AS 'MODULE_PATHNAME', 'pg_flush_relation_buf'
LANGUAGE C STRICT;


--
-- pg_mark_dirty_database_buf()
--
CREATE FUNCTION pg_mark_dirty_database_buf(
    IN dboid oid)
RETURNS bool
AS 'MODULE_PATHNAME', 'pg_mark_dirty_database_buf'
LANGUAGE C STRICT;

--
-- pg_flush_database_buf()
--
CREATE FUNCTION pg_flush_database_buf(
    IN dboid oid)
RETURNS bool
AS 'MODULE_PATHNAME', 'pg_flush_database_buf'
LANGUAGE C STRICT;

--
-- pg_mark_dirty_tablespace_buffers()
--
CREATE FUNCTION pg_mark_dirty_tablespace_buffers(
    IN dboid oid)
RETURNS bool
AS 'MODULE_PATHNAME', 'pg_mark_dirty_tablespace_buffers'
LANGUAGE C STRICT;

--
-- pg_flush_tablespace_buf()
--
CREATE FUNCTION pg_flush_tablespace_buf(
    IN spcoid oid)
RETURNS bool
AS 'MODULE_PATHNAME', 'pg_flush_tablespace_buf'
LANGUAGE C STRICT;

--
-- pg_show_relation_buf()
--
CREATE FUNCTION pg_show_relation_buf(IN relname text,
    OUT blocknumber integer,
    OUT buffernumber integer,
    OUT dirty bool,
    OUT usagecount smallint,
    OUT pinning integer,
    OUT fork text)
RETURNS SETOF RECORD
AS 'MODULE_PATHNAME', 'pg_show_relation_buf'
LANGUAGE C STRICT;

--
-- pg_read_page_into_buf()
--
CREATE FUNCTION pg_read_page_into_buf(
    IN relname text, 
    IN fork text, 
    IN BluckNumber integer) 
RETURNS integer 
AS 'MODULE_PATHNAME', 'pg_read_page_into_buffer'
LANGUAGE C STRICT;
