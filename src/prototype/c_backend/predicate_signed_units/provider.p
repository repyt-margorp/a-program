Flag := @{true : *; false : *;};
keep32 := \flag : Flag => \n : #Int32 => flag;
keep64 := \flag : Flag => \n : #Int64 => flag;
choose32 := \flag : Flag => \left : #Int32 => \right : #Int32 => flag;
choose64 := \flag : Flag => \left : #Int64 => \right : #Int64 => flag;
print_flag := \flag : Flag => flag @true => #print #"T|" @false => #print #"F|";
reference := {
	print_flag (keep32 Flag.true #-2147483648);
	print_flag (keep32 Flag.false #2147483647);
	print_flag (choose32 Flag.true #-1 #1);
	print_flag (choose32 Flag.false #1 #-1);
	print_flag (keep32 Flag.true #0);
	print_flag (keep32 Flag.false #0);
	print_flag (choose32 Flag.true #-2147483648 #2147483647);
	print_flag (choose32 Flag.false #2147483647 #-2147483648);
};
