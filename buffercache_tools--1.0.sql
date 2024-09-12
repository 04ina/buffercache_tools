/* buffercache_tools--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION buffercache_tools" to load this file. \quit

--
-- pg_show_buffer()
--
CREATE FUNCTION pg_show_buffer(IN buffer integer,
    OUT blocknum bigint,
    OUT fork text,
    OUT relnumber Oid,
    OUT dboid Oid,
    OUT spcoid Oid,
    OUT dirty bool,
    OUT usagecount smallint,
    OUT pinning integer)
RETURNS SETOF RECORD
AS 'MODULE_PATHNAME', 'pg_show_buffer'
LANGUAGE C STRICT;

--
-- pg_show_relation_buffers()
--
CREATE FUNCTION pg_show_relation_buffers(IN relname text,
    OUT buffernum integer,
    OUT blocknum bigint,
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
    IN BluckNumber bigint) 
RETURNS integer 
AS 'MODULE_PATHNAME', 'pg_read_page_into_buffer'
LANGUAGE C STRICT;

--
-- pg_change_buffer()
--
CREATE FUNCTION pg_change_buffer(
    IN buf_proc_func text,
    IN buffer integer)
RETURNS bool 
AS 'MODULE_PATHNAME', 'pg_change_buffer'
LANGUAGE C STRICT;

CREATE FUNCTION pg_change_buffer(
    IN buf_proc_func text,
    IN buffer integer,
    IN int_value oid)
RETURNS bool 
AS 'MODULE_PATHNAME', 'pg_change_buffer'
LANGUAGE C STRICT;

CREATE FUNCTION pg_change_buffer(
    IN buf_proc_func text,
    IN buffer integer,
    IN int_value bigint)
RETURNS bool 
AS 'MODULE_PATHNAME', 'pg_change_buffer'
LANGUAGE C STRICT;

CREATE FUNCTION pg_change_buffer(
    IN buf_proc_func text,
    IN buffer integer,
    IN text_value text)
RETURNS bool 
AS $$
    SELECT pg_change_buffer($1, $2,
        CASE $3 
            WHEN 'main' THEN 0
            WHEN 'fsm' THEN 1 
            WHEN 'vm' THEN 2 
            WHEN 'init' THEN 3 
        END
    );
$$ LANGUAGE SQL;

--
-- pg_change_relation_fork_buffers()
--
CREATE FUNCTION pg_change_relation_fork_buffers(
    IN buf_proc_func text,
    IN relname text, 
    IN fork text)
RETURNS bool 
AS 'MODULE_PATHNAME', 'pg_change_relation_fork_buffers'
LANGUAGE C STRICT;

CREATE FUNCTION pg_change_relation_fork_buffers(
    IN buf_proc_func text,
    IN relname text, 
    IN fork text,
    IN int_value Oid)
RETURNS bool 
AS 'MODULE_PATHNAME', 'pg_change_relation_fork_buffers'
LANGUAGE C STRICT;

CREATE FUNCTION pg_change_relation_fork_buffers(
    IN buf_proc_func text,
    IN relname text, 
    IN fork text,
    IN int_value bigint)
RETURNS bool 
AS 'MODULE_PATHNAME', 'pg_change_relation_fork_buffers'
LANGUAGE C STRICT;

CREATE FUNCTION pg_change_relation_fork_buffers(
    IN buf_proc_func text,
    IN relname text, 
    IN fork text,
    IN text_value text)
RETURNS bool 
AS $$
    SELECT pg_change_relation_fork_buffers($1, $2, $3,
        CASE $4 
            WHEN 'main' THEN 0
            WHEN 'fsm' THEN 1 
            WHEN 'vm' THEN 2 
            WHEN 'init' THEN 3 
        END
    );
$$ LANGUAGE SQL;

--
-- pg_change_relation_buffers()
--
CREATE FUNCTION pg_change_relation_buffers(
    IN buf_proc_func text,
    IN relname text)
RETURNS bool 
AS 'MODULE_PATHNAME', 'pg_change_relation_buffers'
LANGUAGE C STRICT;

CREATE FUNCTION pg_change_relation_buffers(
    IN buf_proc_func text,
    IN relname text, 
    IN int_value Oid)
RETURNS bool 
AS 'MODULE_PATHNAME', 'pg_change_relation_buffers'
LANGUAGE C STRICT;

CREATE FUNCTION pg_change_relation_buffers(
    IN buf_proc_func text,
    IN relname text, 
    IN int_value bigint)
RETURNS bool 
AS 'MODULE_PATHNAME', 'pg_change_relation_buffers'
LANGUAGE C STRICT;

CREATE FUNCTION pg_change_relation_buffers(
    IN buf_proc_func text,
    IN relname text, 
    IN text_value text)
RETURNS bool 
AS $$
    SELECT pg_change_relation_buffers($1, $2,
        CASE $3 
            WHEN 'main' THEN 0
            WHEN 'fsm' THEN 1 
            WHEN 'vm' THEN 2 
            WHEN 'init' THEN 3 
        END
    );
$$ LANGUAGE SQL;

--
-- pg_change_database_buffers()
--
CREATE FUNCTION pg_change_database_buffers(
    IN buf_proc_func text,
    IN dboid Oid)
RETURNS bool 
AS 'MODULE_PATHNAME', 'pg_change_database_buffers'
LANGUAGE C STRICT;

CREATE FUNCTION pg_change_database_buffers(
    IN buf_proc_func text,
    IN dboid Oid, 
    IN int_value Oid)
RETURNS bool 
AS 'MODULE_PATHNAME', 'pg_change_database_buffers'
LANGUAGE C STRICT;

CREATE FUNCTION pg_change_database_buffers(
    IN buf_proc_func text,
    IN dboid Oid, 
    IN int_value bigint)
RETURNS bool 
AS 'MODULE_PATHNAME', 'pg_change_database_buffers'
LANGUAGE C STRICT;

CREATE FUNCTION pg_change_database_buffers(
    IN buf_proc_func text,
    IN dboid Oid, 
    IN text_value text)
RETURNS bool 
AS $$
    SELECT pg_change_database_buffers($1, $2,
        CASE $3 
            WHEN 'main' THEN 0
            WHEN 'fsm' THEN 1 
            WHEN 'vm' THEN 2 
            WHEN 'init' THEN 3 
        END
    );
$$ LANGUAGE SQL;

--
-- pg_change_tablespace_buffers()
--
CREATE FUNCTION pg_change_tablespace_buffers(
    IN buf_proc_func text,
    IN spcoid Oid)
RETURNS bool 
AS 'MODULE_PATHNAME', 'pg_change_tablespace_buffers'
LANGUAGE C STRICT;

CREATE FUNCTION pg_change_tablespace_buffers(
    IN buf_proc_func text,
    IN spcoid Oid, 
    IN int_value Oid)
RETURNS bool 
AS 'MODULE_PATHNAME', 'pg_change_tablespace_buffers'
LANGUAGE C STRICT;

CREATE FUNCTION pg_change_tablespace_buffers(
    IN buf_proc_func text,
    IN spcoid Oid, 
    IN int_value bigint)
RETURNS bool 
AS 'MODULE_PATHNAME', 'pg_change_tablespace_buffers'
LANGUAGE C STRICT;

CREATE FUNCTION pg_change_tablespace_buffers(
    IN buf_proc_func text,
    IN spcoid Oid, 
    IN text_value text)
RETURNS bool 
AS $$
    SELECT pg_change_tablespace_buffers($1, $2,
        CASE $3 
            WHEN 'main' THEN 0
            WHEN 'fsm' THEN 1 
            WHEN 'vm' THEN 2 
            WHEN 'init' THEN 3 
        END
    );
$$ LANGUAGE SQL;

--
-- pg_change_all_valid_buffers()
--
CREATE FUNCTION pg_change_all_valid_buffers(
    IN buf_proc_func text)
RETURNS bool 
AS 'MODULE_PATHNAME', 'pg_change_all_valid_buffers'
LANGUAGE C STRICT;

CREATE FUNCTION pg_change_all_valid_buffers(
    IN buf_proc_func text,
    IN int_value Oid)
RETURNS bool 
AS 'MODULE_PATHNAME', 'pg_change_all_valid_buffers'
LANGUAGE C STRICT;

CREATE FUNCTION pg_change_all_valid_buffers(
    IN buf_proc_func text,
    IN int_value bigint)
RETURNS bool 
AS 'MODULE_PATHNAME', 'pg_change_all_valid_buffers'
LANGUAGE C STRICT;

CREATE FUNCTION pg_change_all_valid_buffers(
    IN buf_proc_func text,
    IN text_value text)
RETURNS bool 
AS $$
    SELECT pg_change_all_valid_buffers($1,
        CASE $2 
            WHEN 'main' THEN 0
            WHEN 'fsm' THEN 1 
            WHEN 'vm' THEN 2 
            WHEN 'init' THEN 3 
        END
    );
$$ LANGUAGE SQL;