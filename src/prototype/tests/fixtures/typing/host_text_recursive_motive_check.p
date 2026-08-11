TextTree := @{
	text : #.Text -> *;
	left : * -> *;
	right : * -> *;
};

firstText := \tree : TextTree =>
	tree @text value => value
		@left child => *child
		@right child => *child;
firstText :: TextTree -> #.Text;

sample := TextTree.left (TextTree.right (TextTree.text #"ok"));
main := firstText sample;
expected := { #"ok"; };
