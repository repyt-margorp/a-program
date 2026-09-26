Bool := @{true : *; false : *;};
List := \A : @ => @{nil : *; cons : A -> * -> *;};
headOr := \A : @ => \fallback : A => \xs : List A =>
	xs @nil => fallback @cons head tail => head;
consume := \fallback : Bool => \input : List Bool => \output : Bool =>
	\graph : @headOr Bool fallback input output => output;
empty := (List Bool).nil;
graph := (@headOr Bool Bool.false).nil;
valid := consume Bool.false empty Bool.false graph;
bad := consume Bool.true empty Bool.false graph;
