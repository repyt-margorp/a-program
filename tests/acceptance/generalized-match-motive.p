Nat := @{zero:*; succ:*->*;};
Vec := \A:@ => @\n:Nat => {nil:* Nat.zero; cons:A->*n->*(Nat.succ n);};

flip := \A:@ => \n:Nat => \v:Vec A n => v
	@(size self => Vec A size)
	@nil => (Vec A).nil
	@cons h t => (t @(n self => Vec A (Nat.succ n))
		@nil => (Vec A).cons h (Vec A).nil
		@cons q r => (Vec A).cons q ((Vec A).cons h r));
flip :: (A:@)->(n:Nat)->Vec A n->Vec A n;

replace_second := \A:@ => \n:Nat => \v:Vec A n => \replacement:A => v
	@(size self => Vec A size)
	@nil => (Vec A).nil
	@cons h t => (t @(n self => Vec A (Nat.succ n))
		@nil => (Vec A).cons h (Vec A).nil
		@cons q r => (Vec A).cons h ((Vec A).cons replacement r));
replace_second :: (A:@)->(n:Nat)->Vec A n->A->Vec A n;

one := Nat.succ Nat.zero;
two := Nat.succ one;
empty := (Vec Nat).nil;
singleton := (Vec Nat).cons one empty;
sample := (Vec Nat).cons one ((Vec Nat).cons Nat.zero empty);
expected := (Vec Nat).cons Nat.zero singleton;
replaced := (Vec Nat).cons one ((Vec Nat).cons two empty);
main := flip Nat two sample;
roundtrip := flip Nat two main;
empty_flip := flip Nat Nat.zero empty;
singleton_flip := flip Nat one singleton;
replacement_result := replace_second Nat two sample two;
