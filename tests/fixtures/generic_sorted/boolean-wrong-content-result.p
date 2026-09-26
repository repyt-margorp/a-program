// A proof for the ordinary sorted result cannot claim that every result is nil.
wrong_content_result := \xs:List Bool => quick_content_result Bool &bool_le xs;
wrong_content_result :: (xs:List Bool)->permutation Bool xs (List Bool).nil;
