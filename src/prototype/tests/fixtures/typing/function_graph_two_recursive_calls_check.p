Tree := @{
	leaf : *;
	fork : * -> * -> *;
};

mirror := \tree : Tree =>
	tree
		@leaf => Tree.leaf
		@fork left right => Tree.fork *right *left;

mirror :: Tree -> Tree;

inspect := \input : Tree => \output : Tree =>
	\graph : @mirror input output =>
		graph
			@leaf => output
			@fork left right rightOutput rightGraph leftOutput leftGraph =>
				Tree.fork rightOutput leftOutput;

MirrorOutputTree := @\output : Tree => {
	leaf : * Tree.leaf;
	fork : (right : Tree) -> (left : Tree) ->
		* right -> * left -> * (Tree.fork right left);
};

mirrorOutputTree := \input : Tree => \output : Tree =>
	\graph : @mirror input output =>
		graph
		@leaf => MirrorOutputTree.leaf
		@fork left right rightOutput rightGraph leftOutput leftGraph =>
			MirrorOutputTree.fork rightOutput leftOutput
				*rightGraph *leftGraph;

sample := Tree.fork Tree.leaf (Tree.fork Tree.leaf Tree.leaf);

certified := {
	packet := *mirror sample;
	packet @returned output graph => inspect sample output graph;
};

certifiedOutputTree := {
	packet := *mirror sample;
	packet @returned output graph => mirrorOutputTree sample output graph;
};

main := mirror sample;
expected := {
	Tree.fork (Tree.fork Tree.leaf Tree.leaf) Tree.leaf;
};
