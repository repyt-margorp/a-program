// The theorem certifies the output, not a different, unsorted list.
wrong := *quickSort Bool &bool_le duplicates @ys =>
	(bool_correct duplicates ys @ys :: bool_sorted reverse);
