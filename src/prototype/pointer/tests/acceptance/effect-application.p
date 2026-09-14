Trace := \A : @ => @{ done : A -> *; step : #Text -> * -> *; };
TextTrace := Trace #Text;
second := \x : #Text => \y : #Text => y;

main := (second (#print #"a") (#print #"b"))
	@#print req k => TextTrace.step req (k req)
	@#return x => TextTrace.done x;
expected := TextTrace.step #"a" (TextTrace.step #"b" (TextTrace.done #"b"));

partial := second (#print #"a");
partialMain := ({ f := partial; #print #"between"; f (#print #"b"); })
	@#print req k => TextTrace.step req (k req)
	@#return x => TextTrace.done x;
partialExpected := TextTrace.step #"a"
	(TextTrace.step #"between" (TextTrace.step #"b" (TextTrace.done #"b")));

unusedMain := ({ partial; #"finished"; })
	@#print req k => TextTrace.step req (k req)
	@#return x => TextTrace.done x;
unusedExpected := TextTrace.step #"a" (TextTrace.done #"finished");

sharedMain := ({ f := partial; f (#print #"b"); f (#print #"c"); })
	@#print req k => TextTrace.step req (k req)
	@#return x => TextTrace.done x;
sharedExpected := TextTrace.step #"a"
	(TextTrace.step #"b" (TextTrace.step #"c" (TextTrace.done #"c")));

repeatedMain := ({ f := partial; g := partial; second (f #"b") (g #"c"); })
	@#print req k => TextTrace.step req (k req)
	@#return x => TextTrace.done x;
repeatedExpected := TextTrace.step #"a" (TextTrace.step #"a" (TextTrace.done #"c"));

PairText := @{ mk : #Text -> #Text -> *; };
PairTrace := Trace PairText;
constructorMain := (PairText.mk (#print #"left") (#print #"right"))
	@#print req k => PairTrace.step req (k req)
	@#return x => PairTrace.done x;
constructorExpected := PairTrace.step #"left"
	(PairTrace.step #"right" (PairTrace.done (PairText.mk #"left" #"right")));

nestedMain := (#print (#print #"twice"))
	@#print req k => TextTrace.step req (k req)
	@#return x => TextTrace.done x;
nestedExpected := TextTrace.step #"twice" (TextTrace.step #"twice" (TextTrace.done #"twice"));

make := \x : #Text => { #print x; \y : #Text => y; };
bodyMain := (make (#print #"a") (#print #"b"))
	@#print req k => TextTrace.step req (k req)
	@#return x => TextTrace.done x;
bodyExpected := TextTrace.step #"a"
	(TextTrace.step #"a" (TextTrace.step #"b" (TextTrace.done #"b")));

calleeMain := (({ #print #"before"; second; }) (#print #"a") (#print #"b"))
	@#print req k => TextTrace.step req (k req)
	@#return x => TextTrace.done x;
calleeExpected := TextTrace.step #"before"
	(TextTrace.step #"a" (TextTrace.step #"b" (TextTrace.done #"b")));

annotatedMain := ({ f : #Text -> #Text := partial; f (#print #"b"); })
	@#print req k => TextTrace.step req (k req)
	@#return x => TextTrace.done x;
