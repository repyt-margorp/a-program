// Appended to the positive client by generic_sorted.sh.
wrong := \m:Nat => \n:Nat => \input:Box (LT m (Nat.succ n)) =>
	\output:Box (LT (Nat.succ m) (Nat.succ (Nat.succ n))) =>
	\proof:@wrap m n input output => read m n input output proof;
wrong :: (m:Nat)->(n:Nat)->(input:Box (LT m (Nat.succ n)))->
	(output:Box (LT (Nat.succ m) (Nat.succ (Nat.succ n))))->
	@wrap m n input output->LT m (Nat.succ n);
