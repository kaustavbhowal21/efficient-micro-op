# ReplacementPolicies.py snippet for gem5 integration of FURBYS
# Append or include this into src/mem/cache/replacement_policies/ReplacementPolicies.py

from m5.params import *
from m5.proxy import *
from m5.objects.BaseReplacementPolicy import BaseReplacementPolicy

class FURBYSRP(BaseReplacementPolicy):
    type = 'FURBYSRP'
    cxx_class = 'gem5::replacement_policy::FURBYSRP'
    cxx_header = "mem/cache/replacement_policies/furbys_rp.hh"

    bypass_threshold_k = Param.Int(1, "Hyperparameter K for selective bypassing (default 1)")
    pitfall_depth = Param.Unsigned(2, "Depth of local miss-pitfall detector (default 2)")

