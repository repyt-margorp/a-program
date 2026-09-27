// Diagnostic, not an accepted negative test: this valid pure dependent call
// rejects at b590dbc. See the finite-position sorting plan's constructor gate.
Nat := @{zero:*;succ:*->*;};
same := \A:@ => @\left:A => @\right:A => {refl:(x:A)->* x x;};
box := \A:@ => @{mk:A->*;};
unbox := \A:@ => \b:box A => b @mk x => x;
boxed := \A:@ => \x:A => (box A).mk x;

packet := \A:@ => @\key:A => {
	mk:(tag:same A key key)->(b:box A)->same A (unbox A b) key->* key;
};
build := \A:@ => \x:A => (packet A).mk ((same A).refl x) (boxed A x) ((same A).refl x);
build :: (A:@)->(x:A)->packet A x;
