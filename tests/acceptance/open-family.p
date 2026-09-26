// A type family must remain symbolic while its function parameter is open.
identity_at := &(\F : (@ -> @) => \A : @ => \x : (F A) => x);
