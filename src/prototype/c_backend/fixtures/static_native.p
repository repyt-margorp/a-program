Nat := @{zero : *; succ : * -> *;};
Numbers := @{nil : *; cons : Nat -> * -> *;};
length := \xs : Numbers => xs @nil => #0 @cons n tail => #int_add #1 *tail;
local_successor := \n : Nat => { f := \x : Nat => Nat.succ x; f n; };
unused_successor := \n : Nat => { f := \x : Nat => Nat.succ x; n; };
repeated_successor := \n : Nat => { f := \x : Nat => Nat.succ x; f (f n); };
captured_list := \xs : Numbers => { f := \n : Nat => Numbers.cons n xs; f Nat.zero; };
captured_length := \xs : Numbers => { f := \dummy : #Int32 => length xs; f #0; };
shadowed_list := \xs : Numbers => {
	f := \n : Nat => Numbers.cons n xs;
	(\xs : Numbers => f Nat.zero) Numbers.nil;
};
