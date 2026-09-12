Nat := @{zero:*; succ:*->*;};
Tree := @{leaf:*; fork:*->*->*;};
pred := \n:Nat => n @zero => Nat.zero @succ k => k;

twice := \n:Nat => n
	@zero => pred Nat.zero
	@succ k => { first:=*k; second:=*k; Nat.succ second; };

countCalls := \input:Nat => \output:Nat => \graph:@twice input output => graph
	@zero => Nat.zero
	@succ k first firstGraph second secondGraph => Nat.succ (Nat.succ *secondGraph);

cutoff := \n:Nat => n @zero => Nat.zero
	@succ k => { selected:=*k; ignored:=*k; }.selected;
cutDepth := \input:Nat => \output:Nat => \graph:@cutoff input output => graph
	@zero => Nat.zero
	@succ k result resultGraph => Nat.succ *resultGraph;

rightOnly := \tree:Tree => tree
	@leaf => Tree.leaf
	@fork left right => *right;

rightDepth := \input:Tree => \output:Tree => \graph:@rightOnly input output => graph
	@leaf => Nat.zero
	@fork left right result resultGraph => Nat.succ *resultGraph;

orderedMirror := \tree:Tree => tree
	@leaf => Tree.leaf
	@fork left right => { r:=*right; l:=*left; Tree.fork r l; };

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
main := { packet:=*twice two; packet @returned output graph => countCalls two output graph; };
expected := Nat.succ (Nat.succ two);
unusedMain := { packet:=*rightOnly sample; packet @returned output graph => rightDepth sample output graph; };
unusedExpected := two;
orderedMain := { packet:=*orderedMirror sample; packet @returned output graph => inspect sample output graph; };
orderedExpected := Tree.fork (Tree.fork Tree.leaf Tree.leaf) Tree.leaf;
originalMain := twice two;
originalExpected := two;
originalOrdered := orderedMirror sample;
originalRight := rightOnly sample;
leaf := Tree.leaf;
propertyMain := { packet:=*orderedMirror sample; packet @returned output graph => certify sample output graph; };
propertyExpected := OutputTree.fork (Tree.fork leaf leaf) leaf
	(OutputTree.fork leaf leaf OutputTree.leaf OutputTree.leaf) OutputTree.leaf;
cutMain := { packet:=*cutoff two; packet @returned output graph => cutDepth two output graph; };
cutExpected := two;
