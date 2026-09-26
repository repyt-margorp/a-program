Unit := @{ unit : *; };
Nat := @{ zero : *; succ : * -> *; };
Loop := @{ loop : *; };

loopRelation := \left : Unit => \right : Unit => Loop;
natFamily := \x : Unit => Nat;

Acc := \A : @ => \R : A -> A -> @ => @\subject : A => {
	acc : (x : A) -> ((y : A) -> R y x -> * y) -> * x;
};

accElim := \A : @ => \R : A -> A -> @ => \P : A -> @ =>
	\step : (x : A) -> ((y : A) -> R y x -> P y) -> P x =>
	\subject : A => \proof : Acc A R subject =>
		proof @acc x down => { stepAtX := step x; stepAtX *down; };

step := \x : Unit => \ih : (y : Unit) -> Loop -> Nat => Nat.zero;
atUnit := accElim Unit;
atRelation := atUnit &loopRelation;
atMotive := atRelation &natFamily;
specialized := atMotive &step;
main := specialized;
