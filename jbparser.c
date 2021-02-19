/*-------------------------------------------------------------------------
*
* zhparser.c
*	  a text search parser for Chinese
*
*-------------------------------------------------------------------------
*/
#include "jbparser.h"
#include "postgres.h"
#include "miscadmin.h"
#include "fmgr.h"
#include "utils/builtins.h"

PG_MODULE_MAGIC;

PGDLLEXPORT void _PG_init(void);
PGDLLEXPORT void _PG_fini(void);

/*
* prototypes
*/
PGDLLEXPORT Datum		jbprs_start(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(jbprs_start);

PGDLLEXPORT Datum		jbprs_start_q(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(jbprs_start_q);

PGDLLEXPORT Datum		jbprs_getlexeme(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(jbprs_getlexeme);

PGDLLEXPORT Datum		jbprs_end(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(jbprs_end);

PGDLLEXPORT Datum		jbprs_lextype(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(jbprs_lextype);

PGDLLEXPORT Datum		jiebaparser_reset(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(jiebaparser_reset);


static volatile bool gInited = false;
static void init(int forceReset) {
	char sharepath[MAXPGPATH] = { 0 };
	char dict_path[MAXPGPATH] = { 0 };
	
	get_share_path(my_exec_path, sharepath);
	snprintf(dict_path, MAXPGPATH, "%s/tsearch_data/jieba/",
		sharepath);
	gInited = parse_init(dict_path, forceReset) == 0;
	if (!gInited)
	{
		_log("parse_init error,dict_path:%s", dict_path);
		ereport(ERROR, (errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
			errmsg("parser init error"),
			errdetail(gErrMsg),
			errhint("please make sure dict path is correct"))
		);
	}
}

/*
* functions
*/

/*
* Module load callback
*/
void
_PG_init(void)
{
	if (!gInited)
		init(0);
}

/*
* Module unload callback
*/
void
_PG_fini(void)
{
	gInited = false;
	parse_uninit();
}

Datum
jiebaparser_reset(PG_FUNCTION_ARGS)
{
	parse_uninit();
	init(1);
	PG_RETURN_VOID();
}
Datum
jbprs_start(PG_FUNCTION_ARGS)
{
	if (!gInited)
		init(0);
	char* pstr = (char *)PG_GETARG_POINTER(0);
	int len = PG_GETARG_INT32(1);
	ParserState* pst = (ParserState *)palloc(sizeof(ParserState));
	init_parse_state(pst, pstr, len, 0);
	PG_RETURN_POINTER(pst);
}
Datum
jbprs_start_q(PG_FUNCTION_ARGS)
{
	if (!gInited)
		init(0);
	char* pstr = (char *)PG_GETARG_POINTER(0);
	int len = PG_GETARG_INT32(1);
	ParserState* pst = (ParserState *)palloc(sizeof(ParserState));
	init_parse_state(pst, pstr, len, 1);
	PG_RETURN_POINTER(pst);
}

Datum
jbprs_getlexeme(PG_FUNCTION_ARGS)
{
	ParserState *pst = (ParserState *)PG_GETARG_POINTER(0);
	char	  **t = (char **)PG_GETARG_POINTER(1);
	int		   *tlen = (int *)PG_GETARG_POINTER(2);
	int			type = -1;
	get_word(pst, t, tlen, &type);
	PG_RETURN_INT32(type);
}

Datum
jbprs_end(PG_FUNCTION_ARGS)
{
	ParserState *pst = (ParserState *)PG_GETARG_POINTER(0);
	end_parse(pst);
	pfree(pst);
	PG_RETURN_VOID();
}

Datum
jbprs_lextype(PG_FUNCTION_ARGS)
{
	if (!gInited)
		init(0);
	LexDescr * descr = (LexDescr *)palloc(sizeof(LexDescr) * (iLexDescr_size + 1));
	load_lextype(descr);
	PG_RETURN_POINTER(descr);
}
//TODO :headline function
