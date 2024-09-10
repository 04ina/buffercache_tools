# Makefile

MODULE_big = buffercache_tools
OBJS = \
		buffercache_tools.o \
		buffercache_tools_internals.o

EXTENSION = buffercache_tools 
DATA = buffercache_tools--1.0.sql

REGRESS = \
	buffer_processing_functions \
	change_func_buffers_coverage \
	read_page_into_buffer

REGRESS_OPTS = --inputdir=test

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)