#ifndef ZHPARSER_H 
#define ZHPARSER_H

typedef struct
{
	void* wordVct;
	int pos;
	int size;
} ParserState;


/* copy-paste from wparser.h of tsearch2 */
typedef struct
{
	int			lexid;
	char	   *alias;
	char	   *descr;
} LexDescr;

extern int iLexDescr_size;
void load_lextype(LexDescr* lxarr);
int parse_init(const char* base_path, int forceReset);
void parse_uninit();
int init_parse_state(ParserState* pst, const char* str, int len, int queryMode);
int get_word(ParserState* pst, char** ppstr, int* plen, int* type);
void end_parse(ParserState* pst);


void _log(const char *fmt, ...);

#define ERR_MSG_SZ 2000
extern char gErrMsg[ERR_MSG_SZ+2];


#endif
