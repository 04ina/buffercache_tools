/*-------------------------------------------------------------------------
 *
 * buffercache_tools.c
 * 
 *		Definitions of extension functions 
 *		that manipulate the buffer cache
 *
 *-------------------------------------------------------------------------
 */

#include "buffercache_tools_internals.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(pg_change_buffer);
PG_FUNCTION_INFO_V1(pg_change_relation_fork_buffers);
PG_FUNCTION_INFO_V1(pg_change_relation_buffers);
PG_FUNCTION_INFO_V1(pg_change_database_buffers);
PG_FUNCTION_INFO_V1(pg_change_tablespace_buffers);
PG_FUNCTION_INFO_V1(pg_change_all_valid_buffers);

PG_FUNCTION_INFO_V1(pg_show_buffer);
PG_FUNCTION_INFO_V1(pg_show_relation_buffers);
PG_FUNCTION_INFO_V1(pg_read_page_into_buffer);

/*
 * Number of arguments of pg_change_* functions
 * not including buffer processing functions (BPF) arguments
 */
#define PG_CHANGE_BUFFER_NUM_MAIN_ARGS 					2
#define PG_CHANGE_RELATION_FORK_BUFFERS_NUM_MAIN_ARGS	3
#define PG_CHANGE_RELATION_BUFFERS_NUM_MAIN_ARGS		2
#define PG_CHANGE_DATABASE_BUFFERS_NUM_MAIN_ARGS		2
#define PG_CHANGE_TABLESPACE_BUFFERS_NUM_MAIN_ARGS		2	
#define PG_CHANGE_ALL_VALID_BUFFERS_NUM_MAIN_ARGS		1	

/*-------------------------------------------------------------------------
 * 								extension functions	 
 *-------------------------------------------------------------------------
 */
Datum
pg_show_buffer(PG_FUNCTION_ARGS)
{
	Buffer buffer = (Buffer) PG_GETARG_INT32(0);

	superuser_check();

	pg_show_buffer_internals(fcinfo, buffer);

	PG_RETURN_BOOL(true);
}

/*
 * Show buffers from the buffer cache that belong to 
 * a specific relation  
 */
Datum
pg_show_relation_buffers(PG_FUNCTION_ARGS) 
{
	text *relname = PG_GETARG_TEXT_PP(0);

	superuser_check();

	pg_show_relation_buffers_internals(fcinfo, relname);

	PG_RETURN_BOOL(true);
}

/*
 * Read a specific page of a specific relation into the buffer cache
 */
Datum
pg_read_page_into_buffer(PG_FUNCTION_ARGS)
{
	text *relName = PG_GETARG_TEXT_PP(0);
	text *forkName = PG_GETARG_TEXT_PP(1);
	BlockNumber blockNum = (BlockNumber) PG_GETARG_INT32(2);

	Buffer buffer = pg_read_page_into_buffer_internals(relName, forkName, blockNum);

	PG_RETURN_INT32((int32) buffer);
}

Datum
pg_change_buffer(PG_FUNCTION_ARGS)
{
	char 	*buf_proc_func_name = text_to_cstring(PG_GETARG_TEXT_PP(0)); 

	Buffer	buffer = (Buffer) PG_GETARG_INT32(1);

	NullableDatum 	*bpf_args = fcinfo->args + PG_CHANGE_BUFFER_NUM_MAIN_ARGS;
	BufProcFunc 	buf_proc_func = buf_proc_func_name_to_number(buf_proc_func_name);
	short 			bpf_nargs = PG_NARGS() - PG_CHANGE_BUFFER_NUM_MAIN_ARGS;

	superuser_check();

	bpf_func_nargs_check(buf_proc_func, bpf_nargs);

	one_buffer_handler(buf_proc_func, buffer, bpf_args);

	PG_RETURN_BOOL(true);
}

Datum
pg_change_relation_fork_buffers(PG_FUNCTION_ARGS) 
{
	char *buf_proc_func_name = text_to_cstring(PG_GETARG_TEXT_PP(0)); 

	text *relName = PG_GETARG_TEXT_PP(1);
	text *forkName = PG_GETARG_TEXT_PP(2);

	NullableDatum 	*bpf_args = fcinfo->args + PG_CHANGE_RELATION_FORK_BUFFERS_NUM_MAIN_ARGS;
	BufProcFunc 	buf_proc_func = buf_proc_func_name_to_number(buf_proc_func_name);
	short 			bpf_nargs = PG_NARGS() - PG_CHANGE_RELATION_FORK_BUFFERS_NUM_MAIN_ARGS;

	superuser_check();

	bpf_func_nargs_check(buf_proc_func, bpf_nargs);

	relation_fork_buffers_handler(buf_proc_func, relName, forkName, bpf_args);

	PG_RETURN_BOOL(true);
}

Datum
pg_change_relation_buffers(PG_FUNCTION_ARGS) 
{
	char *buf_proc_func_name = text_to_cstring(PG_GETARG_TEXT_PP(0)); 

	text *relName = PG_GETARG_TEXT_PP(1);

	NullableDatum 	*bpf_args = fcinfo->args + PG_CHANGE_RELATION_BUFFERS_NUM_MAIN_ARGS;
	BufProcFunc 	buf_proc_func = buf_proc_func_name_to_number(buf_proc_func_name);
	short 			bpf_nargs = PG_NARGS() - PG_CHANGE_RELATION_BUFFERS_NUM_MAIN_ARGS;

	superuser_check();

	bpf_func_nargs_check(buf_proc_func, bpf_nargs);

	relation_buffers_handler(buf_proc_func, relName, bpf_args);

	PG_RETURN_BOOL(true);
}

Datum 
pg_change_database_buffers(PG_FUNCTION_ARGS)
{
	char *buf_proc_func_name = text_to_cstring(PG_GETARG_TEXT_PP(0)); 

	Oid dbOid = PG_GETARG_OID(1);	
	
	NullableDatum 	*bpf_args = fcinfo->args + PG_CHANGE_DATABASE_BUFFERS_NUM_MAIN_ARGS;
	BufProcFunc 	buf_proc_func = buf_proc_func_name_to_number(buf_proc_func_name);
	short 			bpf_nargs = PG_NARGS() - PG_CHANGE_DATABASE_BUFFERS_NUM_MAIN_ARGS;

	superuser_check();

	bpf_func_nargs_check(buf_proc_func, bpf_nargs);

	if (database_is_invalid_oid(dbOid)) 
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				errmsg("invalid database oid")));

	database_buffers_handler(buf_proc_func, dbOid, bpf_args);

	PG_RETURN_BOOL(true);
}

Datum
pg_change_tablespace_buffers(PG_FUNCTION_ARGS)
{
	char *buf_proc_func_name = text_to_cstring(PG_GETARG_TEXT_PP(0)); 

	Oid spcOid = PG_GETARG_OID(1);	

	NullableDatum 	*bpf_args = fcinfo->args + PG_CHANGE_TABLESPACE_BUFFERS_NUM_MAIN_ARGS;
	BufProcFunc 	buf_proc_func = buf_proc_func_name_to_number(buf_proc_func_name);
	short 			bpf_nargs = PG_NARGS() - PG_CHANGE_TABLESPACE_BUFFERS_NUM_MAIN_ARGS;
	
	superuser_check();

	bpf_func_nargs_check(buf_proc_func, bpf_nargs);

	tablespace_buffers_handler(buf_proc_func, spcOid, bpf_args);

	PG_RETURN_BOOL(true);
}

Datum
pg_change_all_valid_buffers(PG_FUNCTION_ARGS)
{
	char *buf_proc_func_name = text_to_cstring(PG_GETARG_TEXT_PP(0)); 

	NullableDatum 	*bpf_args = fcinfo->args + PG_CHANGE_ALL_VALID_BUFFERS_NUM_MAIN_ARGS;
	BufProcFunc 	buf_proc_func = buf_proc_func_name_to_number(buf_proc_func_name);
	short 			bpf_nargs = PG_NARGS() - PG_CHANGE_ALL_VALID_BUFFERS_NUM_MAIN_ARGS;
	
	superuser_check();

	bpf_func_nargs_check(buf_proc_func, bpf_nargs);

	all_valid_buffers_handler(buf_proc_func, bpf_args);

	PG_RETURN_BOOL(true);
}
