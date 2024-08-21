# buffercache_tools
PostgreSQL Extention for buffer cache manipulation.

1. [Install](#install)
2. [Usage](#usage)
3. [Regression testing](#Regression-testing)

## Install  
```sh
git clone https://github.com/04ina/buffercache_tools.git   
cd buffercache_tools
```
### make
```sh
make install
```    
or 
```sh
make PG_CONFIG=postgres_directory/bin/pg_config install
```
if postgreSQL bin directory is not in PATH, where postgres_directory is the main PostgreSQL directory.  
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
if postgreSQL bin directory is not in PATH, where postgres_directory is the main PostgreSQL directory.  
### Setup in postgres
```sql
CREATE EXTENSION buffercache_tools;
```
## Usage
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
## Regression testing  
To run the test suite, execute
```sh
make installcheck    
```
or
```sh
cd build  
ninja test  
```


make PG_CONFIG=postgres_directory/bin/pg_config installcheck
