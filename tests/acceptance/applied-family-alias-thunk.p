Unit := @{unit:*;};
Counter := \A : @ => @{zero:*; succ:*->*;};
delayed := &(Counter Unit);
main := delayed.zero;
