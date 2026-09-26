emit := #print;
main := (#print #"hello") @#print req k => k req @#return x => x;
expected := { #"hello"; };
alias := (emit #"hello") @emit req k => k req @#return x => x;
discard := (#print #"ignored") @#print req k => #"hello" @#return x => x;
twice := (#print #"unused")
	@#print req k => { k #"first"; k #"hello"; }
	@#return x => x;
forward := ((#print #"hello") @#return x => x)
	@#print req k => k req @#return x => x;
reemitted := ((#print #"hello") @#print req k => #print req @#return x => x)
	@#print req k => k req @#return x => x;
raw := #print #"hello";
arithmetic := (#print #"hello")
	@#print req k => { #int_add #1 #2; k req; }
	@#return x => x;
rawAnnotated := { x : #Text := #print #"hello"; x; };
annotated := rawAnnotated @#print req k => k req @#return x => x;
lastAnnotated := ({ x : #Text := #print #"hello"; })
	@#print req k => k req @#return x => x;
selectedAnnotated := ({ x : #Text := #print #"hello"; missing; }.x)
	@#print req k => k req @#return x => x;
TextAlias := #Text;
aliasAnnotated := ({ x : TextAlias := emit #"hello"; x; })
	@#print req k => k req @#return x => x;
computedAnnotated := ({ x : (\T : @ => T) #Text := #print #"hello"; x; })
	@#print req k => k req @#return x => x;
continuationAnnotated := (#print #"hello")
	@#print req k => { x : #Text := k req; x; }
	@#return x => x;
