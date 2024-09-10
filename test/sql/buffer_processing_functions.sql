--
-- Preparing
--
SET allow_in_place_tablespaces = true;
CREATE TABLESPACE test_tablespace LOCATION '';
RESET allow_in_place_tablespaces;

CREATE DATABASE test_database;

\c test_database \\
CREATE EXTENSION buffercache_tools;

-- test table
CREATE TABLE test_table(col integer) 
    WITH (autovacuum_enabled = off)
    TABLESPACE test_tablespace;
INSERT INTO test_table 
    SELECT 1 FROM generate_series(1,1000); 

-- test buffer number
CREATE MATERIALIZED VIEW test_buf_num  AS (
    SELECT buffernum::integer
        FROM pg_show_relation_buffers('test_table') 
        ORDER BY fork, blocknum
        LIMIT 1
);

-- test buffer source data
CREATE MATERIALIZED VIEW test_buf_src_dat AS (
    SELECT * FROM pg_show_buffer(
        (TABLE test_buf_num)::integer
    )
);

VACUUM;
CHECKPOINT;

--
-- Tests 
--

-- Check 'mark_dirty' and 'flush' buffer processing functions for pg_change_buffer()
SELECT dirty FROM pg_show_buffer((TABLE test_buf_num)); 

SELECT pg_change_buffer('mark_dirty', (TABLE test_buf_num));

SELECT dirty FROM pg_show_buffer((TABLE test_buf_num)); 

SELECT pg_change_buffer('flush', (TABLE test_buf_num));

SELECT dirty FROM pg_show_buffer((TABLE test_buf_num)); 

-- Check 'change_spcoid' buffer processing function for pg_change_buffer()
SELECT pg_change_buffer(
    'change_spcoid', 
    (TABLE test_buf_num), 
    10000::Oid
);

SELECT spcoid FROM pg_show_buffer((TABLE test_buf_num));

SELECT pg_change_buffer(
    'change_spcoid', 
    (TABLE test_buf_num), 
    (SELECT spcoid FROM test_buf_src_dat)::Oid
); 

-- Check 'change_dboid' buffer processing function for pg_change_buffer()
SELECT pg_change_buffer(
    'change_dboid', 
    (TABLE test_buf_num), 
    10000::Oid
);

SELECT dboid FROM pg_show_buffer((TABLE test_buf_num));

SELECT pg_change_buffer(
    'change_dboid', 
    (TABLE test_buf_num), 
    (SELECT dboid FROM test_buf_src_dat)::Oid
); 

-- Check 'change_relnumber' buffer processing function for pg_change_buffer()
SELECT pg_change_buffer(
    'change_relnumber', 
    (TABLE test_buf_num), 
    10000::Oid
);

SELECT relnumber FROM pg_show_buffer((TABLE test_buf_num));

SELECT pg_change_buffer(
    'change_relnumber', 
    (TABLE test_buf_num), 
    (SELECT relnumber FROM test_buf_src_dat)::Oid
); 

-- Check 'change_forknum' buffer processing function for pg_change_buffer()
SELECT pg_change_buffer(
    'change_forknum', 
    (TABLE test_buf_num), 
    'init' 
);

SELECT fork FROM pg_show_buffer((TABLE test_buf_num));

SELECT pg_change_buffer(
    'change_forknum', 
    (TABLE test_buf_num), 
    (SELECT fork FROM test_buf_src_dat)::text
);

-- Check 'change_blocknum' buffer processing function for pg_change_buffer()
SELECT pg_change_buffer(
    'change_blocknum', 
    (TABLE test_buf_num), 
    232323::bigint 
);

SELECT blocknum FROM pg_show_buffer((TABLE test_buf_num));

SELECT pg_change_buffer(
    'change_blocknum', 
    (TABLE test_buf_num), 
    (SELECT blocknum FROM test_buf_src_dat)::Oid
); 

--
-- Cleanup 
--
DROP MATERIALIZED VIEW test_buf_src_dat;
DROP MATERIALIZED VIEW test_buf_num;
DROP TABLE test_table;

DROP TABLESPACE test_tablespace;

\c template1 \\
DROP DATABASE test_database;