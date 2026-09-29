import Bool;
import List;
import quickSort;

le := \x : Bool => \y : Bool => x @false => Bool.true @true => y;
nil := (List Bool).nil;
sample := (List Bool).cons Bool.true
	((List Bool).cons Bool.false ((List Bool).cons Bool.true ((List Bool).cons Bool.false nil)));
emitList := \xs : List Bool => xs
	@nil => #print #""
	@cons h t => { h @false => #print #"F" @true => #print #"T"; *t; };
main := emitList (quickSort Bool &le sample);
