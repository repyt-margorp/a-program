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
one := List.cons Nat.zero List.nil;
main := *append one one @result => preserve one one result @result (Covered.cons Nat.zero List.nil Covered.nil) (Covered.cons Nat.zero List.nil Covered.nil);
expected := Covered.cons Nat.zero one (Covered.cons Nat.zero List.nil Covered.nil);
