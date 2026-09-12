Bool := @{true:*; false:*;};
Acc := \A:@ => \R:A->A->@ => @\subject:A => {
	acc : (x:A) -> ((y:A) -> R y x -> * y) -> * x;
};
walk := \A:@ => \R:A->A->@ => \P:A->@ => \values:(x:A)->P x => \B:@ => \base:B =>
	\step:(x:A)->((y:A)->R y x->B)->B =>
	\subject:A => \proof:Acc A R subject =>
	proof @acc x down =>
		(\item:P x => \flag:Bool => flag
			@true => base
			@false => step x &(\y:A => \rel:R y x => *down y rel (values y) flag));
walk :: (A:@) -> (R:A->A->@) -> (P:A->@) -> ((x:A)->P x) -> (B:@) -> B ->
	((x:A)->((y:A)->R y x->B)->B) ->
	(subject:A) -> Acc A R subject -> P subject -> Bool -> B;
