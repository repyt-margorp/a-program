Nat := @{zero:*; succ:*->*;};
Bool := @{false:*; true:*;};
List := @{nil:*; cons:Bool->*->*;};

length := \xs:List => (\ys:List => ys
	@nil => Nat.zero
	@cons head tail => (head
		@false => Nat.succ *tail
		@true => Nat.succ *tail)) xs;
sample := List.cons Bool.false (List.cons Bool.true List.nil);
expected := Nat.succ (Nat.succ Nat.zero);
main := *length sample @output => output;

Size := @\xs:List => @\n:Nat => {
	nil:* List.nil Nat.zero;
	cons:(head:Bool)->(tail:List)->(n:Nat)->* tail n->* (List.cons head tail) (Nat.succ n);
};
correct := \xs:List => \n:Nat => \g:@length xs n => g
	@nil => Size.nil
	@false tail count trace => Size.cons Bool.false tail count *trace
	@true tail count trace => Size.cons Bool.true tail count *trace;
correct :: (xs:List)->(n:Nat)->@length xs n->Size xs n;
read := \xs:List => \n:Nat => \p:Size xs n => p
	@nil => Nat.zero
	@cons head tail count trace => Nat.succ *trace;
proofMain := *length sample @output => read sample output (correct sample output @output);
emptyMain := *length List.nil @output => output;
zero := Nat.zero;

prefix := \xs:List => (\ys:List => ys
	@nil => Nat.zero
	@cons head tail => {
		prior := *tail;
		head @false => Nat.succ prior
		     @true => { again := *tail; Nat.succ again; };
	}) xs;
prefixMain := *prefix sample @output => output;
prefixCorrect := \xs:List => \n:Nat => \g:@prefix xs n => g
	@nil => Size.nil
	@false tail prior priorTrace => Size.cons Bool.false tail prior *priorTrace
	@true tail prior priorTrace again againTrace => Size.cons Bool.true tail again *againTrace;
prefixCorrect :: (xs:List)->(n:Nat)->@prefix xs n->Size xs n;
prefixProofMain := *prefix sample @output => read sample output (prefixCorrect sample output @output);

after := \xs:List => (\ys:List => ys
	@nil => Nat.zero
	@cons head tail => {
		prior := *tail;
		prior @zero => Nat.succ Nat.zero
		      @succ n => Nat.succ (Nat.succ n);
	}) xs;
afterMain := *after sample @output => output;

Pick := @{left:*; right:*;};
Pairs := @{empty:*; link:Bool->Pick->*->*;};
nested := \xs:Pairs => (\ys:Pairs => ys
	@empty => Nat.zero
	@link b p tail => (b @false => Nat.succ *tail
		@true => (p @left => Nat.succ *tail @right => Nat.succ *tail))) xs;
nestedInput := Pairs.link Bool.true Pick.left
	(Pairs.link Bool.true Pick.right (Pairs.link Bool.false Pick.left Pairs.empty));
nestedMain := *nested nestedInput @output => output;
three := Nat.succ expected;

direct := \xs:List => xs @nil => Nat.zero
	@cons head tail => (head @false => Nat.succ *tail @true => Nat.succ *tail);
directMain := *direct sample @output => output;

Tree := @{leaf:*; node:Bool->(Nat->*)->*;};
walk := \tree:Tree => (\current:Tree => current @leaf => Nat.zero
	@node flag down => (flag
		@false => Nat.succ (*down Nat.zero)
		@true => { prior := *down Nat.zero; Nat.succ (*down prior); })) tree;
tree := Tree.node Bool.true &(\n:Nat => Tree.node Bool.false &(\m:Nat => Tree.leaf));
walkMain := *walk tree @output => output;

Indexed := @\n:Nat => { base:* Nat.zero; step:(k:Nat)->Bool->* k->* (Nat.succ k); };
countIndexed := \n:Nat => \xs:Indexed n => xs
	@base => Nat.zero
	@step k flag tail => (flag @false => Nat.succ *tail @true => Nat.succ *tail);
indexed := Indexed.step (Nat.succ Nat.zero) Bool.false (Indexed.step Nat.zero Bool.true Indexed.base);
indexedMain := *countIndexed expected indexed @output => output;

Root := @{root:Bool->*;};
Vec := @\n:Nat => { nil:* Nat.zero; cons:(k:Nat)->* k->* (Nat.succ k); };
lastSize := \root:Root => root @root flag =>
	(\n:Nat => \v:Vec n => \consumer:Vec n->Nat =>
		(v @nil => Nat.zero @cons k tail => k));
vector := Vec.cons (Nat.succ Nat.zero) (Vec.cons Nat.zero Vec.nil);
refinedMain := *lastSize (Root.root Bool.true) expected vector &(\v:Vec expected => Nat.zero) @output => output;
one := Nat.succ Nat.zero;
