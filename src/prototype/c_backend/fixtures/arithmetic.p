main := {
	#print (#int_to_text (#int_add #2147483647 #1));
	#print #":";
	#print (#int_to_text (#int_neg #-2147483648));
	#print #":";
	#print (#int_to_text (#int_mul #2147483647 #2147483647));
	#print #":";
	#print (#int_to_text (#int_sub #-2147483648 #1));
};
