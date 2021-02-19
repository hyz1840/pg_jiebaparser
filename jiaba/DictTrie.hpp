#ifndef CPPJIEBA_DICT_TRIE_HPP
#define CPPJIEBA_DICT_TRIE_HPP

#include <iostream>
#include <fstream>
#include <map>
#include <string>
#include <cstring>
#include <cstdlib>
#include <stdint.h>
#include <cmath>
#include <limits>
#include <boost/algorithm/string.hpp>

//#include "limonp/StringUtil.hpp"
//#include "limonp/Logging.hpp"
#include "Unicode.hpp"
#include "Trie.hpp"
extern "C" {
	void _log(const char *fmt, ...);
}


namespace cppjieba {

	//using namespace limonp;

	const double MIN_DOUBLE = -3.14e+100;// numeric_limits<double>::min();// 
	const double MAX_DOUBLE = 3.14e+100; //numeric_limits<double>::max();// 
	const size_t DICT_COLUMN_NUM = 3;
	const char* const UNKNOWN_TAG = "";


	class DictTrie {
	public:
		enum UserWordWeightOption {
			WordWeightMin,
			WordWeightMedian,
			WordWeightMax,
		}; // enum UserWordWeightOption

		DictTrie(bool fromShm,const string& dict_path, const string& user_dict_paths = "", UserWordWeightOption user_word_weight_opt = WordWeightMedian):static_node_infos_s(nullptr){
			Init(fromShm,dict_path, user_dict_paths, user_word_weight_opt);
		}

		~DictTrie() {
			delete trie_;
		}

		bool Find(RuneStrArray::const_iterator begin, RuneStrArray::const_iterator end, DictUnit* du=nullptr) const {
			return trie_->Find(begin, end, du);
		}

		void Find(RuneStrArray::const_iterator begin,
			RuneStrArray::const_iterator end,
			vector<struct Dag>&res,
			size_t max_word_len = MAX_WORD_LENGTH) const {
			trie_->Find(begin, end, res, max_word_len);
		}

		bool Find(const string& word)
		{
			const DictUnit *tmp = NULL;
			RuneStrArray runes;
			if (!DecodeRunesInString(word, runes))
			{
				//XLOG(ERROR) << "Decode failed.";
				return false;
			}
			return Find(runes.begin(), runes.end());
		}

		bool IsUserDictSingleChineseWord(const Rune& word) const {
			return user_dict_single_chinese_word_.find(word) != user_dict_single_chinese_word_.end();
		}

		double GetMinWeight() const {
			return min_weight_;
		}
		void InserUserDictNode(const string& line) {
			vector<string> buf;
			boost::algorithm::split(buf, line, boost::algorithm::is_any_of(" "));
			const int sz = buf.size();
			if (sz < 1 || sz>3)
				return;
			DictUnit& node_info = static_node_infos_.emplace_back();
			switch (sz)
			{
			case 1:
			{
				MakeNodeInfo(node_info,
					buf[0],
					user_word_default_weight_,
					UNKNOWN_TAG);
				break;
			}
			case 2:
			{
				MakeNodeInfo(node_info,
					buf[0],
					user_word_default_weight_,
					buf[1]);
				break;
			}
			case 3:
			{
				int freq = atoi(buf[1].c_str());
				assert(freq_sum_ > 0.0);
				double weight = log(1.0 * freq / freq_sum_);
				MakeNodeInfo(node_info, buf[0], weight, buf[2]);
				break;
			}
			default:
				break;
			}
			if (node_info.word.size() == 1) {
				user_dict_single_chinese_word_.insert(node_info.word[0]);
			}
		}

		void LoadUserDict(const vector<string>& buf) {
			for (auto& s: buf) {
				InserUserDictNode(s);
			}
		}

		void LoadUserDict(const set<string>& buf) {
			std::set<string>::const_iterator iter;
			for (iter = buf.begin(); iter != buf.end(); iter++) {
				InserUserDictNode(*iter);
			}
		}

		void LoadUserDict(const string& filePaths) {
			vector<string> files;
			boost::algorithm::split(files, filePaths, boost::algorithm::is_any_of("|;"));
			//vector<string> files = limonp::Split(filePaths, "|;");
			size_t lineno = 0;
			for (size_t i = 0; i < files.size(); i++) {
				ifstream ifs(files[i].c_str());
				if(!ifs.is_open())
					continue;
				string line;

				for (; getline(ifs, line); lineno++) {
					if (line.size() == 0) {
						continue;
					}
					InserUserDictNode(line);
				}
			}
		}
		DictUnitVect static_node_infos_;
		SharedDictUnitVect* static_node_infos_s;

	private:
		void Init(bool fromShm,const string& dict_path, const string& user_dict_paths, UserWordWeightOption user_word_weight_opt) {
			if (fromShm)
			{
				static_node_infos_s = _loadFromShm(min_weight_, max_weight_, median_weight_);
			}
			if (static_node_infos_s == nullptr)
			{
				LoadDict(dict_path);
				freq_sum_ = CalcFreqSum(static_node_infos_);
				CalculateWeight(static_node_infos_, freq_sum_);
				SetStaticWordWeights(user_word_weight_opt);
				if (user_dict_paths.size()) {
					LoadUserDict(user_dict_paths);
				}
				static_node_infos_s = _saveToShm(static_node_infos_,min_weight_, max_weight_, median_weight_);
				static_node_infos_.clear();
				static_node_infos_.shrink_to_fit();
			}
			CreateTrie(static_node_infos_s); 
		}

		void CreateTrie(SharedDictUnitVect* dictUnits) {
			trie_ = new Trie(dictUnits);
			trie_->tnodes.shrink_to_fit();
			trie_->nxtMaps.shrink_to_fit();
		}

		inline bool MakeNodeInfo(DictUnit& node_info,
			const string& word,
			double weight,
			const string& tag) {
			if (!DecodeRunesInString(word, node_info.word)) {
				//XLOG(ERROR) << "Decode " << word << " failed.";
				return false;
			}
			node_info.weight = weight;
			node_info.word.shrink_to_fit();
			memset(node_info.tag, 0, sizeof(node_info.tag));
			strncpy(node_info.tag, tag.c_str(), 4);
			return true;
		}

		void LoadDict(const string& filePath) {
			static_node_infos_.clear();
			static_node_infos_.reserve(600*1000);
			ifstream ifs(filePath.c_str());
			if (!ifs.is_open())
				return;
			//XCHECK(ifs.is_open()) << "open " << filePath << " failed.";
			string line;
			boost::container::vector<string> buf;
			//DictUnit node_info;
			for (size_t lineno = 0; getline(ifs, line); lineno++) {
				boost::algorithm::split(buf, line, boost::algorithm::is_any_of(" "));
				//Split(line, buf, " ");
				assert(buf.size() == DICT_COLUMN_NUM);// << "split result illegal, line:" << line;
				MakeNodeInfo(static_node_infos_.emplace_back(DictUnit()),
					buf[0],
					atof(buf[1].c_str()),
					buf[2]);
			}
		}

		static bool WeightCompare(const DictUnit& lhs, const DictUnit& rhs) {
			return lhs.weight < rhs.weight;
		}

		void SetStaticWordWeights(UserWordWeightOption option) {
			assert(!static_node_infos_.empty());
			if (static_node_infos_.empty())
				return;
			double wt = 0.0;
			min_weight_ = MAX_DOUBLE;
			max_weight_ = MIN_DOUBLE;
			median_weight_ = 0.0;
			for (auto& nd : static_node_infos_)
			{
				if (nd.weight > max_weight_)
					max_weight_ = nd.weight;
				else if (nd.weight < min_weight_)
					min_weight_ = nd.weight;
				wt += nd.weight;
			}
			median_weight_ = wt / static_node_infos_.size();// x[x.size() / 2].weight;
			switch (option) {
			case WordWeightMin:
				user_word_default_weight_ = min_weight_;
				break;
			case WordWeightMedian:
				user_word_default_weight_ = median_weight_;
				break;
			default:
				user_word_default_weight_ = max_weight_;
				break;
			}
		}

		double CalcFreqSum(const DictUnitVect& node_infos) const {
			double sum = 0.0;
			for (const DictUnit& du: node_infos) {
				sum += du.weight;
			}
			return sum;
		}

		void CalculateWeight(DictUnitVect& node_infos, double sum) const {
			assert(sum > 0.0);
			//int sz = node_infos.size();
			for (DictUnit& du : node_infos) {
				assert(du.weight > 0.0);
				du.weight = log(double(du.weight) / sum);
			}
		}

		void Shrink(DictUnitVect& units) const {
			DictUnitVect(units.begin(), units.end()).swap(units);
		}


		Trie * trie_;

		double freq_sum_;
		double min_weight_;
		double max_weight_;
		double median_weight_;
		double user_word_default_weight_;
		flat_set<Rune> user_dict_single_chinese_word_;
	};
}

#endif
