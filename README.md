# buffercache_tools
PostgreSQL Extention for buffer cache manipulation.

buffercache_tools allows you to flush and mark dirty buffers of various postgres objects, such as: 
Relation 
Relation fork
Database
Tablespace 
It is possible to clear and mark dirty specific buffers.   
  
Also, buffercache_tools allows you to read a specific page of a relation into the buffer cache and see information about all buffers of a specific relation.

1. [Install](#install)
2. [Usage](#usage)
3. [Test suite](#test-suite)

## Install  
This extension supports building with make and meson. It is necessary that the postgres bin directory is specified in the PATH environment variable. Alternatively, you can also specify the absolute directory of the client application yourself.

### make
```sh
make install
```    
or 
```sh
make PG_CONFIG=postgres_directory/bin/pg_config install
```
### meson
```sh
meson build  
cd build  
ninja install
```
or
```sh  
meson build -Dpg_config=postgres_directory/bin/pg_config      
cd build  
ninja install
```
## Usage
There are 12 functions for working with the buffer cache.  

At the moment, information about temporary table buffers can only be viewed, but not changed.
### pg_show_relation_buffers(relname text) 
Show buffers from the buffer cache that belong to a specific relation.  
```sql
SELECT * FROM pg_show_relation_buffers('test_table');
 blocknumber | buffernumber | dirty | usagecount | pinning | fork 
-------------+--------------+-------+------------+---------+------
           4 |         5076 | t     |          5 |       0 | main
           3 |         5077 | t     |          5 |       0 | main
           2 |         5078 | t     |          5 |       0 | main
           1 |         5079 | t     |          5 |       0 | main
           2 |         5080 | f     |          4 |       0 | fsm
           1 |         5081 | f     |          1 |       0 | fsm
           0 |         5082 | f     |          5 |       0 | fsm
           0 |         5083 | t     |          5 |       0 | main
```
The standard pg_buffercache extension also has a function for displaying information about buffers in the buffer cache, but pg_show_relation_buffers() will also let you see information about local buffers of temporary tables that exist only in the current session.
```sql
SELECT * FROM pg_show_relation_buffers('test_temp_table');
 blocknumber | buffernumber | dirty | usagecount | pinning | fork 
-------------+--------------+-------+------------+---------+------
           0 |           -1 | t     |          3 |       0 | main
           0 |           -2 | t     |          2 |       0 | vm
           0 |           -3 | t     |          2 |       0 | fsm
           1 |           -4 | t     |          2 |       0 | fsm
           2 |           -5 | t     |          2 |       0 | fsm
```
### pg_mark_buffer_dirty(buffer integer)
Mark buffer dirty.
```sql
SELECT pg_mark_buffer_dirty(5076);
 pg_mark_buffer_dirty 
----------------------
 t
```
### pg_flush_buffer(buffer integer)
Write (flush) buffer page to disk without drop.
```sql
SELECT pg_flush_buffer(5076);
 pg_flush_buffer 
-----------------
 t
```
### pg_mark_relation_fork_buffers_dirty(relname text, fork text)
Mark relation buffer pages dirty of a specific fork. 
```sql
SELECT * FROM pg_mark_relation_fork_buffers_dirty('test_table', 'main');
 pg_mark_relation_fork_buffers_dirty 
-------------------------------------
 t
```
### pg_flush_relation_fork_buffers(relname text, fork text)
Write relation buffer pages of a specific fork to disk without drop.
```sql
SELECT * FROM pg_flush_relation_fork_buffers('test_table', 'main');
 pg_flush_relation_fork_buffers 
--------------------------------
 t
```
### pg_mark_relation_buffers_dirty(relname text)
Mark relation buffer pages dirty.
```sql
SELECT * FROM pg_mark_relation_buffers_dirty('test_table');
 pg_mark_relation_buffers_dirty 
--------------------------------
 t
```
### pg_flush_relation_buffers(relname text)
Write relation buffer pages to disk without drop.
```sql
SELECT * FROM pg_flush_relation_buffers('test_table');
 pg_flush_relation_buffers 
---------------------------
 t
```
### pg_mark_database_buffers_dirty(dboid oid)
Mark all database buffer pages dirty.
```sql
SELECT * FROM pg_mark_database_buffers_dirty(4);
 pg_mark_database_buffers_dirty 
--------------------------------
 t
```
### pg_flush_database_buffers(dboid oid)
Write all database buffer pages to disk without drop.
```sql
SELECT * FROM pg_flush_database_buffers(4);
 pg_flush_database_buffers 
---------------------------
 t
```
### pg_mark_tablespace_buffers_dirty(spcoid oid)
Mark all tablespace buffer pages drity. 
```sql
SELECT * FROM pg_mark_tablespace_buffers_dirty(1456);
 pg_mark_tablespace_buffers_dirty 
----------------------------------
 t
```
### pg_flush_tablespace_buffers(spcoid oid)
Write all tablespace buffer pages to disk without drop.
```sql
SELECT * FROM pg_flush_tablespace_buffers(1456);
 pg_flush_tablespace_buffers 
-----------------------------
 t
```
### pg_read_page_into_buffer(relname text, fork text, blocknumber integer)
Read a specific page of a specific relation into the buffer cache. Returns the number of the filled buffer.
```sql
SELECT pg_read_page_into_buffer('test', 'main', 0);
 pg_read_page_into_buffer 
--------------------------
                     5074
```
## Test suite 
To run the test suite, execute
```sh
make installcheck    
```
or
```sh
cd build  
ninja test  
```
after installation.
