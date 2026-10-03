Bool := @{false : *; true : *;};
Nat := @{zero : *; succ : * -> *;};
List := \A : @ => @{nil : *; cons : A -> * -> *;};
Reverse := \A : @ => @{more : * -> A -> *; end : *;};
Choice := \A : @ => \B : @ => @{unset : *; chosen : A -> B -> *;};
Sized := \A : @ => @\size : Nat => {
	nil : * Nat.zero;
	cons : (n : Nat) -> A -> * n -> * (Nat.succ n);
};

list_type := List Nat;
empty := (List Nat).nil;
empty_flags := (List Bool).nil;
empty_reverse := (Reverse Nat).end;
empty_sized := (Sized Nat).nil;
empty_choice := (Choice Nat Bool).unset;
choose := \n : Nat => \b : Bool => (Choice Nat Bool).chosen n b;
choice_number := \x : Choice Nat Bool => x @unset => Nat.zero @chosen n b => n;
choice_flag := \x : Choice Nat Bool => x @unset => Bool.false @chosen n b => b;
identity := \xs : List Nat => xs;
prepend := \n : Nat => \xs : List Nat => (List Nat).cons n xs;
length := \xs : List Nat => xs @nil => #0 @cons n tail => #int_add #1 *tail;
append := \xs : List Nat => xs
	@nil => (\ys : List Nat => ys)
	@cons n tail => (\ys : List Nat => (List Nat).cons n (*tail ys));
identity_flags := \xs : List Bool => xs;
prepend_flag := \b : Bool => \xs : List Bool => (List Bool).cons b xs;
length_flags := \xs : List Bool => xs @nil => #0 @cons b tail => #int_add #1 *tail;
identity_reverse := \xs : Reverse Nat => xs;
prepend_reverse := \n : Nat => \xs : Reverse Nat => (Reverse Nat).more xs n;
length_reverse := \xs : Reverse Nat => xs @end => #0 @more tail n => #int_add #1 *tail;
callback := \f : Nat -> Nat => \n : Nat => f n;
