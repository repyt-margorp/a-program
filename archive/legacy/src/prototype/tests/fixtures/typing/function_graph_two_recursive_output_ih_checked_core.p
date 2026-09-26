Tree := @{
	leaf : *;
	fork : * -> * -> *;
};

mirror := \tree : Tree =>
	tree
		@leaf => Tree.leaf
		@fork left right => Tree.fork *right *left;

mirror :: Tree -> Tree;

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
