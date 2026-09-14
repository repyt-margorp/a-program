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
