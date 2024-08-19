/*-------------------------------------------------------------------------
 *
 * buffercache_tools.c
 * 		Functions for manipulating the buffer cache 
 *
 * IDENTIFICATION
 *	  buffercache_tools.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/relation.h"
#include "catalog/namespace.h"
#include "catalog/pg_database.h"
#include "common/relpath.h"
#include "funcapi.h"
#include "nodes/execnodes.h"
#include "storage/bufmgr.h"
#include "storage/buf_internals.h"
#include "storage/lmgr.h"
#include "utils/builtins.h"
#include "utils/relcache.h"
#include "utils/tuplestore.h"
#include "utils/varlena.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(pg_mark_buffer_dirty);
PG_FUNCTION_INFO_V1(pg_flush_buffer);

PG_FUNCTION_INFO_V1(pg_mark_relation_fork_buffers_dirty);
PG_FUNCTION_INFO_V1(pg_flush_relation_fork_buffers);

PG_FUNCTION_INFO_V1(pg_mark_relation_buffers_dirty);
PG_FUNCTION_INFO_V1(pg_flush_relation_buffers);

PG_FUNCTION_INFO_V1(pg_mark_database_buffers_dirty);
PG_FUNCTION_INFO_V1(pg_flush_database_buffers);

PG_FUNCTION_INFO_V1(pg_mark_tablespace_buffers_dirty);
PG_FUNCTION_INFO_V1(pg_flush_tablespace_buffers);


PG_FUNCTION_INFO_V1(pg_show_relation_buffers);
PG_FUNCTION_INFO_V1(pg_read_page_into_buffer);

/*
 * Macroses for the last argument of handler functions.
 *
 * BCT_MARK_DIRTY - Mark postgres object dirty
 * 
 * BCT_FLUSH - Flush postgres object
 */
#ifndef BCT_MARK_DIRTY
	#define BCT_MARK_DIRTY MarkBufferDirty
#else
	Assert(false);
#endif 

#ifndef BCT_FLUSH
	#define BCT_FLUSH FlushOneBuffer
#else
	Assert(false);
#endif

/*-------------------------------------------------------------------------
 * 								Headers 
 *-------------------------------------------------------------------------
 */
static void pg_one_buffer_handler(Buffer bufNum, void (* BufferFunc)(Buffer buffer));

static void pg_relation_fork_buffers_handler(text *relName, text *forkName, 
											void (* BufferFunc)(Buffer buffer));

static void pg_relation_buffers_handler(text *relName, void (* BufferFunc)(Buffer buffer));

static void pg_database_buffers_handler(Oid dbOid, void (* BufferFunc)(Buffer buffer));

static void pg_tablespace_buffers_handler(Oid spcOid, void (* BufferFunc)(Buffer buffer));

static void pg_superuser_check(void);

static void pg_other_temp_check(Relation rel);

/*-------------------------------------------------------------------------
 * 								Check functions
 *-------------------------------------------------------------------------
 */

void
pg_superuser_check(void)
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
pg_other_temp_check(Relation rel)
{
	if (RELATION_IS_OTHER_TEMP(rel))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				errmsg("cannot access temporary tables of other sessions")));
}

/*-------------------------------------------------------------------------
 * 								Handlers 
 *-------------------------------------------------------------------------
 */

/*
 * One buffer handler
 */
void
pg_one_buffer_handler(Buffer bufNum, void (* BufferFunc)(Buffer buffer))
{
	if (bufNum > NBuffers || bufNum <= InvalidBuffer)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				errmsg("buffer %d does not exist", bufNum)));

	LockBuffer(bufNum, BUFFER_LOCK_EXCLUSIVE);
	BufferFunc(bufNum);
	LockBuffer(bufNum, BUFFER_LOCK_UNLOCK);
}

/*
 * Relation fork buffers handler
 */
void
pg_relation_fork_buffers_handler(text *relName, text *forkName, 
								void (* BufferFunc)(Buffer buffer))
{
	Buffer i;
	BufferDesc *bufHdr;
	uint32 bufState;

	Relation rel;
	RangeVar *relrv;
	ForkNumber forkNum; 

	relrv = makeRangeVarFromNameList(textToQualifiedNameList(relName));	
	rel = relation_openrv(relrv, AccessExclusiveLock);

	pg_other_temp_check(rel);

	forkNum = forkname_to_number(text_to_cstring(forkName));	

	/* Iterate over all non-local buffers */
	for (i = 1; i <= NBuffers; i++)
	{
		bufHdr = GetBufferDescriptor(i - 1);
		bufState = LockBufHdr(bufHdr);

		if (BufTagMatchesRelFileLocator(&bufHdr->tag, &rel->rd_locator) &&
			bufHdr->tag.forkNum == forkNum)
		{
			LockBuffer(i, BUFFER_LOCK_EXCLUSIVE);
			UnlockBufHdr(bufHdr, bufState);
			BufferFunc(i);
			LockBuffer(i, BUFFER_LOCK_UNLOCK);			
		}
		else
		{
			UnlockBufHdr(bufHdr, bufState);
		}
	}

	relation_close(rel, AccessExclusiveLock);
}

/*
 * Relation buffers handler
 */
void
pg_relation_buffers_handler(text *relName, void (* BufferFunc)(Buffer buffer))
{
	Buffer i;
	BufferDesc *bufHdr;
	uint32 bufState;

	Relation rel;
	RangeVar *relrv;

	relrv = makeRangeVarFromNameList(textToQualifiedNameList(relName));	
	rel = relation_openrv(relrv, AccessExclusiveLock);

	pg_other_temp_check(rel);

	/* Iterate over all non-local buffers */
	for (i = 1; i <= NBuffers; i++)
	{
		bufHdr = GetBufferDescriptor(i - 1);
		bufState = LockBufHdr(bufHdr);

		if (BufTagMatchesRelFileLocator(&bufHdr->tag, &rel->rd_locator))
		{
			LockBuffer(i, BUFFER_LOCK_EXCLUSIVE);
			UnlockBufHdr(bufHdr, bufState);
			BufferFunc(i);
			LockBuffer(i, BUFFER_LOCK_UNLOCK);			
		}
		else 
		{
			UnlockBufHdr(bufHdr, bufState);
		}
	}

	relation_close(rel, AccessExclusiveLock);
}

/*
 * Database buffers handler
 */
void
pg_database_buffers_handler(Oid dbOid, void (* BufferFunc)(Buffer buffer))
{
	Buffer i;
	BufferDesc *bufHdr;
	uint32 bufState;

	/* Iterate over all non-local buffers */
	for (i = 1; i <= NBuffers; i++)
	{
		bufHdr = GetBufferDescriptor(i - 1);
		bufState = LockBufHdr(bufHdr);

		if (bufHdr->tag.dbOid == dbOid)
		{
			LockBuffer(i, BUFFER_LOCK_EXCLUSIVE);
			UnlockBufHdr(bufHdr, bufState);
			BufferFunc(i);
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
pg_tablespace_buffers_handler(Oid spcOid, void (* BufferFunc)(Buffer buffer))
{
	Buffer i;
	BufferDesc *bufHdr;
	uint32 bufState;

	/* Iterate over all non-local buffers */
	for (i = 1; i <= NBuffers; i++)
	{
		bufHdr = GetBufferDescriptor(i - 1);
		bufState = LockBufHdr(bufHdr);

		if (bufHdr->tag.spcOid == spcOid)
		{
			LockBuffer(i, BUFFER_LOCK_EXCLUSIVE);
			UnlockBufHdr(bufHdr, bufState);
			BufferFunc(i);
			LockBuffer(i, BUFFER_LOCK_UNLOCK);			
		}
		else
		{
			UnlockBufHdr(bufHdr, bufState);
		}
	}
}

/*-------------------------------------------------------------------------
 * 								extension functions	 
 *-------------------------------------------------------------------------
 */

/*
 * Mark buffer dirty
 */
Datum
pg_mark_buffer_dirty(PG_FUNCTION_ARGS)
{
	Buffer	bufNum = PG_GETARG_INT32(0);

	pg_superuser_check();

	pg_one_buffer_handler(bufNum, BCT_MARK_DIRTY);

	PG_RETURN_BOOL(true);
}

/*
 * Write (flush) buffer page to disk without drop
 */
Datum
pg_flush_buffer(PG_FUNCTION_ARGS)
{
	Buffer	bufNum = PG_GETARG_INT32(0);

	pg_superuser_check();

	pg_one_buffer_handler(bufNum, BCT_FLUSH);

	PG_RETURN_BOOL(true);
}

/*
 * Mark relation buffer pages dirty of a specific fork 
 */
Datum
pg_mark_relation_fork_buffers_dirty(PG_FUNCTION_ARGS) 
{
	text *relName = PG_GETARG_TEXT_PP(0);
	text *forkName = PG_GETARG_TEXT_PP(1);

	pg_superuser_check();

	pg_relation_fork_buffers_handler(relName, forkName, BCT_MARK_DIRTY);

	PG_RETURN_BOOL(true);
}

/*
 * Write relation buffer pages of a specific fork 
 * to disk without drop
 */
Datum
pg_flush_relation_fork_buffers(PG_FUNCTION_ARGS) 
{
	text *relName = PG_GETARG_TEXT_PP(0);
	text *forkName = PG_GETARG_TEXT_PP(1);

	pg_superuser_check();

	pg_relation_fork_buffers_handler(relName, forkName, BCT_FLUSH);

	PG_RETURN_BOOL(true);
}

/*
 * Mark relation buffer pages dirty  
 */
Datum
pg_mark_relation_buffers_dirty(PG_FUNCTION_ARGS) 
{
	text *relName = PG_GETARG_TEXT_PP(0);

	pg_superuser_check();

	pg_relation_buffers_handler(relName, BCT_MARK_DIRTY);

	PG_RETURN_BOOL(true);
}

/*
 * Write relation buffer pages to disk without drop 
 */
Datum
pg_flush_relation_buffers(PG_FUNCTION_ARGS) 
{
	text *relName = PG_GETARG_TEXT_PP(0);

	pg_superuser_check();

	pg_relation_buffers_handler(relName, BCT_FLUSH);

	PG_RETURN_BOOL(true);
}

/*
 * Mark all database buffer pages dirty 
 */
Datum 
pg_mark_database_buffers_dirty(PG_FUNCTION_ARGS)
{
	Oid dbOid = PG_GETARG_OID(0);	
	
	pg_superuser_check();

	if (database_is_invalid_oid(dbOid)) 
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				errmsg("invalid database oid")));

	pg_database_buffers_handler(dbOid, BCT_MARK_DIRTY);

	PG_RETURN_BOOL(true);
}

/*
 * Write all database buffer pages to disk without drop 
 */
Datum 
pg_flush_database_buffers(PG_FUNCTION_ARGS)
{
	Oid dbOid = PG_GETARG_OID(0);	
	
	pg_superuser_check();

	if (database_is_invalid_oid(dbOid)) 
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				errmsg("invalid database oid")));

	pg_database_buffers_handler(dbOid, BCT_FLUSH);

	PG_RETURN_BOOL(true);
}

/*
 * Mark all tablespace buffer pages drity 
 */
Datum 
pg_mark_tablespace_buffers_dirty(PG_FUNCTION_ARGS)
{
	Oid spcOid = PG_GETARG_OID(0);	
	
	pg_superuser_check();

	pg_tablespace_buffers_handler(spcOid, BCT_MARK_DIRTY);

	PG_RETURN_BOOL(true);
}

/*
 * Write all tablespace buffer pages to disk without drop 
 */
Datum 
pg_flush_tablespace_buffers(PG_FUNCTION_ARGS)
{
	Oid spcOid = PG_GETARG_OID(0);	
	
	pg_superuser_check();

	pg_tablespace_buffers_handler(spcOid, BCT_FLUSH);

	PG_RETURN_BOOL(true);
}

/*
 * Show buffers from the buffer cache that belong to 
 * a specific relation  
 */
Datum
pg_show_relation_buffers(PG_FUNCTION_ARGS) 
{
	Buffer i;
	BufferDesc *bufHdr;
	uint32 bufState;

	Relation rel;
	RangeVar *relrv;

	text *relname = PG_GETARG_TEXT_PP(0);

	TupleDesc tupdesc;
	ReturnSetInfo *rsinfo = (ReturnSetInfo *) fcinfo->resultinfo;
	Datum	values[6];
	bool nulls[6] = {0};

	pg_superuser_check();

	InitMaterializedSRF(fcinfo, 0);

	relrv = makeRangeVarFromNameList(textToQualifiedNameList(relname));	
	rel = relation_openrv(relrv, AccessExclusiveLock);

	pg_other_temp_check(rel);

	if (RelationUsesLocalBuffers(rel))
	{
		/*  
		 * Show temporary table buffers
		 */

		/* Iterate over all local buffers */
		for (i = 0; i < NLocBuffer; i++)
		{
			bufHdr = GetLocalBufferDescriptor(i);

			bufState = LockBufHdr(bufHdr);
			if (BufTagMatchesRelFileLocator(&bufHdr->tag, &rel->rd_locator))
			{

				values[0] = (Datum) bufHdr->tag.blockNum;
				values[1] = (Datum) BufferDescriptorGetBuffer(bufHdr);
				values[2] = BoolGetDatum((bool) (bufState & BM_DIRTY));
				values[3] = (Datum) BUF_STATE_GET_USAGECOUNT(bufState); 
				values[4] = (Datum) BUF_STATE_GET_REFCOUNT(bufState);
				values[5] = CStringGetTextDatum(forkNames[bufHdr->tag.forkNum]);	

				if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
						elog(ERROR, "return type must be a row type");
				
				tuplestore_putvalues(rsinfo->setResult, rsinfo->setDesc, values, nulls);
			}

			UnlockBufHdr(bufHdr, bufState);
		}
	}
	else
	{
		/* Iterate over all non-local buffers */
		for (i = 0; i < NBuffers; i++)
		{
			bufHdr = GetBufferDescriptor(i);

			bufState = LockBufHdr(bufHdr);
			if (BufTagMatchesRelFileLocator(&bufHdr->tag, &rel->rd_locator))
			{

				values[0] = (Datum) bufHdr->tag.blockNum;
				values[1] = (Datum) BufferDescriptorGetBuffer(bufHdr);
				values[2] = BoolGetDatum((bool) (bufState & BM_DIRTY));
				values[3] = (Datum) BUF_STATE_GET_USAGECOUNT(bufState); 
				values[4] = (Datum) BUF_STATE_GET_REFCOUNT(bufState);
				values[5] = CStringGetTextDatum(forkNames[bufHdr->tag.forkNum]);	

				if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
						elog(ERROR, "return type must be a row type");
				
				tuplestore_putvalues(rsinfo->setResult, rsinfo->setDesc, values, nulls);
			}

			UnlockBufHdr(bufHdr, bufState);
		}
	}

	relation_close(rel, AccessExclusiveLock);

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
	BlockNumber blockNum = PG_GETARG_INT32(2);
	ForkNumber forkNum; 
	Buffer	readBuf;	

	Relation rel;
	RangeVar *relrv;

	pg_superuser_check();

	relrv = makeRangeVarFromNameList(textToQualifiedNameList(relName));	
	rel = relation_openrv(relrv, AccessExclusiveLock);

	forkNum = forkname_to_number(text_to_cstring(forkName));	

	pg_other_temp_check(rel);

	/* Is block number out of range for relation? */
	if (blockNum >= RelationGetNumberOfBlocksInFork(rel, forkNum))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				errmsg("block number %u is out of range for relation \"%s\"",
						blockNum, RelationGetRelationName(rel))));

	readBuf = ReadBufferExtended(rel, forkNum, blockNum, RBM_NORMAL, NULL);

	ReleaseBuffer(readBuf);

	relation_close(rel, AccessExclusiveLock);

	PG_RETURN_INT32((int32) readBuf);
}
