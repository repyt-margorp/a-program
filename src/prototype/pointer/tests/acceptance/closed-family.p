// Reduction exposes RETURN(A); no open computation is treated as a value.
identity_at := &(\A : @ => \x : ((\B : @ => B) A) => x);
