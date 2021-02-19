#ifndef CPPJIEBA_TRIE_HPP
#define CPPJIEBA_TRIE_HPP

#include <vector>
#include <map>
#include <queue>
//#include "limonp/StdExtension.hpp"
#include "Unicode.hpp"

#include <boost/container/map.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/posix_time/posix_time_system.hpp>
#include <boost/flyweight.hpp>

namespace cppjieba {

	using namespace std;

	const size_t MAX_WORD_LENGTH = 512;
	using boost::container::flat_set;
	using boost::container::flat_map;

	
#pragma   pack(4) 

	class DictUnit {
	public:
		DictUnit():weight(0.0){ tag[0] = 0; }
		Unicode word;
		float weight;
		char tag[6];
	}; // struct DictUnit

	class SharedDictUnit {
	public:
		SharedDictUnit(msm::segment_manager* shm) :word(SharedUnicode::allocator_type(shm)) {};
		SharedDictUnit(msm::segment_manager* shm, const DictUnit& du) :word(du.word.c_str(), SharedUnicode::allocator_type(shm)), weight(du.weight){
			strncpy(tag,du.tag,6);
		};
		SharedUnicode word;
		float weight;
		char tag[6];
	}; // struct DictUnit
#pragma pack() 

	
	DictUnit& sdu2du(const SharedDictUnit& sdu, DictUnit& du)
	{
		du.word = std::wstring(sdu.word.c_str());
		du.weight = sdu.weight;
		strncpy(du.tag, sdu.tag, 6);
		return du;
	}

	typedef bip::allocator<SharedDictUnit, msm::segment_manager> DictUnitAllocator;

	typedef vector<DictUnit> DictUnitVect;
	typedef vector<SharedDictUnit, DictUnitAllocator> SharedDictUnitVect;
	SharedDictUnitVect* _loadFromShm(double& min_weight_, double& max_weight_, double& median_weight_);
	SharedDictUnitVect* _saveToShm(const DictUnitVect& duv, const double min_weight_, const double max_weight_, const double median_weight_);


	   // for debugging
	   // inline ostream & operator << (ostream& os, const DictUnit& unit) {
	   //   string s;
	   //   s << unit.word;
	   //   return os << StringFormat("%s %s %.3lf", s.c_str(), unit.tag.c_str(), unit.weight);
	   // }
#pragma   pack(4) 
	struct Dag {
		RuneStr runestr;
		// [offset, nexts.first]
		vector<pair<size_t, DictUnit> > nexts;
		//limonp::LocalVector<pair<size_t, const DictUnit*> > nexts;
		const DictUnit * pInfo;
		double weight;
		size_t nextPos; // TODO
		Dag() :runestr(), pInfo(NULL), weight(0.0), nextPos(0) {
		}
	}; // struct Dag
#pragma pack() 
	typedef Rune TrieKey;
	struct TrieNode_;
	typedef flat_map<TrieKey, int32_t> NextMap;
#pragma   pack(4) 
	typedef struct TrieNode_
	{
		int32_t next;
		int ptValue;
	}TrieNode;
#pragma pack() 
	typedef vector<TrieNode> TrieNodeVect;
	class Trie {
	public:
		TrieNodeVect tnodes;
		vector<NextMap> nxtMaps;
		//const DictUnitVect& static_node_infos_;
		SharedDictUnitVect* static_node_infos_s;
		Trie(SharedDictUnitVect* dictUnits):static_node_infos_s(dictUnits)
		{
			tnodes.push_back(TrieNode());
			tnodes.back().next = -1;
			tnodes.back().ptValue = -1;
			CreateTrie(dictUnits);
		}
		~Trie() {
			tnodes.clear();
			nxtMaps.clear();
			tnodes.shrink_to_fit();
			nxtMaps.shrink_to_fit();
			//DeleteNode(root_);
		}
		bool Find(RuneStrArray::const_iterator begin, RuneStrArray::const_iterator end, DictUnit* du = nullptr) const {
			if (begin == end) {
				return false;
			}
			const TrieNode* ptNode = &tnodes[0];// root_;
			NextMap::const_iterator citer;
			for (RuneStrArray::const_iterator it = begin; it != end; it++) {
				if (-1 == ptNode->next) {
					return false;
				}
				const NextMap* pmp = &nxtMaps[ptNode->next];
				citer = pmp->find(it->rune);
				if (pmp->end() == citer) {
					return NULL;
				}
				ptNode = &tnodes[citer->second];
			}
			DictUnit res;
			if (ptNode->ptValue < 0)
				return false;
			if(du!=nullptr)
				sdu2du(static_node_infos_s->at(ptNode->ptValue), *du);
			return true;
		}

		void Find(RuneStrArray::const_iterator begin,
			RuneStrArray::const_iterator end,
			vector<struct Dag>&res,
			size_t max_word_len = MAX_WORD_LENGTH) const {
			res.resize(end - begin);
			const TrieNode& root_ = tnodes.front();
			const TrieNode *ptNode = NULL;
			NextMap::const_iterator citer;
			SharedDictUnitVect& sduV = *static_node_infos_s;
			for (size_t i = 0; i < size_t(end - begin); i++) {
				res[i].runestr = *(begin + i);
				Rune rn = res[i].runestr.rune;
				if (root_.next != -1 && nxtMaps[root_.next].end() != (citer = nxtMaps[root_.next].find(rn))) {
					ptNode = &tnodes[citer->second];
				}
				else {
					ptNode = NULL;
				}
				DictUnit du;
				if (ptNode != NULL && ptNode->ptValue>=0) {
					res[i].nexts.push_back(pair<size_t, DictUnit>(i, sdu2du(static_node_infos_s->at(ptNode->ptValue),du)));
				}
				else {
					res[i].nexts.push_back(pair<size_t, DictUnit>(i, du));
				}

				for (size_t j = i + 1; j < size_t(end - begin) && (j - i + 1) <= max_word_len; j++) {
					if (ptNode == NULL || ptNode->next == -1) {
						break;
					}
					const NextMap* pmp = &nxtMaps[ptNode->next];
					Rune crn = (begin + j)->rune;
					citer = pmp->find(crn);
					if (pmp->end() == citer) {
						break;
					}
					ptNode = &tnodes[citer->second];
					if (ptNode->ptValue>=0) {
						res[i].nexts.push_back(pair<size_t, DictUnit>(j, sdu2du(static_node_infos_s->at(ptNode->ptValue),du)));
					}
				}
			}
		}
		//int maxMpSz;
		void InsertNode(const SharedDictUnit& du,int idx) {
			if (du.word.size() <= 0) {
				return;
			}
			int32_t iLstIdx = tnodes.size() - 1;
			NextMap::const_iterator kmIter;
			TrieNode *ptNode = &tnodes[0];
			int curIdx = 0;
			NextMap* cpmp = NULL;
			for(const TrieKey& ky: du.word){
				if (-1 == ptNode->next) {
					ptNode->next = nxtMaps.size();
					cpmp = &nxtMaps.emplace_back(NextMap());
				}
				else
					cpmp = &nxtMaps[ptNode->next];
				kmIter = cpmp->find(ky);
				if (cpmp->end() == kmIter) {
					TrieNode& nextNode = tnodes.emplace_back(TrieNode());
					nextNode.next = -1;
					nextNode.ptValue = -1;
					//ptNode = &tnodes[curIdx];
					cpmp->insert(make_pair(ky, ++iLstIdx));
					ptNode = &nextNode;
				}
				else {
					curIdx = kmIter->second;
					ptNode = &tnodes[curIdx];
				}
			}
			assert(ptNode != NULL);
			ptNode->ptValue = idx;
		}

	private:
		void CreateTrie(SharedDictUnitVect* dictUnits) {
			int sz = dictUnits->size();
			tnodes.reserve(sz * 2);
			nxtMaps.reserve(sz);
			
			for (int i = 0; i < sz;++i) {
				InsertNode(dictUnits->at(i),i);
			}
			for (NextMap& mp : nxtMaps)
			{
				mp.shrink_to_fit();
			}
			//tnodes.clear();
			//tnodes.shrink_to_fit();
		}
	}; // class Trie
} // namespace cppjieba

#endif // CPPJIEBA_TRIE_HPP
