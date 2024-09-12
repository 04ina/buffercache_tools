/*-------------------------------------------------------------------------
 *
 * buffercache_tools_internals.c
 * 
 * 		Functions for manipulating the buffer cache 
 *
 *-------------------------------------------------------------------------
 */

#include "buffercache_tools_internals.h"

#if (PG_VERSION_NUM >= 160000)
#define PG_VERSION_NUM_EQUAL_OR_MORE_160000
#endif

#include "c.h"

#include "access/relation.h"
#include "catalog/namespace.h"
#include "common/relpath.h"
#include "nodes/execnodes.h"
#include "storage/bufmgr.h"
#include "storage/buf_internals.h"

/* PG_VERSION_NUM < 160000 */
#ifndef PG_VERSION_NUM_EQUAL_OR_MORE_160000 
#define HAVE_RELFILENUMBERMAP_H
#include "storage/relfilenode.h"
#endif

#include "storage/lmgr.h"
#include "utils/relcache.h"
#include "miscadmin.h"
#include "utils/tuplestore.h"
#include "utils/varlena.h"

#ifdef PG_VERSION_NUM_EQUAL_OR_MORE_160000

/*
 * does the buffer page belongs to the relation?
 */
#define BCT_IS_BUFFER_BELONGS_RELATION(_bct_bufHdr_, _bct_rel_) \
	BufTagMatchesRelFileLocator(&_bct_bufHdr_->tag, &_bct_rel_->rd_locator)

/*
 * does the buffer page belongs to the database?
 */
#define BCT_IS_BUFFER_BELONGS_DATABASE(_bct_bufHdr_, _bct_dboid_) \
	bufHdr->tag.dbOid == _bct_dboid_ 

/*
 * does the buffer page belongs to the tablespace?
 */
#define BCT_IS_BUFFER_BELONGS_TABLESPACE(_bct_bufHdr_, _bct_spcOid_) \
	bufHdr->tag.spcOid == _bct_spcOid_ 

#else 

/*
 * does the buffer page belongs to the relation?
 */
#define BCT_IS_BUFFER_BELONGS_RELATION(_bct_bufHdr_, _bct_rel_) \
	_bct_bufHdr_->tag.rnode.relNode == _bct_rel_->rd_node.relNode && \
	_bct_bufHdr_->tag.rnode.dbNode == _bct_rel_->rd_node.dbNode && \
	_bct_bufHdr_->tag.rnode.spcNode == _bct_rel_->rd_node.spcNode

/*
 * does the buffer page belongs to the database?
 */
#define BCT_IS_BUFFER_BELONGS_DATABASE(_bct_bufHdr_, _bct_dbOid_) \
	_bct_bufHdr_->tag.rnode.dbNode == _bct_dbOid_ 

/*
 * does the buffer page belongs to the tablespace?
 */
#define BCT_IS_BUFFER_BELONGS_TABLESPACE(_bct_bufHdr_, _bct_spcOid_) \
	_bct_bufHdr_->tag.rnode.spcNode == _bct_spcOid_ 

#endif	/* PG_VERSION_NUM >= 160000*/

/*
 * does the buffer page belongs to the fork?
 */
#define BCT_IS_BUFFER_BELONGS_FORK(_bct_bufHdr_, _bct_forkNum_) \
	_bct_bufHdr_->tag.forkNum == _bct_forkNum_ 

#ifndef tuplestore_donestoring
#define tuplestore_donestoring(state) 	((void) 0)
#endif

/*
 * is buffer valid?
 */
#define BUFFER_IS_VALID(_buf_state_) \
	(_buf_state_ & BM_VALID) && (_buf_state_ & BM_TAG_VALID)

/*
 * Lookup table of buffer processing function name by number 
 */
const char *const bufProcFuncNames[] = {
	[BCT_MARK_DIRTY] = "mark_dirty",
	[BCT_FLUSH] = "flush",
	[BCT_CHANGE_SPCOID] = "change_spcoid",
	[BCT_CHANGE_DBOID] = "change_dboid",
	[BCT_CHANGE_RELNUMBER] = "change_relnumber",
	[BCT_CHANGE_FORKNUM] = "change_forknum",
	[BCT_CHANGE_BLOCKNUM] = "change_blocknum",
};

/*
 * change buffer tag functions headers 
 */
static void ChangeSpcOidBuffer(Buffer buffer, Oid spcOid);
static void ChangeDBOidBuffer(Buffer buffer,  Oid dbOid);
#ifdef HAVE_RELFILENUMBERMAP_H
static void ChangeRelNumberBuffer(Buffer buffer, Oid relNumber);
#else
static void ChangeRelNumberBuffer(Buffer buffer, RelFileNumber relNumber);
#endif  /* HAVE_RELFILENUMBERMAP_H */
static void ChangeForkNumBuffer(Buffer buffer, ForkNumber forkNum);
static void ChangeBlockNumBuffer(Buffer buffer, BlockNumber blockNum);

/* 
 * other functions headers 
 */
static void BufProcFuncWrapper(int32 buf_proc_func, Buffer buffer, NullableDatum *bpf_args);

/*-------------------------------------------------------------------------
 * 							Auxiliary functions
 *-------------------------------------------------------------------------
 */

/*
 * buf_proc_func_name_to_number - look up buffer processing function  number by name
 *
 */
ForkNumber
buf_proc_func_name_to_number(const char *bpfname)
{
	BufProcFunc	bpfnum;

	for (bpfnum = 0; bpfnum <= MAX_BPF_NUM; bpfnum++)
		if (strcmp(bufProcFuncNames[bpfnum], bpfname) == 0)
			return bpfnum;

	ereport(ERROR,
			(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			 errmsg("invalid name of the buffer change mode")));

	return BCT_INVALID_BPF;
}
/*-------------------------------------------------------------------------
 * 								Check functions
 *-------------------------------------------------------------------------
 */

/*
 * Is the caller a superuser?
 */
void
superuser_check(void)
{
	if (!superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				errmsg("must be superuser to flush buffers")));
}

/*
* Reject attempts to read non-local temporary relations; we would be
* likely to get wrong data since we have no visibility into the owning
* session's local buffers.
*/
void 
other_temp_check(Relation rel)
{
	if (RELATION_IS_OTHER_TEMP(rel))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				errmsg("cannot access temporary tables of other sessions")));
}

/*
 * Checking the number of arguments of buffer processing functions 
 */
void
bpf_func_nargs_check(BufProcFunc buf_proc_func, short nargs)
{
	bool invalid_nargs = false;

	switch(buf_proc_func)	
	{
		/* one arg*/
		case BCT_MARK_DIRTY:
		case BCT_FLUSH:
			if (nargs != 0)
				invalid_nargs = true;	
			break;
		/* two args*/
		case BCT_CHANGE_SPCOID:
		case BCT_CHANGE_DBOID:
		case BCT_CHANGE_RELNUMBER:
		case BCT_CHANGE_FORKNUM:
		case BCT_CHANGE_BLOCKNUM:
			if (nargs != 1)
				invalid_nargs = true;	
			break;
		default:
			Assert(false);
	}

	if (invalid_nargs)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				errmsg("invalid number of arguments")));
}

/*
 * Is it possible to convert int64 to BlockNumber 
 */
void
int64_to_block_number_convert_check(int64 int64_value)
{
	if (int64_value > PG_UINT32_MAX || 
		!BlockNumberIsValid(int64_value) || 
		int64_value < 0)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				errmsg("invalid blockNum value")));
}

/*
 * Checking the buffer for correctness
 */
void
buffer_is_correct_check(Buffer buffer)
{
	if (buffer > NBuffers || buffer == InvalidBuffer)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				errmsg("buffer %d does not exist", buffer)));
}

/*
 * Checking the buffer for locality 
 */
void
buffer_is_not_local_check(Buffer buffer)
{
	if (BufferIsLocal(buffer))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				errmsg("this function only works with non-local buffers")));
}

/* 
 * Is block number out of range for relation? 
 */
void
block_num_not_exist_in_relation_check(Relation rel, ForkNumber forkNum, BlockNumber blockNum) 
{
	if (blockNum >= RelationGetNumberOfBlocksInFork(rel, forkNum))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				errmsg("block number %u is out of range for relation \"%s\"",
						blockNum, RelationGetRelationName(rel))));
}

/*
 * Checking the forkNum for correctness
 */
void
fork_num_correct_check(ForkNumber forkNum)
{
	if (forkNum <= InvalidForkNumber || forkNum > MAX_FORKNUM)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				errmsg("Invalid fork value")));
}

/*-------------------------------------------------------------------------
 * 					Wrapper and buffer processing functions
 *-------------------------------------------------------------------------
 */

/*
 * Wrapper function for buffer processiong functions
 */
static void
BufProcFuncWrapper(BufProcFunc buf_proc_func, Buffer buffer, NullableDatum *bpf_args)
{
	switch(buf_proc_func)	
	{
		case BCT_MARK_DIRTY:
			MarkBufferDirty(buffer);
			break;
		case BCT_FLUSH:
			FlushOneBuffer(buffer);
			break;
		case BCT_CHANGE_SPCOID:
			Oid spcOid = DatumGetObjectId(bpf_args[0].value);

			ChangeSpcOidBuffer(buffer, spcOid);
			break;
		case BCT_CHANGE_DBOID:
			Oid dbOid = DatumGetObjectId(bpf_args[0].value);

			ChangeDBOidBuffer(buffer, dbOid);
			break;
		case BCT_CHANGE_RELNUMBER:
			Oid relNumber = (Oid) DatumGetObjectId(bpf_args[0].value);

			ChangeRelNumberBuffer(buffer, relNumber);
			break;
		case BCT_CHANGE_FORKNUM:
    		ForkNumber  forkNum = (ForkNumber) bpf_args[0].value;

			fork_num_correct_check(forkNum);

			ChangeForkNumBuffer(buffer, forkNum);
			break;
		case BCT_CHANGE_BLOCKNUM:
			BlockNumber blockNum;

			int64_to_block_number_convert_check(DatumGetInt64(bpf_args[0].value));
			blockNum = DatumGetUInt32(bpf_args[0].value);

			ChangeBlockNumBuffer(buffer, blockNum);
			break;
		default:
			Assert(false);
	}
}

/*
 * Change spcOid of buffer 
 */
static void
ChangeSpcOidBuffer(Buffer buffer, Oid spcOid)
{
	BufferDesc 	*bufHdr;
	uint32 		bufState;

	bufHdr = GetBufferDescriptor(buffer - 1);

	bufState = LockBufHdr(bufHdr);

	START_CRIT_SECTION();

#ifdef HAVE_RELFILENUMBERMAP_H
	bufHdr->tag.rnode.spcNode = spcOid;
#else
	bufHdr->tag.spcOid = spcOid;
#endif  /* HAVE_RELFILENUMBERMAP_H */

	END_CRIT_SECTION();
	
	UnlockBufHdr(bufHdr, bufState);
}

/*
 * Change dbOid of buffer 
 */
static void
ChangeDBOidBuffer(Buffer buffer, Oid dbOid)
{
	BufferDesc 	*bufHdr;
	uint32 		bufState;

	bufHdr = GetBufferDescriptor(buffer - 1);

	bufState = LockBufHdr(bufHdr);

	START_CRIT_SECTION();

#ifdef HAVE_RELFILENUMBERMAP_H
	bufHdr->tag.rnode.dbNode = dbOid;
#else
	bufHdr->tag.dbOid = dbOid;
#endif  /* HAVE_RELFILENUMBERMAP_H */

	END_CRIT_SECTION();
	
	UnlockBufHdr(bufHdr, bufState);
}

#ifdef HAVE_RELFILENUMBERMAP_H
static void ChangeRelNumberBuffer(Buffer buffer, Oid relNumber);
#else
static void ChangeRelNumberBuffer(Buffer buffer, RelFileNumber relNumber);
#endif  /* HAVE_RELFILENUMBERMAP_H */

/*
 * Change relNumber of buffer 
 */
#ifdef HAVE_RELFILENUMBERMAP_H
static void
ChangeRelNumberBuffer(Buffer buffer, Oid relNumber)
#else
static void
ChangeRelNumberBuffer(Buffer buffer, RelFileNumber relNumber)
#endif  /* HAVE_RELFILENUMBERMAP_H */
{
	BufferDesc 	*bufHdr;
	uint32 		bufState;

	bufHdr = GetBufferDescriptor(buffer - 1);

	bufState = LockBufHdr(bufHdr);

	START_CRIT_SECTION();

#ifdef HAVE_RELFILENUMBERMAP_H
	bufHdr->tag.rnode.relNode = relNumber;
#else
	bufHdr->tag.relNumber = relNumber;
#endif  /* HAVE_RELFILENUMBERMAP_H */

	END_CRIT_SECTION();
	
	UnlockBufHdr(bufHdr, bufState);
}

/*
 * Change forkNum of buffer 
 */
static void
ChangeForkNumBuffer(Buffer buffer, ForkNumber forkNum)
{
	BufferDesc 	*bufHdr;
	uint32 		bufState;

	bufHdr = GetBufferDescriptor(buffer - 1);

	bufState = LockBufHdr(bufHdr);

	START_CRIT_SECTION();

	bufHdr->tag.forkNum = forkNum;

	END_CRIT_SECTION();
	
	UnlockBufHdr(bufHdr, bufState);
}

/*
 * Change blockNum of buffer 
 */
static void
ChangeBlockNumBuffer(Buffer buffer, BlockNumber blockNum)
{
	BufferDesc 	*bufHdr;
	uint32 		bufState;

	bufHdr = GetBufferDescriptor(buffer - 1);

	bufState = LockBufHdr(bufHdr);

	START_CRIT_SECTION();

	bufHdr->tag.blockNum = blockNum;

	END_CRIT_SECTION();
	
	UnlockBufHdr(bufHdr, bufState);
}

/*-------------------------------------------------------------------------
 * 								Handler functions
 *-------------------------------------------------------------------------
 */

/*
 * One buffer handler
 */
void
one_buffer_handler(BufProcFunc buf_proc_func, Buffer buffer, 
				   NullableDatum *bpf_args)
{
	buffer_is_correct_check(buffer);
	buffer_is_not_local_check(buffer);

	LockBuffer(buffer, BUFFER_LOCK_EXCLUSIVE);
	BufProcFuncWrapper(buf_proc_func, buffer, bpf_args); 
	LockBuffer(buffer, BUFFER_LOCK_UNLOCK);
}

/*
 * Relation fork buffers handler
 */
void
relation_fork_buffers_handler(BufProcFunc buf_proc_func, 
								 text *relName, text *forkName, 
								 NullableDatum *bpf_args)
{
	Buffer 		i;
	BufferDesc 	*bufHdr;
	uint32 		bufState;

	Relation 	rel;
	RangeVar 	*relrv;
	ForkNumber 	forkNum; 

	/* Open relation */
	relrv = makeRangeVarFromNameList(textToQualifiedNameList(relName));	
	rel = relation_openrv(relrv, AccessExclusiveLock);

	other_temp_check(rel);

	forkNum = forkname_to_number(text_to_cstring(forkName));	

	/* Iterate over all non-local buffers */
	for (i = 1; i <= NBuffers; i++)
	{
		bufHdr = GetBufferDescriptor(i - 1);
		bufState = LockBufHdr(bufHdr);

		if (BCT_IS_BUFFER_BELONGS_RELATION(bufHdr, rel) && 
			BCT_IS_BUFFER_BELONGS_FORK(bufHdr, forkNum))
		{
			LockBuffer(i, BUFFER_LOCK_EXCLUSIVE);
			UnlockBufHdr(bufHdr, bufState);
			BufProcFuncWrapper(buf_proc_func, (Buffer) i, bpf_args);
			LockBuffer(i, BUFFER_LOCK_UNLOCK);			
		}
		else
		{
			UnlockBufHdr(bufHdr, bufState);
		}
	}

	/* Close relation */
	relation_close(rel, AccessExclusiveLock);
}

/*
 * Relation buffers handler
 */
void
relation_buffers_handler(BufProcFunc buf_proc_func, 
							text *relName, NullableDatum *bpf_args)
{
	Buffer 		i;
	BufferDesc 	*bufHdr;
	uint32 		bufState;

	Relation rel;
	RangeVar *relrv;

	/* Open relation */
	relrv = makeRangeVarFromNameList(textToQualifiedNameList(relName));	
	rel = relation_openrv(relrv, AccessExclusiveLock);

	other_temp_check(rel);

	/* Iterate over all non-local buffers */
	for (i = 1; i <= NBuffers; i++)
	{
		bufHdr = GetBufferDescriptor(i - 1);
		bufState = LockBufHdr(bufHdr);

		if (BCT_IS_BUFFER_BELONGS_RELATION(bufHdr, rel))
		{
			LockBuffer(i, BUFFER_LOCK_EXCLUSIVE);
			UnlockBufHdr(bufHdr, bufState);
			BufProcFuncWrapper(buf_proc_func, (Buffer) i, bpf_args);
			LockBuffer(i, BUFFER_LOCK_UNLOCK);			
		}
		else 
		{
			UnlockBufHdr(bufHdr, bufState);
		}
	}

	/* Close relation */
	relation_close(rel, AccessExclusiveLock);
}

/*
 * Database buffers handler
 */
void
database_buffers_handler(BufProcFunc buf_proc_func, 
						 Oid dbOid, NullableDatum *bpf_args)
{
	Buffer 		i;
	BufferDesc 	*bufHdr;
	uint32 		bufState;

	/* Iterate over all non-local buffers */
	for (i = 1; i <= NBuffers; i++)
	{
		bufHdr = GetBufferDescriptor(i - 1);
		bufState = LockBufHdr(bufHdr);

		if (BCT_IS_BUFFER_BELONGS_DATABASE(bufHdr, dbOid))
		{
			LockBuffer(i, BUFFER_LOCK_EXCLUSIVE);
			UnlockBufHdr(bufHdr, bufState);
			BufProcFuncWrapper(buf_proc_func, (Buffer) i, bpf_args);
			LockBuffer(i, BUFFER_LOCK_UNLOCK);			
		}
		else
		{
			UnlockBufHdr(bufHdr, bufState);
		}
	}
}

/*
 * Tablespace buffers handler
 */
void
tablespace_buffers_handler(BufProcFunc buf_proc_func, 
						   Oid spcOid, NullableDatum *bpf_args)
{
	Buffer 		i;
	BufferDesc 	*bufHdr;
	uint32 		bufState;

	/* Iterate over all non-local buffers */
	for (i = 1; i <= NBuffers; i++)
	{
		bufHdr = GetBufferDescriptor(i - 1);
		bufState = LockBufHdr(bufHdr);

		if (BCT_IS_BUFFER_BELONGS_TABLESPACE(bufHdr, spcOid))
		{
			LockBuffer(i, BUFFER_LOCK_EXCLUSIVE);
			UnlockBufHdr(bufHdr, bufState);
			BufProcFuncWrapper(buf_proc_func, (Buffer) i, bpf_args);
			LockBuffer(i, BUFFER_LOCK_UNLOCK);			
		}
		else
		{
			UnlockBufHdr(bufHdr, bufState);
		}
	}
}

/*
 * All valid buffers handler
 */
void
all_valid_buffers_handler(BufProcFunc buf_proc_func, NullableDatum *bpf_args)
{
	Buffer 		i;
	BufferDesc 	*bufHdr;
	uint32 		bufState;

	/* Iterate over all non-local buffers */
	for (i = 1; i <= NBuffers; i++)
	{
		bufHdr = GetBufferDescriptor(i - 1);
		bufState = LockBufHdr(bufHdr);

		if (BUFFER_IS_VALID(bufState))
		{
			LockBuffer(i, BUFFER_LOCK_EXCLUSIVE);
			UnlockBufHdr(bufHdr, bufState);
			BufProcFuncWrapper(buf_proc_func, (Buffer) i, bpf_args);
			LockBuffer(i, BUFFER_LOCK_UNLOCK);			
		}
		else
		{
			UnlockBufHdr(bufHdr, bufState);
		}
	}
}

void
pg_show_buffer_internals(FunctionCallInfo fcinfo, Buffer buffer)
{
	BufferDesc 	*bufHdr;
	uint32 		bufState;

	ReturnSetInfo 	*rsinfo = (ReturnSetInfo *) fcinfo->resultinfo;
	TupleDesc 		tupdesc;
	Tuplestorestate *tupstore;
	Datum			values[8];
	bool 			nulls[8] = {0};

	MemoryContext per_query_ctx;
	MemoryContext oldcontext;

	/*
	 * Reading local buffer is currently not supperted for pg_show_buffer()
	 */
	buffer_is_not_local_check(buffer);

	buffer_is_correct_check(buffer);

	per_query_ctx = rsinfo->econtext->ecxt_per_query_memory;
	oldcontext = MemoryContextSwitchTo(per_query_ctx);

	/* get the requested return tuple description */
	tupdesc = CreateTupleDescCopy(rsinfo->expectedDesc);

	/* let the caller know we're sending back a tuplestore */
	rsinfo->returnMode = SFRM_Materialize;

	tupstore = tuplestore_begin_heap(true, false, work_mem);

	bufHdr = GetBufferDescriptor(buffer - 1);
	bufState = LockBufHdr(bufHdr);

	if (BUFFER_IS_VALID(bufState))
	{
		values[0] = (Datum) bufHdr->tag.blockNum;
		values[1] = CStringGetTextDatum(forkNames[bufHdr->tag.forkNum]);	
#ifdef HAVE_RELFILENUMBERMAP_H
		values[2] = (Datum) bufHdr->tag.rnode.relNode;
		values[3] = (Datum) bufHdr->tag.rnode.dbNode;
		values[4] = (Datum) bufHdr->tag.rnode.spcNode;
#else
		values[2] = (Datum) bufHdr->tag.relNumber;
		values[3] = (Datum) bufHdr->tag.dbOid;
		values[4] = (Datum) bufHdr->tag.spcOid;
#endif  /* HAVE_RELFILENUMBERMAP_H */
		values[5] = BoolGetDatum((bool) (bufState & BM_DIRTY));
		values[6] = (Datum) BUF_STATE_GET_USAGECOUNT(bufState); 
		values[7] = (Datum) BUF_STATE_GET_REFCOUNT(bufState);
	}
	else
	{
		nulls[0] = true;
		nulls[1] = true;
		nulls[2] = true;
		nulls[3] = true;
		nulls[4] = true;
		nulls[5] = true;
		nulls[6] = true;
		nulls[7] = true;
	}

	UnlockBufHdr(bufHdr, bufState);

	if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
			elog(ERROR, "return type must be a row type");
	
	tuplestore_putvalues(tupstore, tupdesc, values, nulls);


	/*
	 * no longer need the tuple descriptor reference created by
	 * TupleDescGetAttInMetadata()
	 */
	buffer_is_not_local_check(buffer);

	buffer_is_correct_check(buffer);

	ReleaseTupleDesc(tupdesc);

	tuplestore_donestoring(tupstore);
	rsinfo->setResult = tupstore;

	/*
	 * SFRM_Materialize mode expects us to return a NULL Datum. The actual
	 * tuples are in our tuplestore and passed back through rsinfo->setResult.
	 * rsinfo->setDesc is set to the tuple description that we actually used
	 * to build our tuples with, so the caller can verify we did what it was
	 * expecting.
	 */
	rsinfo->setDesc = tupdesc;

	MemoryContextSwitchTo(oldcontext);
}

/*
 * Show buffers from the buffer cache that belong to 
 * a specific relation  
 */
void
pg_show_relation_buffers_internals(FunctionCallInfo fcinfo, text *relname)
{
	Buffer 		i;
	BufferDesc 	*bufHdr;
	uint32 		bufState;

	Relation rel;
	RangeVar *relrv;

	ReturnSetInfo 	*rsinfo = (ReturnSetInfo *) fcinfo->resultinfo;
	TupleDesc 		tupdesc;
	Tuplestorestate *tupstore;
	Datum			values[6];
	bool 			nulls[6] = {0};

	MemoryContext per_query_ctx;
	MemoryContext oldcontext;

	per_query_ctx = rsinfo->econtext->ecxt_per_query_memory;
	oldcontext = MemoryContextSwitchTo(per_query_ctx);

	/* get the requested return tuple description */
	tupdesc = CreateTupleDescCopy(rsinfo->expectedDesc);

	/* let the caller know we're sending back a tuplestore */
	rsinfo->returnMode = SFRM_Materialize;

	tupstore = tuplestore_begin_heap(true, false, work_mem);

	/* Open relation */
	relrv = makeRangeVarFromNameList(textToQualifiedNameList(relname));	
	rel = relation_openrv(relrv, AccessExclusiveLock);

	other_temp_check(rel);

	if (RelationUsesLocalBuffers(rel))
	{
		/* 
		 *Iterate over all local buffers 
		 */
		for (i = 0; i < NLocBuffer; i++)
		{
			bufHdr = GetLocalBufferDescriptor(i);
			bufState = LockBufHdr(bufHdr);

			if (BCT_IS_BUFFER_BELONGS_RELATION(bufHdr, rel))
			{
				values[0] = (Datum) BufferDescriptorGetBuffer(bufHdr);
				values[1] = (Datum) bufHdr->tag.blockNum;
				values[2] = BoolGetDatum((bool) (bufState & BM_DIRTY));
				values[3] = (Datum) BUF_STATE_GET_USAGECOUNT(bufState); 
				values[4] = (Datum) BUF_STATE_GET_REFCOUNT(bufState);
				values[5] = CStringGetTextDatum(forkNames[bufHdr->tag.forkNum]);	

				if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
					elog(ERROR, "return type must be a row type");
				
				tuplestore_putvalues(tupstore, tupdesc, values, nulls);
			}

			UnlockBufHdr(bufHdr, bufState);
		}
	}
	else
	{
		/* 
		 * Iterate over all non-local buffers 
		 */
		for (i = 0; i < NBuffers; i++)
		{
			bufHdr = GetBufferDescriptor(i);
			bufState = LockBufHdr(bufHdr);

			if (BCT_IS_BUFFER_BELONGS_RELATION(bufHdr, rel))
			{
				values[0] = (Datum) BufferDescriptorGetBuffer(bufHdr);
				values[1] = (Datum) bufHdr->tag.blockNum;
				values[2] = BoolGetDatum((bool) (bufState & BM_DIRTY));
				values[3] = (Datum) BUF_STATE_GET_USAGECOUNT(bufState); 
				values[4] = (Datum) BUF_STATE_GET_REFCOUNT(bufState);
				values[5] = CStringGetTextDatum(forkNames[bufHdr->tag.forkNum]);	

				if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
						elog(ERROR, "return type must be a row type");
				
				tuplestore_putvalues(tupstore, tupdesc, values, nulls);
			}

			UnlockBufHdr(bufHdr, bufState);
		}
	}

	/*
	 * no longer need the tuple descriptor reference created by
	 * TupleDescGetAttInMetadata()
	 */
	ReleaseTupleDesc(tupdesc);

	tuplestore_donestoring(tupstore);
	rsinfo->setResult = tupstore;

	/*
	 * SFRM_Materialize mode expects us to return a NULL Datum. The actual
	 * tuples are in our tuplestore and passed back through rsinfo->setResult.
	 * rsinfo->setDesc is set to the tuple description that we actually used
	 * to build our tuples with, so the caller can verify we did what it was
	 * expecting.
	 */
	rsinfo->setDesc = tupdesc;

	MemoryContextSwitchTo(oldcontext);

	/* Close relation*/
	relation_close(rel, AccessExclusiveLock);
}

/*
 * Read a specific page of a specific relation into the buffer cache
 */
Buffer	
pg_read_page_into_buffer_internals(text *relName, text *forkName, BlockNumber blockNum)
{
	ForkNumber 	forkNum; 
	Buffer		readBuf;	

	Relation rel;
	RangeVar *relrv;

	superuser_check();

	/* Open relation */
	relrv = makeRangeVarFromNameList(textToQualifiedNameList(relName));	
	rel = relation_openrv(relrv, AccessExclusiveLock);

	forkNum = forkname_to_number(text_to_cstring(forkName));	

	other_temp_check(rel);

	/* Is block number out of range for relation? */
	block_num_not_exist_in_relation_check(rel, forkNum, blockNum);

	readBuf = ReadBufferExtended(rel, forkNum, blockNum, RBM_NORMAL, NULL);

	/* After the ReadBuffer function we need to release the buffer*/
	ReleaseBuffer(readBuf);

	/* Close relation*/
	relation_close(rel, AccessExclusiveLock);

    return readBuf;
}