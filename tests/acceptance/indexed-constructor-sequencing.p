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
inspect := \A:@ => \key:A => \p:packet A key => p @mk tag b proof => unbox A b;

// The final proof's classifier supplies the index, before its preceding
// computed field is applied. No equation is assumed for a sequencing binder.
late := \A:@ => @\key:A => {
	mk:(b:box A)->same A (unbox A b) key->* key;
};
late_build := \A:@ => \x:A => (late A).mk (boxed A x) ((same A).refl x);
late_build :: (A:@)->(x:A)->late A x;
late_inspect := \A:@ => \key:A => \p:late A key => p @mk b proof => unbox A b;

zero := Nat.zero;
one := Nat.succ zero;
main := inspect Nat one (build Nat one);
expected := one;
late_main := late_inspect Nat one (late_build Nat one);
alias := (packet Nat).mk;
partial := alias ((same Nat).refl one);
alias_result := inspect Nat one (alias ((same Nat).refl one) (boxed Nat one) ((same Nat).refl one));
partial_result := inspect Nat one (partial (boxed Nat one) ((same Nat).refl one));
late_partial := (late Nat).mk (boxed Nat one);
late_partial_result := late_inspect Nat one (late_partial ((same Nat).refl one));

// Two still-unknown indices must not move already supplied effects under
// their quantified Lambda, even when the partial call is never completed.
Vec := \A:@ => @\n:Nat => {nil:* Nat.zero;cons:A->* n->*(Nat.succ n);};
pair_vec := @\n:Nat => @\m:Nat => {
	mk:#Text->#Text->Vec #Text n->Vec #Text m->* n m;
};
Trace := @{done:*;step:#Text->*->*;};
effect_partial := pair_vec.mk (#print #"first") (#print #"second");
effect_result := ({
	f := effect_partial;
	#print #"between";
	f (Vec #Text).nil (Vec #Text).nil;
	Trace.done;
}) @#print req k => Trace.step req (k req) @#return x => x;
effect_expected := Trace.step #"first" (Trace.step #"second" (Trace.step #"between" Trace.done));
unused_result := ({effect_partial;Trace.done;})
	@#print req k => Trace.step req (k req) @#return x => x;
unused_expected := Trace.step #"first" (Trace.step #"second" Trace.done);
