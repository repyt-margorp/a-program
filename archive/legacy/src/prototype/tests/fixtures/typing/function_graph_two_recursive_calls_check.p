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

mirror_graph := \tree:Tree => tree @(self => @mirror self (mirror self))
	@leaf => (@mirror).leaf
	@fork l r => (@mirror).fork l r (mirror r) *r (mirror l) *l;
mirror_graph :: (tree:Tree)->@mirror tree (mirror tree);
certified := inspect sample (mirror sample) (mirror_graph sample);
certifiedOutputTree := mirrorOutputTree sample (mirror sample) (mirror_graph sample);

main := mirror sample;
expected := {
	Tree.fork (Tree.fork Tree.leaf Tree.leaf) Tree.leaf;
};
