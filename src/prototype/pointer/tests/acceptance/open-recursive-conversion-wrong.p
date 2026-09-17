Nat := @{zero:*; succ:*->*;};
Bool := @{true:*; false:*;};
always := \n:Nat => n @zero => Bool.true @succ k => *k;
OnlyTrue := @\b:Bool => { yes:* Bool.true; };
// Extensional constancy is not a DefEq rule for a neutral recursive Match.
wrong := \n:Nat => ((OnlyTrue).yes :: OnlyTrue (always n));
