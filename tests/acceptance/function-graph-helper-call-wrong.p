Nat := @{zero:*; succ:*->*;};
List := @{nil:*; cons:Nat->*->*;};
length := \xs:List => xs @nil => Nat.zero @cons head tail => Nat.succ *tail;
otherLength := \xs:List => xs @nil => Nat.zero @cons head tail => Nat.succ *tail;
tailLength := \xs:List => xs @nil => Nat.zero @cons head tail => length tail;
wrong := \head:Nat => \tail:List => \n:Nat => \p:@otherLength tail n =>
	(@tailLength).cons head tail n p;
