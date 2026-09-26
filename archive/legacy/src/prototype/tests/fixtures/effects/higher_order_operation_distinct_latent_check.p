pureRequest := (#.scope_text &{ #"pure"; });

effectRequest := (#.scope_text &((#.print #"inner")));

main := effectRequest;
