wrong_content := \xs:List Bool => \ys:List Bool => \g:@quickSort Bool &bool_le xs ys =>
	quick_content Bool &bool_le xs ys g;
wrong_content :: (xs:List Bool)->(ys:List Bool)->@quickSort Bool &bool_le xs ys->permutation Bool xs (List Bool).nil;
