// A graph for the reverse comparator cannot certify the ascending algorithm.
descending := \x:Bool => \y:Bool => bool_le y x;
wrong := \xs:List Bool => \ys:List Bool =>
	\graph:@quickSort Bool &descending xs ys => bool_correct xs ys graph;
