Nat := @{zero:*; succ:*->*;};
apply := \f:Nat->Nat => \x:Nat => f x;
consume := \f:Nat->Nat => \x:Nat => \y:Nat => \trace:@apply f x y => y;
bad := *apply &Nat.succ Nat.zero @output => consume &Nat.succ Nat.zero Nat.zero @output;
