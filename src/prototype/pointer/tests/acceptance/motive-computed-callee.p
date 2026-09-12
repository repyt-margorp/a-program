Nat := @{zero : *; succ : * -> *;};
provide := \ignored : Nat => &(\n : Nat => Nat.succ n);
copy := \n : Nat => n
	@zero => Nat.zero
	@succ previous => (provide Nat.zero) *previous;
main := copy (Nat.succ (Nat.succ Nat.zero));
expected := Nat.succ (Nat.succ Nat.zero);
