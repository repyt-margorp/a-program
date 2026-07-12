Bool := @{
	true : *;
	false : *;
};

Nat := @{
	zero : *;
	succ : * -> *;
};

Int := @{
	pair : Nat -> Nat -> *;
};

not :: Bool -> Bool;
not := \b : Bool =>
	b @true		=> Bool.false
	  @false	=> Bool.true;

and :: Bool -> Bool -> Bool;
and := \a : Bool =>
	a @true		=> (\b : Bool => b)
	  @false	=> (\b : Bool => Bool.false);

addNat :: Nat -> Nat -> Nat;
addNat := \n : Nat =>
	n @zero		=> (\m : Nat => m)
	  @succ k	=> (\m : Nat => Nat.succ (*k m));

mulNat :: Nat -> Nat -> Nat;
mulNat := \n : Nat =>
	n @zero		=> (\m : Nat => Nat.zero)
	  @succ k	=> (\m : Nat => addNat m (*k m));

subNat :: Nat -> Nat -> Nat;
subNat := \n : Nat =>
	n @zero		=> (\m : Nat => Nat.zero)
	  @succ k	=> (\m : Nat =>
		m @zero		=> Nat.succ k
		  @succ j	=> *k j);

eqNat :: Nat -> Nat -> Bool;
eqNat := \n : Nat =>
	n @zero		=> (\m : Nat =>
		m @zero		=> Bool.true
		  @succ j	=> Bool.false)
	  @succ k	=> (\m : Nat =>
		m @zero		=> Bool.false
		  @succ j	=> *k j);

leqNat :: Nat -> Nat -> Bool;
leqNat := \n : Nat =>
	n @zero		=> (\m : Nat => Bool.true)
	  @succ k	=> (\m : Nat =>
		m @zero		=> Bool.false
		  @succ j	=> *k j);

zero :: Int;
zero := Int.pair Nat.zero Nat.zero;

fromNat :: Nat -> Int;
fromNat := \n : Nat => Int.pair n Nat.zero;

negFromNat :: Nat -> Int;
negFromNat := \n : Nat => Int.pair Nat.zero n;

neg :: Int -> Int;
neg := \x : Int =>
	x @pair p n => Int.pair n p;

normalize :: Int -> Int;
normalize := \x : Int =>
	x @pair p n => (
		(leqNat p n :: Bool)
			@true	=> Int.pair Nat.zero (subNat n p)
			  @false	=> Int.pair (subNat p n) Nat.zero
	);

add :: Int -> Int -> Int;
add := \x : Int =>
	x @pair xp xn => (\y : Int =>
		y @pair yp yn =>
			normalize (Int.pair (addNat xp yp) (addNat xn yn)));

sub :: Int -> Int -> Int;
sub := \x : Int => \y : Int => add x (neg y);

mul :: Int -> Int -> Int;
mul := \x : Int =>
	x @pair xp xn => (\y : Int =>
		y @pair yp yn =>
			normalize (Int.pair
				(addNat (mulNat xp yp) (mulNat xn yn))
				(addNat (mulNat xp yn) (mulNat xn yp))));

eq :: Int -> Int -> Bool;
eq := \x : Int =>
	x @pair xp xn => (\y : Int =>
		y @pair yp yn =>
			eqNat (addNat xp yn) (addNat xn yp));
