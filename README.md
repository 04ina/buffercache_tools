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
### Flush (write) buffers
### Mark buffers dirty
### Read buffers into buffer cache
## Regression testing  
### make  
> make installcheck    
> (or "make PG_CONFIG=postgres_directory/bin/pg_config installcheck" if postgreSQL bin directory is not in PATH)   
### meson  
> cd build  
> ninja test  


