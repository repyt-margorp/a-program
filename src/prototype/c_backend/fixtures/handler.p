main := ({ #print #"hidden"; #print #"also hidden"; })
	@#print req k => { #print #"outside"; k req; }
	@#return value => #print #"returned";
duplicated := ({ #print #"request"; #"done"; })
	@#print req k => { k req; k req; }
	@#return value => #print value;
forwarded := ({ #print #"forward"; } @#return value => value)
	@#print req k => { #print #"caught"; k req; }
	@#return value => value;
aborted := ({ #print #"stop"; #print #"unreachable"; })
	@#print req k => #"stopped"
	@#return value => value;
