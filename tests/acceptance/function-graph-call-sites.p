Nat := @{zero:*; succ:*->*;};
Tree := @{leaf:*; fork:*->*->*;};
pred := \n:Nat => n @zero => Nat.zero @succ k => k;

twice := \n:Nat => n
	@zero => pred Nat.zero
	@succ k => { first:=*k; second:=*k; Nat.succ second; };
twice_graph := \n:Nat => n @(self => @twice self (twice self))
	@zero => (@twice).zero
	@succ k => (@twice).succ k (twice k) *k (twice k) *k;
twice_graph :: (n:Nat)->@twice n (twice n);

countCalls := \input:Nat => \output:Nat => \graph:@twice input output => graph
	@zero => Nat.zero
	@succ k first firstGraph second secondGraph => Nat.succ (Nat.succ *secondGraph);

cutoff := \n:Nat => n @zero => Nat.zero
	@succ k => { selected:=*k; ignored:=*k; }.selected;
cutoff_graph := \n:Nat => n @(self => @cutoff self (cutoff self))
	@zero => (@cutoff).zero
	@succ k => (@cutoff).succ k (cutoff k) *k;
cutoff_graph :: (n:Nat)->@cutoff n (cutoff n);
cutDepth := \input:Nat => \output:Nat => \graph:@cutoff input output => graph
	@zero => Nat.zero
	@succ k result resultGraph => Nat.succ *resultGraph;

rightOnly := \tree:Tree => tree
	@leaf => Tree.leaf
	@fork left right => *right;
right_graph := \tree:Tree => tree @(self => @rightOnly self (rightOnly self))
	@leaf => (@rightOnly).leaf
	@fork left right => (@rightOnly).fork left right (rightOnly right) *right;
right_graph :: (tree:Tree)->@rightOnly tree (rightOnly tree);

rightDepth := \input:Tree => \output:Tree => \graph:@rightOnly input output => graph
	@leaf => Nat.zero
	@fork left right result resultGraph => Nat.succ *resultGraph;

orderedMirror := \tree:Tree => tree
	@leaf => Tree.leaf
	@fork left right => { r:=*right; l:=*left; Tree.fork r l; };
mirror_graph := \tree:Tree => tree @(self => @orderedMirror self (orderedMirror self))
	@leaf => (@orderedMirror).leaf
	@fork left right => (@orderedMirror).fork left right (orderedMirror right) *right (orderedMirror left) *left;
mirror_graph :: (tree:Tree)->@orderedMirror tree (orderedMirror tree);

inspect := \input:Tree => \output:Tree => \graph:@orderedMirror input output => graph
	@leaf => output
	@fork left right r rGraph l lGraph => Tree.fork r l;

OutputTree := @\output:Tree => {
	leaf:* Tree.leaf;
	fork:(r:Tree)->(l:Tree)->* r->* l->* (Tree.fork r l);
};
certify := \input:Tree => \output:Tree => \graph:@orderedMirror input output => graph
	@leaf => OutputTree.leaf
	@fork left right r rGraph l lGraph => OutputTree.fork r l *rGraph *lGraph;

two := Nat.succ (Nat.succ Nat.zero);
sample := Tree.fork Tree.leaf (Tree.fork Tree.leaf Tree.leaf);
main := countCalls two (twice two) (twice_graph two);
expected := Nat.succ (Nat.succ two);
unusedMain := rightDepth sample (rightOnly sample) (right_graph sample);
unusedExpected := two;
orderedMain := inspect sample (orderedMirror sample) (mirror_graph sample);
orderedExpected := Tree.fork (Tree.fork Tree.leaf Tree.leaf) Tree.leaf;
originalMain := twice two;
originalExpected := two;
originalOrdered := orderedMirror sample;
originalRight := rightOnly sample;
leaf := Tree.leaf;
propertyMain := certify sample (orderedMirror sample) (mirror_graph sample);
propertyExpected := OutputTree.fork (Tree.fork leaf leaf) leaf
	(OutputTree.fork leaf leaf OutputTree.leaf OutputTree.leaf) OutputTree.leaf;
cutMain := cutDepth two (cutoff two) (cutoff_graph two);
cutExpected := two;
