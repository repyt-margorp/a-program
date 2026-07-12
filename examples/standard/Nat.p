Bool := @{
	true : *;
	false : *;
};

Nat := @{
	zero : *;
	succ : * -> *;
};

add :: Nat -> Nat -> Nat;
add := \n : Nat =>
	n @zero		=> (\m : Nat => m)
	  @succ k	=> (\m : Nat => Nat.succ (*k m));

isZero :: Nat -> Bool;
isZero := \n : Nat =>
	n @zero		=> Bool.true
	  @succ k	=> Bool.false;

pred :: Nat -> Nat;
pred := \n : Nat =>
	n @zero		=> Nat.zero
	  @succ k	=> k;

mul :: Nat -> Nat -> Nat;
mul := \n : Nat =>
	n @zero		=> (\m : Nat => Nat.zero)
	  @succ k	=> (\m : Nat => add m (*k m));

eq :: Nat -> Nat -> Bool;
eq := \n : Nat =>
	n @zero		=> (\m : Nat =>
		m @zero		=> Bool.true
		  @succ j	=> Bool.false)
	  @succ k	=> (\m : Nat =>
		m @zero		=> Bool.false
		  @succ j	=> *k j);

leq :: Nat -> Nat -> Bool;
leq := \n : Nat =>
	n @zero		=> (\m : Nat => Bool.true)
	  @succ k	=> (\m : Nat =>
		m @zero		=> Bool.false
		  @succ j	=> *k j);
