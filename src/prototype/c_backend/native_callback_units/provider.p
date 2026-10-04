offset32 := \offset : #Int32 => \n : #Int32 => #int_add (#int_neg n) offset;
offset64 := \offset : #Int64 => \n : #Int64 => #int64_add (#int64_neg n) offset;
subtract32 := \left : #Int32 => \right : #Int32 => #int_sub left right;
subtract64 := \left : #Int64 => \right : #Int64 => #int64_sub left right;
reference := {
	#print (#int_to_text (offset32 #0 #-2147483648)); #print #"|";
	#print (#int_to_text (offset32 #-1 #1)); #print #"|";
	#print (#int_to_text (subtract32 #-2147483648 #1)); #print #"|";
	#print (#int_to_text (subtract32 #2147483647 #-1)); #print #"|";
};
