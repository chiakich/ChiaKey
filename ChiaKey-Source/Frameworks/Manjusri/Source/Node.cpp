/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/

#include "Node.h"

namespace Manjusri {
Score Node::c_defaultUNKProbability = (Score)0;
Score Node::c_defaultUNKBackoff = (Score)0;
Score Node::c_defaultOverrideScore = (Score)100;
// Length prior, log10 per extra syllable; gold-set safe window [0.85,1.33]. Tune via SetPhraseLengthBonus.
Score Node::c_phraseLengthBonus = (Score)1.0;
};  // namespace Manjusri
