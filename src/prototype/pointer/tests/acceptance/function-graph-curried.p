Nat := @{zero:*; succ:*->*;};
List := \A:@ => @{nil:*; cons:A->*->*;};
append := \A:@ => \left:List A => left
	@nil => (\right:List A => right)
	@cons head tail => (\right:List A => (List A).cons head (*tail right));
graph := @append;
witness := *append;
one := Nat.succ Nat.zero;
left := (List Nat).cons Nat.zero (List Nat).nil;
right := (List Nat).cons one (List Nat).nil;
main := *append Nat left right @output => output;
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
specMain := *append Nat left right @output =>
	readAppend Nat left right output (appendCorrect Nat left right output @output);
emptySpecMain := *append Nat (List Nat).nil right @output =>
	readAppend Nat (List Nat).nil right output (appendCorrect Nat (List Nat).nil right output @output);

grow := \n:Nat => n
	@zero => (\start:Nat => start)
	@succ k => (\start:Nat => *k (Nat.succ start));
two := Nat.succ one;
growMain := *grow two Nat.zero @output => output;
choose := \n:Nat => n
	@zero => (\A:@ => \x:A => x)
	@succ k => (\A:@ => \x:A => *k A x);
chooseMain := *choose two Nat two @output => output;
twice := \n:Nat => n
	@zero => (\start:Nat => Nat.succ start)
	@succ k => (\start:Nat => { first := *k start; *k first; });
four := Nat.succ (Nat.succ two);
twiceMain := *twice two Nat.zero @output => output;
