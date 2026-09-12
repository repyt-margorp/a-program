Bool := @{true : *; false : *;};
List := \A : @ => @{nil : *; cons : A -> * -> *;};
headOr := \A : @ => \fallback : A => \xs : List A =>
	xs @nil => fallback @cons head tail => head;
consume := \fallback : Bool => \input : List Bool => \output : Bool =>
	\graph : @headOr Bool fallback input output => output;
empty := (List Bool).nil;
bad := {
	packet := *headOr Bool Bool.false empty;
	packet @returned output graph => consume Bool.true empty output graph;
};
