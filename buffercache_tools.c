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
#include "utils/relfilenumbermap.h"
#include "utils/tuplestore.h"
#include "utils/varlena.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(pg_show_relation_buffers);
PG_FUNCTION_INFO_V1(pg_read_page_into_buffer);
PG_FUNCTION_INFO_V1(pg_flush_buffer);
PG_FUNCTION_INFO_V1(pg_mark_buffer_dirty);
PG_FUNCTION_INFO_V1(pg_flush_relation_fork_buffers);
PG_FUNCTION_INFO_V1(pg_flush_database_buffers);
PG_FUNCTION_INFO_V1(pg_flush_relation_buffers);

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

	if (!superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				errmsg("must be superuser to show relation buffers")));	

	InitMaterializedSRF(fcinfo, 0);

	relrv = makeRangeVarFromNameList(textToQualifiedNameList(relname));	
	rel = relation_openrv(relrv, AccessExclusiveLock);

	/*
	 * Reject attempts to read non-local temporary relations; we would be
	 * likely to get wrong data since we have no visibility into the owning
	 * session's local buffers.
	 */
	if (RELATION_IS_OTHER_TEMP(rel))
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					errmsg("cannot access temporary tables of other sessions")));

	if (RelationUsesLocalBuffers(rel))
	{
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
	ForkNumber forkNum; 
	BlockNumber blockNum = PG_GETARG_INT32(2);
	Buffer	readBuf;	

	Relation rel;
	RangeVar *relrv;

	if (!superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				errmsg("must be superuser to use raw page functions")));

	relrv = makeRangeVarFromNameList(textToQualifiedNameList(relName));	
	rel = relation_openrv(relrv, AccessExclusiveLock);

	forkNum = forkname_to_number(text_to_cstring(forkName));	

	/*
	 * Reject attempts to read non-local temporary relations; we would be
	 * likely to get wrong data since we have no visibility into the owning
	 * session's local buffers.
	 */
	if (RELATION_IS_OTHER_TEMP(rel))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				errmsg("cannot access temporary tables of other sessions")));

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

/*
 * Write (flush) buffer page to disk without drop
 */
Datum
pg_flush_buffer(PG_FUNCTION_ARGS)
{
	Buffer	bufNum = PG_GETARG_INT32(0);

	if (bufNum > NBuffers || bufNum <= InvalidBuffer)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				errmsg("buffer %d does not exist", bufNum)));

	if (!superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				errmsg("must be superuser to flush buffers")));

	LockBuffer(bufNum, BUFFER_LOCK_EXCLUSIVE);
	FlushOneBuffer(bufNum);
	LockBuffer(bufNum, BUFFER_LOCK_UNLOCK);

	PG_RETURN_BOOL(true);
}

/*
 * Mark buffer dirty
 */
Datum
pg_mark_buffer_dirty(PG_FUNCTION_ARGS)
{
	Buffer	bufNum = PG_GETARG_INT32(0);

	if (bufNum > NBuffers || bufNum <= InvalidBuffer)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				errmsg("buffer %d does not exist", bufNum)));

	if (!superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				errmsg("must be superuser to flush buffers")));

	LockBuffer(bufNum, BUFFER_LOCK_EXCLUSIVE);
	MarkBufferDirty(bufNum);
	LockBuffer(bufNum, BUFFER_LOCK_UNLOCK);

	PG_RETURN_BOOL(true);
}

/*
 * Write relation buffer pages of a specific fork 
 * to disk without drop
 */
Datum
pg_flush_relation_fork_buffers(PG_FUNCTION_ARGS) 
{
	Buffer i;
	BufferDesc *bufHdr;
	uint32 bufState;

	Relation rel;
	RangeVar *relrv;
	ForkNumber forkNum; 

	text *relName = PG_GETARG_TEXT_PP(0);
	text *forkName = PG_GETARG_TEXT_PP(1);

	if (!superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				errmsg("must be superuser to flush buffers")));

	relrv = makeRangeVarFromNameList(textToQualifiedNameList(relName));	
	rel = relation_openrv(relrv, AccessExclusiveLock);

	forkNum = forkname_to_number(text_to_cstring(forkName));	

	for (i = 1; i <= NBuffers; i++)
	{
		
		bufHdr = GetBufferDescriptor(i - 1);
		bufState = LockBufHdr(bufHdr);

		if (BufTagMatchesRelFileLocator(&bufHdr->tag, &rel->rd_locator) &&
			bufHdr->tag.forkNum == forkNum)
		{
			LockBuffer(i, BUFFER_LOCK_EXCLUSIVE);
			UnlockBufHdr(bufHdr, bufState);
			FlushOneBuffer(i);
			bufState = LockBufHdr(bufHdr);
			LockBuffer(i, BUFFER_LOCK_UNLOCK);			
		}

		UnlockBufHdr(bufHdr, bufState);
	}
	relation_close(rel, AccessExclusiveLock);

	PG_RETURN_BOOL(true);
}

/*
 * Write relation buffer pages to disk without drop 
 */
Datum
pg_flush_relation_buffers(PG_FUNCTION_ARGS) 
{
	Buffer i;
	BufferDesc *bufHdr;
	uint32 bufState;

	Relation rel;
	RangeVar *relrv;

	text *relName = PG_GETARG_TEXT_PP(0);

	if (!superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				errmsg("must be superuser to flush buffers")));

	relrv = makeRangeVarFromNameList(textToQualifiedNameList(relName));	
	rel = relation_openrv(relrv, AccessExclusiveLock);

	for (i = 1; i <= NBuffers; i++)
	{
		
		bufHdr = GetBufferDescriptor(i - 1);
		bufState = LockBufHdr(bufHdr);

		if (BufTagMatchesRelFileLocator(&bufHdr->tag, &rel->rd_locator))
		{
			LockBuffer(i, BUFFER_LOCK_EXCLUSIVE);
			UnlockBufHdr(bufHdr, bufState);
			FlushOneBuffer(i);
			bufState = LockBufHdr(bufHdr);
			LockBuffer(i, BUFFER_LOCK_UNLOCK);			
		}

		UnlockBufHdr(bufHdr, bufState);
	}
	relation_close(rel, AccessExclusiveLock);

	PG_RETURN_BOOL(true);
}

/*
 * Write all database buffer pages to disk without drop 
 */
Datum 
pg_flush_database_buffers(PG_FUNCTION_ARGS)
{
	Oid dbOid = PG_GETARG_OID(0);	
	
	if (!superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				errmsg("must be superuser to flush database buffers")));

	if (database_is_invalid_oid(dbOid)) 
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				errmsg("invalid database oid")));

	FlushDatabaseBuffers(dbOid);

	PG_RETURN_BOOL(true);
}