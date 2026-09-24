// The same length and set of values do not establish multiplicity preservation.
changed_multiplicity := (List Bool).cons Bool.false ((List Bool).cons Bool.true
	((List Bool).cons Bool.true singleton));
wrong_multiplicity := permutation_refl Bool duplicates_expected;
wrong_multiplicity :: permutation Bool duplicates_expected changed_multiplicity;
