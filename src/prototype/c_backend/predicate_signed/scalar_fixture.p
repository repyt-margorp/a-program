Bool := @{false : *; true : *;};
Packet := @{packet : Bool -> #Int64 -> *;};
call64 := \predicate : #Int64 -> Bool => \n : #Int64 => predicate n;
chain64 := \predicate : #Int64 -> Bool => \n : #Int64 => {
	flag := call64 predicate n;
	flag @false => Bool.false @true => call64 predicate n;
};
record64 := \predicate : #Int64 -> Bool => \n : #Int64 => {
	flag := chain64 predicate n;
	Packet.packet flag n;
};
truth := Bool.true;
