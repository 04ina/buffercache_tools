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
    SELECT blocknum, dirty, pinning, fork 
        FROM pg_show_relation_buffers('test_table_1') 
        ORDER BY fork, blocknum; 

CREATE TABLE test_table_2(col integer) 
    WITH (autovacuum_enabled = off)
    TABLESPACE test_tablespace;
INSERT INTO test_table_2 
    SELECT 1 FROM generate_series(1,1000); 

CREATE VIEW tt_2 AS 
    SELECT blocknum, dirty, pinning, fork 
        FROM pg_show_relation_buffers('test_table_2') 
        ORDER BY fork, blocknum; 

\c test_database_2 \\
CREATE EXTENSION buffercache_tools;

CREATE TABLE test_table_3(col integer) 
    WITH (autovacuum_enabled = off)
    TABLESPACE pg_default;
INSERT INTO test_table_3 
    SELECT 1 FROM generate_series(1,1000); 

CREATE VIEW tt_3 AS 
    SELECT blocknum, dirty, pinning, fork 
        FROM pg_show_relation_buffers('test_table_3') 
        ORDER BY fork, blocknum; 
VACUUM;
CHECKPOINT;

\c test_database_1 \\

VACUUM;
CHECKPOINT;

--
-- Tests
--

-- 
-- Check pg_change_buffer() buffers coverage
--
SELECT * FROM tt_1;
SELECT pg_change_buffer('mark_dirty',
    ( 
        SELECT buffernum 
            FROM pg_show_relation_buffers('test_table_1') 
            ORDER BY fork, blocknum
            LIMIT 1
    )::integer
);
SELECT * FROM tt_1;
SELECT pg_change_buffer('flush',
    ( 
        SELECT buffernum 
            FROM pg_show_relation_buffers('test_table_1') 
            ORDER BY fork, blocknum 
            LIMIT 1
    )::integer
);
SELECT * FROM tt_1;

-- 
-- Check pg_change_relation_fork_buffers() buffers coverage
--
SELECT * FROM tt_1;
SELECT pg_change_relation_fork_buffers('mark_dirty', 'test_table_1', 'main');
SELECT * FROM tt_1;
SELECT pg_change_relation_fork_buffers('flush', 'test_table_1', 'main');
SELECT * FROM tt_1;

-- 
-- Check pg_change_relation_buffers() buffers coverage
--
SELECT * FROM tt_1;
SELECT pg_change_relation_buffers('mark_dirty', 'test_table_1');
SELECT * FROM tt_1;
SELECT pg_change_relation_buffers('flush', 'test_table_1');
SELECT * FROM tt_1;

-- 
-- Check pg_change_database_buffers() buffers coverage 
--
SELECT * FROM tt_1;
SELECT * FROM tt_2;
\c test_database_2 \\
SELECT * FROM tt_3;
\c test_database_1 \\

SELECT pg_change_database_buffers('mark_dirty',
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

SELECT pg_change_database_buffers('flush',
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
-- Check pg_change_tablespace_buffers() buffers coverage 
--
SELECT * FROM tt_1;
SELECT * FROM tt_2;
\c test_database_2 \\
SELECT * FROM tt_3;
\c test_database_1 \\

SELECT pg_change_tablespace_buffers('mark_dirty',
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

SELECT pg_change_tablespace_buffers('flush',
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
-- Check pg_change_all_valid_buffers() buffers coverage 
--
SELECT * FROM tt_1;
SELECT * FROM tt_2;
\c test_database_2 \\
SELECT * FROM tt_3;
\c test_database_1 \\

SELECT pg_change_all_valid_buffers('mark_dirty');

SELECT * FROM tt_1;
SELECT * FROM tt_2;
\c test_database_2 \\
SELECT * FROM tt_3;
\c test_database_1 \\

SELECT pg_change_all_valid_buffers('flush');

SELECT * FROM tt_1;
SELECT * FROM tt_2;
\c test_database_2 \\
SELECT * FROM tt_3;
\c test_database_1 \\

-- 
-- Check pg_change_buffer_by_page() buffers coverage
--
SELECT * FROM tt_1;
SELECT pg_change_buffer_by_page('mark_dirty', 'test_table_1', 'main', '0');
SELECT * FROM tt_1;
SELECT pg_change_buffer_by_page('flush', 'test_table_1', 'main', '0');
SELECT * FROM tt_1;

--
-- Cleanup 
--
DROP TABLE test_table_1;
DROP TABLE test_table_2;
\c test_database_2 \\
DROP TABLE test_table_3;

DROP TABLESPACE test_tablespace;
\c template1 \\

DROP DATABASE test_database_1;
DROP DATABASE test_database_2;