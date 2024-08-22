--
-- Preparing
--

SET allow_in_place_tablespaces = true;
CREATE TABLESPACE test_tablespace LOCATION '';
RESET allow_in_place_tablespaces;

CREATE DATABASE test_database_1;
CREATE DATABASE test_database_2;

\c test_database_1 \\
CREATE EXTENSION buffercache_tools;

CREATE TABLE test_table_1(col integer) 
    WITH (autovacuum_enabled = off)
    TABLESPACE pg_default;
INSERT INTO test_table_1 
    SELECT 1 FROM generate_series(1,1000); 

CREATE VIEW tt_1 AS 
    SELECT blocknumber, dirty, pinning, fork 
        FROM pg_show_relation_buffers('test_table_1') 
        ORDER BY fork, blocknumber; 

CREATE TABLE test_table_2(col integer) 
    WITH (autovacuum_enabled = off)
    TABLESPACE test_tablespace;
INSERT INTO test_table_2 
    SELECT 1 FROM generate_series(1,1000); 

CREATE VIEW tt_2 AS 
    SELECT blocknumber, dirty, pinning, fork 
        FROM pg_show_relation_buffers('test_table_2') 
        ORDER BY fork, blocknumber; 

\c test_database_2 \\
CREATE EXTENSION buffercache_tools;

CREATE TABLE test_table_3(col integer) 
    WITH (autovacuum_enabled = off)
    TABLESPACE pg_default;
INSERT INTO test_table_3 
    SELECT 1 FROM generate_series(1,1000); 

CREATE VIEW tt_3 AS 
    SELECT blocknumber, dirty, pinning, fork 
        FROM pg_show_relation_buffers('test_table_3') 
        ORDER BY fork, blocknumber; 
VACUUM;
CHECKPOINT;

\c test_database_1 \\

VACUUM;
CHECKPOINT;

-- 
-- Check pg_mark_buffer_dirty() and pg_flush_buffer()
--
SELECT * FROM tt_1;
SELECT pg_mark_buffer_dirty(
    ( 
        SELECT buffernumber 
            FROM pg_show_relation_buffers('test_table_1') 
            ORDER BY fork, blocknumber
            LIMIT 1
    )::integer
);
SELECT * FROM tt_1;
SELECT pg_flush_buffer(
    ( 
        SELECT buffernumber 
            FROM pg_show_relation_buffers('test_table_1') 
            ORDER BY fork, blocknumber 
            LIMIT 1
    )::integer
);
SELECT * FROM tt_1;

-- 
-- Check pg_mark_relation_fork_buffers_dirty() and pg_flush_relation_fork_buffers()
--
SELECT * FROM tt_1;
SELECT pg_mark_relation_fork_buffers_dirty('test_table_1', 'main');
SELECT * FROM tt_1;
SELECT pg_flush_relation_fork_buffers('test_table_1', 'main');
SELECT * FROM tt_1;

-- 
-- Check pg_mark_relation_buffers_dirty() and pg_flush_relation_buffers()
--
SELECT * FROM tt_1;
SELECT pg_mark_relation_buffers_dirty('test_table_1');
SELECT * FROM tt_1;
SELECT pg_flush_relation_buffers('test_table_1');
SELECT * FROM tt_1;

-- 
-- Check pg_mark_database_buffers_dirty() and pg_flush_database_buffers()
--
SELECT * FROM tt_1;
SELECT * FROM tt_2;
\c test_database_2 \\
SELECT * FROM tt_3;
\c test_database_1 \\

SELECT pg_mark_database_buffers_dirty(
    (
        SELECT oid 
            FROM pg_database 
            WHERE datname = 'test_database_1'
    )::oid  
);

SELECT * FROM tt_1;
SELECT * FROM tt_2;
\c test_database_2 \\
SELECT * FROM tt_3;
\c test_database_1 \\

SELECT pg_flush_database_buffers(
    (
        SELECT oid 
            FROM pg_database 
            WHERE datname = 'test_database_1'
    )::oid  
);

SELECT * FROM tt_1;
SELECT * FROM tt_2;
\c test_database_2 \\
SELECT * FROM tt_3;
\c test_database_1 \\

-- 
-- Check pg_mark_tablespace_buffers_dirty() and pg_flush_tablespace_buffers()
--
SELECT * FROM tt_1;
SELECT * FROM tt_2;
\c test_database_2 \\
SELECT * FROM tt_3;
\c test_database_1 \\

SELECT pg_mark_tablespace_buffers_dirty(
    (
        SELECT oid 
            FROM pg_tablespace 
            WHERE spcname = 'test_tablespace'
    )::oid  
);

SELECT * FROM tt_1;
SELECT * FROM tt_2;
\c test_database_2 \\
SELECT * FROM tt_3;
\c test_database_1 \\

SELECT pg_flush_tablespace_buffers(
    (
        SELECT oid 
            FROM pg_tablespace 
            WHERE spcname = 'test_tablespace'
    )::oid  
);

SELECT * FROM tt_1;
SELECT * FROM tt_2;
\c test_database_2 \\
SELECT * FROM tt_3;
\c test_database_1 \\

-- 
-- Check pg_read_page_into_buffer()
--
SELECT (
    (SELECT pg_read_page_into_buffer('test_table_1', 'main', 0)) = 
    (SELECT buffernumber FROM pg_show_relation_buffers('test_table_1') 
        WHERE blocknumber = 0 AND fork = 'main')
);

--
-- Ending
--
DROP TABLE test_table_1;
DROP TABLE test_table_2;
\c test_database_2 \\
DROP TABLE test_table_3;

DROP TABLESPACE test_tablespace;
\c template1 \\

DROP DATABASE test_database_1;
DROP DATABASE test_database_2;