Nat := @{zero:*; succ:*->*;};
NatList := @{nil:*; cons:Nat->*->*;};
length := \xs:NatList => xs @nil=>Nat.zero @cons head tail=>Nat.succ *tail;
one := NatList.cons Nat.zero NatList.nil;
consume := \input:NatList => \output:Nat => \graph:@length input output => Nat.zero;
bad := *length one @ output => consume one Nat.zero @output;
