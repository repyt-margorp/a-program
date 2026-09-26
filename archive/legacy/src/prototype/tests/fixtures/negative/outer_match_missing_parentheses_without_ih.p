Bool := @{
	true : *;
	false : *;
};

List := \A : @ => @{
	nil : *;
	cons : A -> * -> *;
};

invalid := \xs : List Bool =>
	xs @nil => Bool.false
	   @cons head tail => head
	      @true => Bool.true
	      @false => Bool.false;
