Nat := @{zero:*; succ:*->*;};
NatList := @{nil:*; cons:Nat->*->*;};
length := \xs:NatList => xs @nil=>Nat.zero @cons head tail=>Nat.succ *tail;
one := NatList.cons Nat.zero NatList.nil;
bad := *length one @ output => (\output:Nat => @output) Nat.zero;
