#ifndef CPPJIEBA_KEYWORD_EXTRACTOR_H
#define CPPJIEBA_KEYWORD_EXTRACTOR_H

#include <cmath>
#include <set>
#include "MixSegment.hpp"

namespace cppjieba {

	//using namespace limonp;
	using namespace std;

	/*utf8*/
	class KeywordExtractor {
	public:
		struct Word {
			string word;
			vector<size_t> offsets;
			float weight;
		}; // struct Word

		/*KeywordExtractor(const string& dictPath,
			const string& hmmFilePath,
			const string& idfPath,
			const string& stopWordPath,
			const string& userDict = "")
			: segment_(dictPath, hmmFilePath, userDict) {
			LoadIdfDict(idfPath);
			LoadStopWordDict(stopWordPath);
		}*/
		KeywordExtractor(const DictTrie* dictTrie,
			const HMMModel* model,
			const string& idfPath,
			const string& stopWordPath)
			: segment_(dictTrie, model) {

			//boost::posix_time::ptime nw = boost::posix_time::microsec_clock::universal_time();
			LoadIdfDict(idfPath);
			//cout << "LoadIdfDict tm:" << (boost::posix_time::microsec_clock::universal_time() - nw).total_milliseconds() << "\n";

			LoadStopWordDict(stopWordPath);
		}
		~KeywordExtractor() {
		}

		void Extract(const string& sentence, vector<string>& keywords, size_t topN) const {
			vector<Word> topWords;
			Extract(sentence, topWords, topN);
			for (auto& wd : topWords) {
				keywords.push_back(wd.word);
			}
		}

		void Extract(const string& sentence, vector<pair<string, double> >& keywords, size_t topN) const {
			vector<Word> topWords;
			Extract(sentence, topWords, topN);
			for (auto& wd:topWords) {
				keywords.push_back(pair<string, double>(wd.word, wd.weight));
			}
		}

		void Extract(const string& sentence, vector<Word>& keywords, size_t topN) const {
			vector<string> words;
			segment_.Cut(sentence, words);
			map<string, Word> wordmap;
			size_t offset = 0;
			for (string& wd : words) {
				size_t t = offset;
				offset += wd.size();
				if (IsSingleWord(wd) || stopWords_.find(wd) != stopWords_.end()) {
					continue;
				}
				wordmap[wd].offsets.push_back(t);
				wordmap[wd].weight += 1.0;
			}
			if (offset != sentence.size()) {
				//XLOG(ERROR) << "words illegal";
				return;
			}

			keywords.clear();
			keywords.reserve(wordmap.size());
			for (auto& pr : wordmap) {
				boost::container::map<string, double>::const_iterator cit = idfMap_.find(pr.first);
				if (cit != idfMap_.end()) {
					pr.second.weight *= cit->second;
				}
				else {
					pr.second.weight *= idfAverage_;
				}
				pr.second.word = pr.first;
				keywords.push_back(pr.second);
			}
			topN = min(topN, keywords.size());
			partial_sort(keywords.begin(), keywords.begin() + topN, keywords.end(), Compare);
			keywords.resize(topN);
		}
	private:
		void LoadIdfDict(const string& idfPath) {
			ifstream ifs(idfPath.c_str());
			if (!ifs.is_open())
				return;
			//XCHECK(ifs.is_open()) << "open " << idfPath << " failed";
			string line;
			//boost::container::small_vector<string, 2> buf;
			double idf = 0.0;
			double idfSum = 0.0;
			size_t lineno = 0;
			size_t spcPos = 0;
			char* pstr = NULL;
			int lnSz = 0;
			const int maxLnSz = MAX_WORD_LENGTH + 32;
			for (; getline(ifs, line); lineno++) {
				//buf.clear();
				lnSz = line.length();
				if (lnSz<3 || lnSz>maxLnSz) {
					continue;
				}
				spcPos = line.find(" ");
				if (spcPos != std::string::npos && spcPos>0)
				{
					pstr = (char*)line.data();
					pstr[spcPos] = 0;
					idf = atof(pstr + spcPos + 1);
					idfMap_.emplace(pstr, idf);// [pstr] = idf;
					idfSum += idf;
				}
			}
			//assert(lineno);
			idfAverage_ = idfSum / lineno;
			assert(idfAverage_ > 0.0);
		}
		void LoadStopWordDict(const string& filePath) {
			ifstream ifs(filePath.c_str());
			if (!ifs.is_open())
				return;
			//XCHECK(ifs.is_open()) << "open " << filePath << " failed";
			string line;
			while (getline(ifs, line)) {
				stopWords_.insert(line);
			}
			assert(stopWords_.size());
		}

		static bool Compare(const Word& lhs, const Word& rhs) {
			return lhs.weight > rhs.weight;
		}

		MixSegment segment_;
		boost::container::map<string, double> idfMap_;
		double idfAverage_;

		flat_set<string> stopWords_;
	}; // class KeywordExtractor

	   //inline ostream& operator << (ostream& os, const KeywordExtractor::Word& word) {
	   // return os << "{\"word\": \"" << word.word << "\", \"offset\": " << word.offsets << ", \"weight\": " << word.weight << "}"; 
	   //}

} // namespace cppjieba

#endif


