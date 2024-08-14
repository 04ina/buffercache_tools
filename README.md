# buffercache_tools
PostgreSQL Extention for buffer cache manipulation 
## Installation  
> git clone https://github.com/04ina/buffercache_tools.git   
> cd buffercache_tools
### make
> make install    
> (or "make PG_CONFIG=postgres_directory/bin/pg_config install" if postgreSQL bin directory is not in PATH)    
### meson
> meson build  
> (or "meson build -Dpg_config=postgres_directory/bin/pg_config" if postgreSQL bin directory is not in PATH)      
> cd build  
> ninja install  
### Setup in postgres
> CREATE EXTENSION buffercache_tools;
## Functions
### Show buffers
pg_show_relation_buffers(relname text) -  Show buffers from the buffer cache that belong to a specific relation.
example:
> SELECT * FROM pg_show_relation_buffers('test_table');  
>  blocknumber | buffernumber | dirty | usagecount | pinning | fork   
> -------------+--------------+-------+------------+---------+------  
>            4 |         5076 | t     |          5 |       0 | main  
>            3 |         5077 | t     |          5 |       0 | main  
>            2 |         5078 | t     |          5 |       0 | main  
>            1 |         5079 | t     |          5 |       0 | main  
>            2 |         5080 | f     |          4 |       0 | fsm  
>            1 |         5081 | f     |          1 |       0 | fsm  
>            0 |         5082 | f     |          5 |       0 | fsm  
>            0 |         5083 | t     |          5 |       0 | main   
### Flush (write) buffers
#### pg_flush_buffer
#### pg_flush_relation_fork_buffers
#### pg_flush_relation_buffers
#### pg_flush_database_buffers
### Mark buffers dirty
#### pg_mark_buffer_dirty
### Read buffers into buffer cache
#### pg_read_page_into_buffer
## Regression testing  
### make  
> make installcheck    
> (or "make PG_CONFIG=postgres_directory/bin/pg_config installcheck" if postgreSQL bin directory is not in PATH)   
### meson  
> cd build  
> ninja test  


