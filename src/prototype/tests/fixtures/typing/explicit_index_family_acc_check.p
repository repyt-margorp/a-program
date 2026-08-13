Acc :=
	\A : @ =>
	\R : A -> A -> @ =>
	@\subject : A =>
	{
		acc :
			(x : A) ->
			((y : A) -> R y x -> * y) ->
			* x;
	};

main := Acc;

rebuild := \A : @ => \R : A -> A -> @ =>
	\subject : A => \proof : Acc A R subject =>
		proof @acc x down =>
			(Acc A R).acc x *down;
