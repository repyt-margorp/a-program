Acc := \A:@ => \R:A->A->@ => @\subject:A => {
	acc : (x:A) -> ((y:A) -> R y x -> * y) -> * x;
};
main := \A:@ => \R:A->A->@ => \P:A->@ =>
	\step:(x:A)->((y:A)->R y x->P y)->P x =>
	\subject:A => \proof:Acc A R subject =>
	proof @acc x down => {
		stepAtX := step x;
		stepAtX ((\ignored:((y:A)->R y x->P y) => down) *down);
	};
