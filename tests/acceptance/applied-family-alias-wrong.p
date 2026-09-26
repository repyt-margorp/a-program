Unit := @{unit:*;};
Other := @{unit:*;};
Counter := \A : @ => @{zero:*; succ:*->*;};
Nat := Counter Unit;
OtherNat := Counter Other;
identity := \n : Nat => n;
main := identity OtherNat.zero;
