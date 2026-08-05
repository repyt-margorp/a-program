main := ({
	text : #.Text := perform (#.scope_text &(perform (#.print #"inner")));
	perform (#.print text);
})
	@#.return y => y
	@#.scope_text delayed k => k #"handled"
	@#.print text k => k text;
