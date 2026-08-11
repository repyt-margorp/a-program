nest := \m : &#.Int => &{ &m; };

preserveNested := \m : &&#.Int => &m;

main := preserveNested (nest &{ #1; });
