Bool := @{ true:*; false:*; };
Decision := \A:@ => \R:A->A->@ => \x:A => \y:A => @\b:Bool => {
	yes:R x y->* Bool.true;
	no:R y x->* Bool.false;
};
Graph := \A:@ => \le:A->A->Bool => \x:A => @\y:A => @\b:Bool => {
	run:(a:A)->* a (le x a);
};
bridge := \A:@ => \le:A->A->Bool => \R:A->A->@ => \cert:(x:A)->(y:A)->Decision A R x y (le x y) =>
	\x:A => \y:A => \b:Bool => \g:Graph A le x y b => g @run a => cert x a;
