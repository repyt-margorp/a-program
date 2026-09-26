Nat := @{zero:*; succ:*->*;};
apply := \f:Nat->Nat => \x:Nat => f x;
consume := \f:Nat->Nat => \x:Nat => \y:Nat => \trace:@apply f x y => y;
valid := \trace:@apply &Nat.succ Nat.zero (Nat.succ Nat.zero) => consume &Nat.succ Nat.zero (Nat.succ Nat.zero) trace;
bad := \trace:@apply &Nat.succ Nat.zero (Nat.succ Nat.zero) => consume &Nat.succ Nat.zero Nat.zero trace;
