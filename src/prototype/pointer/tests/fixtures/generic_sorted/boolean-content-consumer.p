// Eliminate the constructed witness, not a replacement sorting function.
permutation_input := \A:@ => \xs:List A => \ys:List A => \p:permutation A xs ys => p
	@nil => (List A).nil
	@keep h x y prior => (List A).cons h *prior
	@swap x y t => (List A).cons x ((List A).cons y t)
	@compose x y z first second => *first;
permutation_output := \A:@ => \xs:List A => \ys:List A => \p:permutation A xs ys => p
	@nil => (List A).nil
	@keep h x y prior => (List A).cons h *prior
	@swap x y t => (List A).cons y ((List A).cons x t)
	@compose x y z first second => *second;
checked_content := \xs:List Bool => *quickSort Bool &bool_le xs @ys =>
	permutation_output Bool xs ys (quick_content Bool &bool_le xs ys @ys);
checked_origin := \xs:List Bool => *quickSort Bool &bool_le xs @ys =>
	permutation_input Bool xs ys (quick_content Bool &bool_le xs ys @ys);
empty_content := checked_content nil;
singleton_content := checked_content singleton;
reverse_content := checked_content reverse;
duplicates_content := checked_content duplicates;
duplicate_origin := checked_origin duplicates;
