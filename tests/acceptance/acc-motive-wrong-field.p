AccessibleFoo := \A:@ => \R:A->A->@ => @\subject:A => {
	accessible_node : (x:A) -> ((y:A) -> R y x -> * y) -> * x;
};
main := \A:@ => \R:A->A->@ => \P:A->@ =>
	\step:(x:A)->((y:A)->R y x->P y)->P x =>
	\subject:A => \proof:AccessibleFoo A R subject =>
	proof @accessible_node x down => {
		stepAtX := step x;
		stepAtX ((\ignored:((y:A)->R y x->P y) => down) *down);
	};
