#pragma once
#include "../txscaled.h"

class Xml;

// Every eqtb entry has a level. Level_zero means undefined
// Level_one is the outer level. The old value must be saved in case
// the current level is different fron the old one, and is greater than one.
// These objects are defined at 1
template <typename T> struct EQTB {
    T   val{};
    int level{1};

    [[nodiscard]] auto must_push(int l) const -> bool { return level != l && l > 1; }
};

using EqtbInt    = EQTB<long>;
using EqtbString = EQTB<std::string>;
using EqtbDim    = EQTB<ScaledInt>;
using EqtbGlue   = EQTB<Glue>;
using EqtbToken  = EQTB<TokenList>;
using EqtbBox    = EQTB<Xml *>;
