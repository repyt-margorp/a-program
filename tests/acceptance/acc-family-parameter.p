Nat := @{zero:*; succ:*->*;};
LT := @\left:Nat => @\right:Nat => {
	step : (n:Nat) -> * n (Nat.succ n);
};
Acc := \A:@ => \R:A->A->@ => @\subject:A => {
	acc : (x:A) -> ((y:A) -> R y x -> * y) -> * x;
};
AccessibleFoo := \A:@ => \R:A->A->@ => @\subject:A => {
	accessible_node : (x:A) -> ((y:A) -> R y x -> * y) -> * x;
};
readAccessible := \A:@ => \R:A->A->@ => \subject:A => \proof:AccessibleFoo A R subject =>
	proof @accessible_node current down => current;
readAccessible :: (A:@) -> (R:A->A->@) -> (subject:A) -> AccessibleFoo A R subject -> A;
Relation := Nat->Nat->@;
Aliased := \R:Relation => @\subject:Nat => {
	acc : (x:Nat) -> ((y:Nat) -> R y x -> * y) -> * x;
};
direct := Acc Nat LT Nat.zero;
aliased := Aliased LT Nat.zero;
identity := \R:Relation => \proof:R Nat.zero (Nat.succ Nat.zero) => proof;
main := identity LT (LT.step Nat.zero);
expected := LT.step Nat.zero;
