Nat := @{
	zero : *;
	succ : * -> *;
};

Fin :=
	@\n : Nat =>
	{
		fzero : (k : Nat) -> * (Nat.succ k);
		fsucc : (k : Nat) -> * k -> * (Nat.succ k);
	};

first := Fin.fzero Nat.zero;
first :: Fin (Nat.succ Nat.zero);

second := Fin.fsucc (Nat.succ Nat.zero) first;
second :: Fin (Nat.succ (Nat.succ Nat.zero));
