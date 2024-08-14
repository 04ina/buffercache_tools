EXTENSION = buffercache_tools 

MODULES = buffercache_tools 
OBJS = buffercache_tools.o

DATA = buffercache_tools--1.0.sql

REGRESS = buffercache_tools 

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)


