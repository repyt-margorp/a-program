Bool := @{ false : *; true : *; };
Nat := @{ zero : *; succ : * -> *; };

Precedes :=
	@\left : Bool =>
	@\right : Bool =>
	{
		falseBeforeTrue : * Bool.false Bool.true;
	};

Acc := \A : @ => \R : A -> A -> @ => @\subject : A => {
	acc : (x : A) -> ((y : A) -> R y x -> * y) -> * x;
};

accElim := \A : @ => \R : A -> A -> @ => \P : A -> @ =>
	\step : (x : A) -> ((y : A) -> R y x -> P y) -> P x =>
	\subject : A => \proof : Acc A R subject =>
		proof @acc x down => { stepAtX := step x; stepAtX *down; };

precedesRelation := \left : Bool => \right : Bool => Precedes left right;

accFalse := (Acc Bool &precedesRelation).acc Bool.false
	&(\y : Bool => \edge : Precedes y Bool.false =>
		edge @falseBeforeTrue => Bool.false);
