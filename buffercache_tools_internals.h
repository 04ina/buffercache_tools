/*-------------------------------------------------------------------------
 *
 * buffercache_tools_internals.h
 * 
 * 		Functions for manipulating the buffer cache 
 *
 *-------------------------------------------------------------------------
 */

#ifndef BUFFERCACHE_TOOLS_INTERNALS_H
#define BUFFERCACHE_TOOLS_INTERNALS_H

#include "postgres.h"

#include "catalog/pg_database.h"
#include "funcapi.h"
#include "storage/bufmgr.h"
#include "storage/buf_internals.h"
#include "utils/builtins.h"

/*
 * Types of buffer processing functions
 */
typedef enum BufProcFunc {
	BCT_INVALID_BPF = -1,
	BCT_MARK_DIRTY,
	BCT_FLUSH,
	BCT_CHANGE_SPCOID,
	BCT_CHANGE_DBOID,
	BCT_CHANGE_RELNUMBER,
	BCT_CHANGE_FORKNUM,
	BCT_CHANGE_BLOCKNUM
} BufProcFunc;

#define MAX_BPF_NUM	BCT_CHANGE_BLOCKNUM

/*-------------------------------------------------------------------------
 * 								function Headers 
 *-------------------------------------------------------------------------
 */


/* 
 * handler functions headers
 */
extern void one_buffer_handler(BufProcFunc buf_proc_func, Buffer buffer, 
								  NullableDatum *bpf_args);

extern void relation_fork_buffers_handler(int32 buf_proc_func, text *relName, text *forkName, 
											 NullableDatum *bpf_args);

extern void relation_buffers_handler(int32 buf_proc_func, text *relName, 
										NullableDatum *bpf_args);

extern void database_buffers_handler(int32 buf_proc_func, Oid dbOid, NullableDatum *bpf_args);

extern void tablespace_buffers_handler(int32 buf_proc_func, Oid spcOid, NullableDatum *bpf_args);

/*
 * Check functions
 */
extern void superuser_check(void);

extern void other_temp_check(Relation rel);

extern void buffer_is_correct_check(Buffer buffer);

extern void buffer_is_not_local_check(Buffer buffer);

extern void bpf_func_nargs_check(BufProcFunc buf_proc_func, short nargs);

extern void int64_to_block_number_convert_check(int64 int64_value);

extern void block_num_not_exist_in_relation_check(Relation rel, ForkNumber forkNum, BlockNumber blockNum);

extern void fork_num_correct_check(ForkNumber forkNum);

/*
 * Other functions
 */
extern Buffer pg_read_page_into_buffer_internals(text *relName, text *forkName, BlockNumber blockNum);

extern void pg_show_buffer_internals(FunctionCallInfo fcinfo, Buffer buffer);

extern void pg_show_relation_buffers_internals(FunctionCallInfo fcinfo, text *relname);

extern ForkNumber buf_proc_func_name_to_number(const char *bpfname);

#endif  /* BUFFERCACHE_TOOLS_INTERNALS_H */