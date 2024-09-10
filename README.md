# buffercache_tools
PostgreSQL Extention for buffer cache manipulation.

buffercache_tools allows you to change buffers of various postgres objects, such as:
1. Relation
2. Relation fork
3. Database
4. Tablespace

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
### pg_change_*
There are 5 buffer change functions with different coverages:
1. pg_change_buffer()
2. pg_change_relation_fork_buffers()
3. pg_change_relation_buffers()
4. pg_change_database_buffers()
5. pg_change_tablespace_buffers()

#### The arguments of the pg_change_* functions consist of three types of arguments:
1. Buffer change mode (In the source code it's called "buffer processing function").
2. The identifier of the object whose buffers need to be modified (for example oid for pg_change_database_buffers()).
3. Arguments for the corresponding buffer modification mode.

#### Buffer change modes:
1. mark_dirty - mark buffer dirty. Arguments: not required.
2. flush - Write buffer page to disk without drop. Arguments: not required.
4. change_spcoid - change tablespace oid. Arguments: Tablespace Oid.
5. change_dboid - change database oid. Arguments: Database Oid.
6. change_relnumber - change relnumber. Arguments: relnumber relations.
7. change_forknum - change fork number. Arguments: Fork name in text format ('main', 'fsm', 'vm', 'init').
8. change_blocknum - change block number. Arguments: block number.

#### Examples:
```sql
pg_change_buffer('mark_dirty', 400);
pg_change_buffer('flush', 400);
pg_change_buffer('change_spcoid', 400, 1253);
pg_change_buffer('change_dboid', 400, 154);
pg_change_buffer('change_relnumber', 400, 254);
pg_change_buffer('change_forknum', 400, 'main');
pg_change_buffer('change_blocknum', 400, 34252);

pg_change_relation_fork_buffers('change_blocknum', 'test_table', 'main', 34252);
pg_change_relation_buffers('change_blocknum', 'test_table', 34252);
pg_change_database_buffers('change_blocknum', 4321, 34252);
pg_change_tablespace_buffers('change_blocknum', 5421, 34252);
```
If the function was executed successfully, the output value will be true.
```sql
SELECT * FROM pg_change_relation_fork_buffers('mark_dirty', 'test_table', 'main');
 pg_change_relation_fork_buffers 
---------------------------------
 t
```
Information about temporary table buffers can only be viewed, but not changed.
### pg_show_relation_buffers(relname text) 
Show information about buffers from the buffer cache that belong to a specific relation.  
```sql
 SELECT * FROM pg_show_relation_buffers('test_table');
 buffernum | blocknum | dirty | usagecount | pinning | fork 
-----------+----------+-------+------------+---------+------
      4487 |        2 | t     |          2 |       0 | fsm
      4488 |        1 | t     |          2 |       0 | fsm
      4489 |        0 | t     |          2 |       0 | fsm
      4490 |        0 | t     |          2 |       0 | vm
      4491 |        0 | t     |          1 |       0 | main
```
The standard pg_buffercache extension also has a function for displaying information about buffers in the buffer cache, but pg_show_relation_buffers() will also let you see information about local buffers of temporary tables that exist only in the current session.
```sql
SELECT * FROM pg_show_relation_buffers('test_table');
 buffernum | blocknum | dirty | usagecount | pinning | fork 
-----------+----------+-------+------------+---------+------
        -1 |        0 | t     |          2 |       0 | main
        -2 |        0 | t     |          2 |       0 | vm
        -3 |        0 | t     |          2 |       0 | fsm
        -4 |        1 | t     |          2 |       0 | fsm
        -5 |        2 | t     |          2 |       0 | fsm
```
### pg_show_buffer(buffer integer)
Show information about specific buffer from the buffer cache. pg_show_buffer() does not supported local buffers.
```sql
SELECT * FROM pg_show_buffer(1);
 blocknum | fork | relnumber | dboid | spcoid | dirty | usagecount | pinning 
----------+------+-----------+-------+--------+-------+------------+---------
        0 | main |      1262 |     0 |   1664 | f     |          5 |       0
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
To run the test suite, execute:
```sh
make installcheck    
```
or
```sh
cd build  
ninja test  
```
after installation.