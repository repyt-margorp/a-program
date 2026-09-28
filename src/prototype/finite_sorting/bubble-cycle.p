// These closed examples do not establish the general nontransitive Local theorem.
bubble_cycle_result := bubble_sort Point &cycle_le input;
bubble_cycle_content := bubble_content Point &cycle_le input;
bubble_cycle_content :: permutation Point input bubble_cycle_result;
bubble_cycle_local := local_certificate;
bubble_cycle_local :: general_locally_sorted Point Cycle bubble_cycle_result;
bubble_cycle_certified := read_local bubble_cycle_result bubble_cycle_local;
