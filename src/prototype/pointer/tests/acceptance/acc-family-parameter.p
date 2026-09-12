Nat := @{zero:*; succ:*->*;};
LT := @\left:Nat => @\right:Nat => {
	step : (n:Nat) -> * n (Nat.succ n);
};
Acc := \A:@ => \R:A->A->@ => @\subject:A => {
	acc : (x:A) -> ((y:A) -> R y x -> * y) -> * x;
};
Relation := Nat->Nat->@;
Aliased := \R:Relation => @\subject:Nat => {
	acc : (x:Nat) -> ((y:Nat) -> R y x -> * y) -> * x;
};
direct := Acc Nat LT Nat.zero;
aliased := Aliased LT Nat.zero;
identity := \R:Relation => \proof:R Nat.zero (Nat.succ Nat.zero) => proof;
main := identity LT (LT.step Nat.zero);
expected := LT.step Nat.zero;
