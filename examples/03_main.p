Bool := @{
	true : *;
	false : *;
};

id := \A : @ => x : A => x;
main := id Bool Bool.true;
