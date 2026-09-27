// The second bound must be a->c, not b->c, even though both adjacent edges exist.
wrong_bound := (general_all_from Point Cycle Point.a).cons Point.b
	((List Point).cons Point.c empty) Cycle.ab
	((general_all_from Point Cycle Point.a).cons Point.c empty Cycle.bc
		(general_all_from Point Cycle Point.a).nil);
