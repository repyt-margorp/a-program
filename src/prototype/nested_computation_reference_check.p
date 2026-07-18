nest := \m : &#.Int64 => &{ &m };

preserveNested := \m : &&#.Int64 => &m;

main := preserveNested (nest &{ #1 });
