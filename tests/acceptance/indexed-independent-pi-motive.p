// Independent proof arguments must not obscure the retained result family.
Nat := @{zero:*;succ:*->*;};
List := @{nil:*;cons:Nat->*->*;};
append := \xs:List => xs @nil => (\ys:List => ys)
	@cons h t => (\ys:List => List.cons h (*t ys));
Covered := @\xs:List => {
	nil:* List.nil;
	cons:(h:Nat)->(t:List)->* t->* (List.cons h t);
};
tail := \h:Nat => \t:List => \p:Covered (List.cons h t) => p @cons a b q => q;
step := \h:Nat => \t:List => \ys:List => \out:List =>
	\ih:Covered t->Covered ys->Covered out =>
	\left:Covered (List.cons h t) => \right:Covered ys =>
	Covered.cons h out (ih (tail h t left) right);
preserve := \xs:List => \ys:List => \zs:List => \g:@append xs ys zs => g
	@case0 following => (\left:Covered List.nil => \right:Covered following => right)
	@case1 h t following output rest => step h t following output &*rest;
preserve :: (xs:List)->(ys:List)->(zs:List)->@append xs ys zs->Covered xs->Covered ys->Covered zs;
append_graph := \xs:List => xs @(self => (ys:List)->@append self ys (append self ys))
	@nil => (\ys:List => (@append).case0 ys)
	@cons h t => (\ys:List => (@append).case1 h t ys (append t ys) (*t ys));
append_graph :: (xs:List)->(ys:List)->@append xs ys (append xs ys);
one := List.cons Nat.zero List.nil;
main := preserve one one (append one one) (append_graph one one)
	(Covered.cons Nat.zero List.nil Covered.nil) (Covered.cons Nat.zero List.nil Covered.nil);
expected := Covered.cons Nat.zero one (Covered.cons Nat.zero List.nil Covered.nil);
