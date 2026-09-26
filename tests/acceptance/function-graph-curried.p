Nat := @{zero:*; succ:*->*;};
List := \A:@ => @{nil:*; cons:A->*->*;};
append := \A:@ => \left:List A => left
	@nil => (\right:List A => right)
	@cons head tail => (\right:List A => (List A).cons head (*tail right));
graph := @append;
append_graph := \A:@ => \xs:List A =>
	xs @(self => (ys:List A)->@append A self ys (append A self ys))
	@nil => (\ys:List A => (@append A).nil ys)
	@cons head tail => (\ys:List A =>
		(@append A).cons head tail ys (append A tail ys) (*tail ys));
append_graph :: (A:@)->(xs:List A)->(ys:List A)->@append A xs ys (append A xs ys);
one := Nat.succ Nat.zero;
left := (List Nat).cons Nat.zero (List Nat).nil;
right := (List Nat).cons one (List Nat).nil;
main := append Nat left right;
expected := (List Nat).cons Nat.zero right;

AppendOf := \A:@ => @\left:List A => @\right:List A => @\output:List A => {
	nil : (right:List A) -> * (List A).nil right right;
	cons : (head:A) -> (tail:List A) -> (right:List A) -> (output:List A) ->
		* tail right output -> * ((List A).cons head tail) right ((List A).cons head output);
};
appendCorrect := \A:@ => \xs:List A => \ys:List A => \zs:List A => \g:@append A xs ys zs => g
	@nil following => (AppendOf A).nil following
	@cons head tail following tailOutput tailGraph =>
		(AppendOf A).cons head tail following tailOutput *tailGraph;
appendCorrect :: (A:@)->(xs:List A)->(ys:List A)->(zs:List A)->(@append A xs ys zs)->AppendOf A xs ys zs;
readAppend := \A:@ => \xs:List A => \ys:List A => \zs:List A => \proof:AppendOf A xs ys zs => proof
	@nil following => following
	@cons head tail following tailOutput rest => (List A).cons head *rest;
specMain := readAppend Nat left right (append Nat left right)
	(appendCorrect Nat left right (append Nat left right) (append_graph Nat left right));
emptySpecMain := readAppend Nat (List Nat).nil right (append Nat (List Nat).nil right)
	(appendCorrect Nat (List Nat).nil right (append Nat (List Nat).nil right) (append_graph Nat (List Nat).nil right));

grow := \n:Nat => n
	@zero => (\start:Nat => start)
	@succ k => (\start:Nat => *k (Nat.succ start));
two := Nat.succ one;
growGraph := @grow;
growMain := grow two Nat.zero;
choose := \n:Nat => n
	@zero => (\A:@ => \x:A => x)
	@succ k => (\A:@ => \x:A => *k A x);
chooseGraph := @choose;
chooseMain := choose two Nat two;
twice := \n:Nat => n
	@zero => (\start:Nat => Nat.succ start)
	@succ k => (\start:Nat => { first := *k start; *k first; });
four := Nat.succ (Nat.succ two);
twiceGraph := @twice;
twiceMain := twice two Nat.zero;
