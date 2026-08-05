pureRequest := perform (#.scope_text &{ #"pure"; });

effectRequest := perform (#.scope_text &(perform (#.print #"inner")));

main := effectRequest;
