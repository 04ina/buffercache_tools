CREATE DATABASE test_database;
\c test_database;

CREATE EXTENSION buffercache_tools;

CREATE TABLE test_table(col integer) 
    WITH (autovacuum_enabled = off);

-- check pg_show_relation_buffers() 
SELECT blocknumber, dirty, usagecount, pinning, fork 
    FROM pg_show_relation_buffers('test_table') 
    ORDER BY fork, blocknumber;
INSERT INTO test_table 
    SELECT 1 FROM generate_series(1,1000); 
SELECT blocknumber, dirty, usagecount, pinning, fork 
    FROM pg_show_relation_buffers('test_table') 
    ORDER BY fork, blocknumber;
VACUUM test_table;
SELECT blocknumber, dirty, usagecount, pinning, fork 
    FROM pg_show_relation_buffers('test_table') 
    ORDER BY fork, blocknumber;
TRUNCATE TABLE test_table;

-- check pg_flush_buffer()
INSERT INTO test_table 
    SELECT 1 FROM generate_series(1,1000); 
SELECT pg_flush_buffer(
    ( 
        SELECT buffernumber 
            FROM pg_show_relation_buffers('test_table') 
            LIMIT 1
    )::integer
);
SELECT blocknumber, dirty, usagecount, pinning, fork 
    FROM pg_show_relation_buffers('test_table') 
    ORDER BY fork, blocknumber;

-- check pg_mark_buffer_dirty()
SELECT pg_mark_buffer_dirty(
    (
        SELECT buffernumber 
        FROM pg_show_relation_buffers('test_table') 
        LIMIT 1
    )::integer
);
SELECT blocknumber, dirty, usagecount, pinning, fork 
    FROM pg_show_relation_buffers('test_table') 
    ORDER BY fork, blocknumber;

-- check pg_flush_relation_fork_buffers()
SELECT pg_flush_relation_fork_buffers('test_table', 'main');
SELECT blocknumber, dirty, usagecount, pinning, fork 
    FROM pg_show_relation_buffers('test_table') 
    ORDER BY fork, blocknumber;

-- check pg_flush_database_buffers()
VACUUM test_table;
SELECT pg_flush_database_buffers(
    (
        SELECT oid 
            FROM pg_database 
            WHERE datname = 'test_database'
    )::oid
);
SELECT blocknumber, dirty, usagecount, pinning, fork 
    FROM pg_show_relation_buffers('test_table') 
    ORDER BY fork, blocknumber;

-- check pg_read_page_into_buffer()
SELECT 1 
    FROM pg_read_page_into_buffer('test_table', 'main', 0);
SELECT blocknumber, dirty, usagecount, pinning, fork 
    FROM pg_show_relation_buffers('test_table') 
    ORDER BY fork, blocknumber;
SELECT 1 
    FROM pg_read_page_into_buffer('test_table', 'main', 0);

DROP TABLE test_table;
\c template1
DROP DATABASE test_database;