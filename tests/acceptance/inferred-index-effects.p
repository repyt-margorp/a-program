Nat := @{zero:*; succ:*->*;};
Vec := \A:@ => @\n:Nat => {
	nil:* Nat.zero;
	cons:A->* n->* (Nat.succ n);
};
Trace := @{done:*; step:#Text->*->*;};
partial := (Vec #Text).cons (#print #"head");
main := ({
	f := partial;
	#print #"between";
	f {#print #"tail"; (Vec #Text).nil;};
	Trace.done;
}) @#print req k => Trace.step req (k req)
   @#return x => x;
expected := Trace.step #"head" (Trace.step #"between" (Trace.step #"tail" Trace.done));

D := @\n:Nat => {mk:Vec #Text (Nat.succ n)->Vec #Text n->* n;};
nil := (Vec #Text).nil;
one := (Vec #Text).cons #"one" nil;
spineMain := ({
	D.mk {#print #"first"; one;} {#print #"second"; nil;};
	Trace.done;
}) @#print req k => Trace.step req (k req)
   @#return x => x;
spineExpected := Trace.step #"first" (Trace.step #"second" Trace.done);
