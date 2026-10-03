add := \x : #Int32 => #int_add x #7;
negative := \x : #Int32 => #int_neg x;
observe := \x : #Int32 => {
	#print (#int_to_text (once32 add x)); #print #"|";
	#print (#int_to_text (twice32 add x)); #print #"|";
	#print (#int_to_text (compose32 add negative x)); #print #"|";
	#print (#int_to_text (captured32 add x)); #print #"|";
	#print (#int_to_text (unused32 add x)); #print #"\n";
};
main := { observe #0; observe #17; observe #-2147483648; observe #2147483647; };
