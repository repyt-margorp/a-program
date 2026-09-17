Nat := @{zero:*; succ:*->*;};
Bool := @{true:*; false:*;};
always := \n:Nat => n @zero => Bool.true @succ k => *k;
OnlyTrue := @\b:Bool => { yes:* Bool.true; };
main := (OnlyTrue).yes;
main :: OnlyTrue (always (Nat.succ (Nat.succ Nat.zero)));
