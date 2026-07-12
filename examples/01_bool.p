Bool := @{
	true : *;
	false : *;
};

id := \A : @ => x : A => x;
x := id Bool Bool.true;
