#ifndef CPPJIEBA_SEGMENTBASE_H
#define CPPJIEBA_SEGMENTBASE_H

//#include "limonp/Logging.hpp"
#include "PreFilter.hpp"
#include <cassert>


namespace cppjieba {

const char* const SPECIAL_SEPARATORS = " \t\n\xEF\xBC\x8C\xE3\x80\x82";

//using namespace limonp;

class SegmentBase {
 public:
  SegmentBase() {
	  ResetSeparators(SPECIAL_SEPARATORS);
      //XCHECK(ResetSeparators(SPECIAL_SEPARATORS));
  }
  virtual ~SegmentBase() {
  }

  virtual void Cut(const string& sentence, vector<string>& words) const = 0;

  bool ResetSeparators(const string& s) {
    symbols_.clear();
    RuneStrArray runes;
    if (!DecodeRunesInString(s, runes)) {
      //XLOG(ERROR) << "decode " << s << " failed";
      return false;
    }
    for (auto& rn: runes) {
      if (!symbols_.insert(rn.rune).second) {
        //XLOG(ERROR) << s.substr(runes[i].offset, runes[i].len) << " already exists";
        return false;
      }
    }
    return true;
  }
 protected:
	 flat_set<Rune> symbols_;
}; // class SegmentBase

} // cppjieba

#endif
