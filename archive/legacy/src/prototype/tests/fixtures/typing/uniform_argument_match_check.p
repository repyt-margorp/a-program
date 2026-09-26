Nat := @{
	zero : *;
	succ : * -> *;
};

identityNat := \x : Nat => x;
expected := { Nat.succ Nat.zero; };
uniformNormalizationEqualArgMatch := Nat.zero
	@zero => {
		n : Nat := identityNat Nat.zero;
		Nat.succ n;
	}
	@succ predecessor => Nat.succ Nat.zero;
