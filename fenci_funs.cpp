

#include <memory>
#include <random>
#include <chrono>
#include <thread>
#include <stdarg.h>
#include <boost/container/vector.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/containers/string.hpp>
#include <boost/interprocess/sync/named_recursive_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/posix_time/posix_time_system.hpp>
#include <boost/uuid/detail/md5.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/hex.hpp>

#include "./jiaba/Jieba.hpp"
#include "config.h"


extern "C" {
	#include "jbparser.h"
	#include "postgres.h"
	char gErrMsg[ERR_MSG_SZ + 2] = { 0 };
	void _log(const char *fmt, ...)
	{
#if _DEBUG_ > 0
		char sbuf[2000] = { 0 };
		va_list mker;
		va_start(mker, fmt);
		vsnprintf(sbuf, 1998, fmt, mker);
		va_end(mker);
#if defined (WIN32) || defined (_WIN32) || defined (WINDOWS)
		OutputDebugStringA(sbuf);
#else
		std::cout << sbuf << std::endl;
#endif
#endif
	}
}

std::auto_ptr<cppjieba::Jieba> gJiaba;

namespace bip = boost::interprocess;
typedef bip::managed_shared_memory msm;
typedef bip::allocator<char, msm::segment_manager> CharAllocator;
typedef bip::basic_string<char, std::char_traits<char>, CharAllocator> shared_string;
static std::auto_ptr<msm> gSegment;
static std::string gsVsValue;

#if defined (WIN32) || defined (_WIN32) || defined (WINDOWS)
#define __stat _stat
#else
#define __stat stat
#endif
std::string file_md5(const std::string& sFile)
{
	const unsigned int MAX_SZ = 1024 * 1024 * 50;
	std::string str_md5;
	struct __stat info;
	if (0 != __stat(sFile.c_str(), &info))
		return str_md5;
	if (info.st_size > MAX_SZ)
	{
		snprintf(gErrMsg, ERR_MSG_SZ, "[%s] size error:%d", sFile.c_str(), info.st_size);
		return str_md5;
	}
	std::ifstream ifs(sFile.c_str());
	if (!ifs.is_open()|| ifs.fail())
	{
		snprintf(gErrMsg, ERR_MSG_SZ,"open [%s] error", sFile.c_str());
		return str_md5;
	}
	std::vector<unsigned char> bf;
	bf.reserve(1024*1024);
	int lineno = 0;
	std::string line;
	for (; getline(ifs, line); lineno++) {
		if (line.size() == 0) {
			continue;
		}
		std::copy(line.begin(), line.end(),std::back_inserter(bf));
	}
	ifs.close();
	boost::uuids::detail::md5 boost_md5;
	boost_md5.process_bytes(&bf[0], bf.size());
	boost::uuids::detail::md5::digest_type digest;
	boost_md5.get_digest(digest);
	const auto char_digest = reinterpret_cast<const char*>(&digest);
	str_md5.clear();
	boost::algorithm::hex(char_digest, char_digest + sizeof(boost::uuids::detail::md5::digest_type), std::back_inserter(str_md5));
	return str_md5;
}

namespace cppjieba {

	class SharedDictUnitVectShm {
	public:
		SharedDictUnitVectShm(msm::segment_manager* shm) :dict(SharedDictUnitVect::allocator_type(shm)), min_weight_(0), max_weight_(0), median_weight_(0) {}
		SharedDictUnitVect dict;
		double min_weight_;
		double max_weight_;
		double median_weight_;
	};
	SharedDictUnitVect* _loadFromShm(double& min_weight_, double& max_weight_, double& median_weight_)
	{
		_log("%s","_loadFromShm");
		std::pair<SharedDictUnitVectShm*, size_t> fdRes = gSegment->find_no_lock<SharedDictUnitVectShm>(SHM_DICT_NAME);
		if (fdRes.first == nullptr)
			return nullptr;
		min_weight_ = fdRes.first->min_weight_;
		max_weight_ = fdRes.first->max_weight_;
		median_weight_ = fdRes.first->median_weight_;
		_log("%d:%f %f %f", fdRes.first->dict.size(), min_weight_, max_weight_, median_weight_);
		return &fdRes.first->dict;
	}
	SharedDictUnitVect* _saveToShm(const DictUnitVect& duv, const double min_weight_, const double max_weight_, const double median_weight_)
	{
		_log("%s", "_saveToShm");
		if (duv.empty())
			return nullptr;
		SharedDictUnitVectShm* sduv = gSegment->construct<SharedDictUnitVectShm>(SHM_DICT_NAME)(gSegment->get_segment_manager());
		sduv->min_weight_ = min_weight_;
		sduv->max_weight_ = max_weight_;
		sduv->median_weight_ = median_weight_;
		for (auto& du : duv)
		{
			sduv->dict.push_back(SharedDictUnit(gSegment->get_segment_manager(), du));
		}
		gSegment->construct<shared_string>(SHM_NAME_VERSION_ID_NAME)(gsVsValue.c_str(), gSegment->get_segment_manager());
		boost::ulong_long_type freeMem = gSegment->get_free_memory();
		_log("shm free:%lu", freeMem);
		if (freeMem > MAX_SHM_SZ/16)
		{
			gSegment.reset();
			msm::shrink_to_fit(SHM_NAME);
			gSegment.reset(new msm(bip::open_only, SHM_NAME));
			_log("changed shm free:%lu", gSegment->get_free_memory());
			std::pair<SharedDictUnitVectShm*, size_t> fdRes = gSegment->find<SharedDictUnitVectShm>(SHM_DICT_NAME);
			return &fdRes.first->dict;
		}
		return &sduv->dict;
	}
}

using boost::container::vector;
bool init_jieba_share(const char* dictPath, int forceReset)
{
	std::string spth(dictPath);
	if (spth.empty())
		return false;
	boost::algorithm::replace_all(spth, "\\", "/");
	if (spth.c_str()[spth.length() - 1] != '/')
		spth += "/";
	const std::string DICT_PATH = spth + "jieba.dict.utf8";
	const std::string HMM_PATH = spth + "hmm_model.utf8";
	const std::string USER_DICT_PATH = spth + "user.dict.utf8";
	const std::string IDF_PATH = spth + "idf.utf8";
	const std::string STOP_WORD_PATH = spth + "stop_words.utf8";
	try
	{
		if (forceReset == 1)
		{
			bip::named_recursive_mutex::remove(SHM_LOCK_NAME);
			bip::shared_memory_object::remove(SHM_NAME);
		}
		std::chrono::steady_clock::time_point tm = std::chrono::steady_clock::now();
		gsVsValue = std::string(SCUR_SHM_VERSION) + "-" + file_md5(DICT_PATH) + "-" + file_md5(USER_DICT_PATH);
		_log("gsVsValue:%s",gsVsValue.c_str());
		if (gsVsValue.length() < strlen(SCUR_SHM_VERSION) + 4)
		{
			snprintf(gErrMsg, ERR_MSG_SZ, "can not load dict");
			return false;
		}
			
		std::auto_ptr<bip::named_recursive_mutex> gLock(new bip::named_recursive_mutex(bip::open_or_create, SHM_LOCK_NAME));
		auto _chkShMem = [&]() {
			gSegment.reset(new msm(bip::open_or_create, SHM_NAME, MAX_SHM_SZ));
			if (!gSegment->check_sanity() ||
				gSegment->find_no_lock<cppjieba::SharedDictUnitVectShm>(SHM_DICT_NAME).first == nullptr)
			{
				gSegment.reset();
				return false;
			}
			shared_string* sVsValueChk = gSegment->find_no_lock<shared_string>(SHM_NAME_VERSION_ID_NAME).first;
			if (sVsValueChk == nullptr || strcmp(gsVsValue.c_str(), sVsValueChk->c_str()) != 0)
			{
				gSegment.reset();
				return false;
			}
			return true;
		};
		using namespace boost::posix_time;
		std::default_random_engine rdm;
		bip::scoped_lock<bip::named_recursive_mutex> slk(*gLock, second_clock::universal_time() + milliseconds(6 * 1000 + rdm() % 500));
		if (!slk.owns())
		{
			bip::named_recursive_mutex::remove(SHM_LOCK_NAME);
			gLock.reset(new bip::named_recursive_mutex(bip::open_or_create, SHM_LOCK_NAME));
			bip::scoped_lock<bip::named_recursive_mutex> tmp(*gLock);
			slk.swap(tmp);
		}
		bool fromShm = _chkShMem();
		if (!fromShm)
		{
			_log("_chkShMem err");
			bip::shared_memory_object::remove(SHM_NAME);
			gSegment.reset(new msm(bip::create_only,
				SHM_NAME, MAX_SHM_SZ));
		}
		gJiaba.reset(new cppjieba::Jieba(fromShm, DICT_PATH,
			HMM_PATH,
			USER_DICT_PATH,
			IDF_PATH,
			STOP_WORD_PATH));
		unsigned int tmUsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - tm).count();
		_log("init_jieba_share tm:%lu", tmUsed);
		if (!gJiaba->isOK())
		{
			snprintf(gErrMsg, ERR_MSG_SZ, "init jieba parser failed");
			gJiaba.reset();
			bip::shared_memory_object::remove(SHM_NAME);
			return false;
		}
	}
	catch (std::exception& e)
	{
		bip::named_recursive_mutex::remove(SHM_LOCK_NAME);
		bip::shared_memory_object::remove(SHM_NAME);
		snprintf(gErrMsg, ERR_MSG_SZ, "exception:%s",e.what());
		return false;
	}
	return true;
}



typedef struct TokenDescData {
	std::string token;
	std::string descr;
	int type;
} TokenDescData;

typedef struct TokenDescData *TokenDesc;

static std::map<std::string, int> lex_descr_map;
static std::vector<TokenDescData> lex_descr =
{
	{ "eng","letter" },
	{ "nz","other proper noun" },
	{ "n","noun" },
	{ "m","numeral" },
	{ "i","idiom" },
	{ "l","temporary idiom" },
	{ "d","adverb" },
	{ "s","space" },
	{ "t","time" },
	{ "mq","numeral-classifier compound" },
	{ "nr","person's name" },
	{ "j","abbreviate" },
	{ "a","adjective" },
	{ "r","pronoun" },
	{ "b","difference" },
	{ "f","direction noun" },
	{ "nrt","nrt" },
	{ "v","verb" },
	{ "z","z" },
	{ "ns","location" },
	{ "q","quantity" },
	{ "vn","vn" },
	{ "c","conjunction" },
	{ "nt","organization" },
	{ "u","auxiliary" },
	{ "o","onomatopoeia" },
	{ "zg","zg" },
	{ "nrfg","nrfg" },
	{ "df","df" },
	{ "p","prepositional" },
	{ "g","morpheme" },
	{ "y","modal verbs" },
	{ "ad","ad" },
	{ "vg","vg" },
	{ "ng","ng" },
	{ "x","unknown" },
	{ "ul","ul" },
	{ "k","k" },
	{ "ag","ag" },
	{ "dg","dg" },
	{ "rr","rr" },
	{ "rg","rg" },
	{ "an","an" },
	{ "vq","vq" },
	{ "e","exclamation" },
	{ "uv","uv" },
	{ "tg","tg" },
	{ "mg","mg" },
	{ "ud","ud" },
	{ "vi","vi" },
	{ "vd","vd" },
	{ "uj","uj" },
	{ "uz","uz" },
	{ "h","h" },
	{ "ug","ug" },
	{ "rz","rz" }
};

int iLexDescr_size = 0;
typedef struct
{
	boost::container::vector<cppjieba::Word> words;
}TParseWd;

std::map<std::string, int> gWdTypesMap;

void parse_uninit()
{
	gJiaba.reset();
}
int parse_init(const char* base_path,int forceReset)
{
	for (int i = 0; i < lex_descr.size(); ++i)
	{
		lex_descr[i].type = 101 + i;
		lex_descr_map[lex_descr[i].token] = lex_descr[i].type;
	}
	iLexDescr_size = lex_descr.size();
	if (!init_jieba_share(base_path, forceReset))
	{
		gJiaba.reset();
		//strncpy(errMsg,"jieba parser init failed(dict path error?)",200);
		return -1;
	}
	return 0;
}

void load_lextype(LexDescr* lxarr)
{
	_log("%s","load_lextype");
	for (int i=0;i<iLexDescr_size;++i)
	{
		lxarr[i].lexid = lex_descr[i].type;
		lxarr[i].alias = pstrdup(lex_descr[i].token.c_str());
		lxarr[i].descr = pstrdup(lex_descr[i].descr.c_str());
	}
	lxarr[iLexDescr_size].lexid = 0;
}

int init_parse_state(ParserState* pst, const char* str,int len, int queryMode)
{
	_log("init_parse_state:queryMode:%d", queryMode);
	if (gJiaba.get() == nullptr)
	{
		pst->size = 0;
		pst->pos = 0;
		return -1;
	}
	TParseWd *pw = new TParseWd;
	pst->wordVct = pw;
	if(queryMode==1)
		gJiaba->CutForSearch(std::string(str, (size_t)len), pw->words, true);
	else
		gJiaba->Cut(std::string(str, (size_t)len), pw->words, true);
	pst->size = pw->words.size();
	pst->pos = 0;
	return 0;
}

int get_word(ParserState* pst,char** ppstr,int* plen,int* type)
{
	TParseWd *pw = (TParseWd *)pst->wordVct;
	if (gJiaba.get()==nullptr||pst->pos >= pst->size)
	{
		*plen = 0;
		*type = 0;
		return -1;
	}
	std::string& stmp = pw->words[pst->pos].word;
	*ppstr = (char*)stmp.data();
	*plen = stmp.length();
	std::string stype = gJiaba->LookupTag(stmp);
	std::map<std::string, int>::const_iterator itr = lex_descr_map.find(stype);
	if (itr == lex_descr_map.end())
		*type = 'x';
	else
		*type = itr->second;
	++pst->pos;
	return 0;
}
void end_parse(ParserState* pst)
{
	TParseWd *pw = (TParseWd *)pst->wordVct;
	if (pw != nullptr)
	{
		delete pw;
	}
	pst->wordVct = NULL;
	pst->size = pst->pos = 0;
}
