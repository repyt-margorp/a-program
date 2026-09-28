// The relation has a <= b <= c <= a, but not b <= a.
cycle_tree := tree_backend Point Cycle &cycle_le &cycle_decide;
cycle_tree_result := sorting_run Point Cycle cycle_tree input;
cycle_tree_local := sorting_local Point Cycle cycle_tree input;
cycle_tree_local :: general_locally_sorted Point Cycle (treeSort Point &cycle_le input);
cycle_tree_content := sorting_content Point Cycle cycle_tree input;
cycle_tree_content :: permutation Point input (treeSort Point &cycle_le input);
cycle_tree_expected := (List Point).cons Point.b ((List Point).cons Point.c ((List Point).cons Point.a empty));
cycle_tree_certified := read_local cycle_tree_result cycle_tree_local;
