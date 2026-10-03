add := \x : #Int32 => #int_add x #7;
main := #print (#int_to_text (once32 &add #0));
