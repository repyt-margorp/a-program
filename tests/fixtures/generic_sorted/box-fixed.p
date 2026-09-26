Bool := @{ true:*; false:*; };
Box := @\b:Bool => { yes:* Bool.true; no:* Bool.false; };
Graph := \f:Bool->Bool => @\x:Bool => @\y:Bool => { run:(a:Bool)->* a (f a); };
bridge := \f:Bool->Bool => \cert:(x:Bool)->Box (f x) => \x:Bool => \g:Graph f x Bool.true => g @run a => cert a;
bridge :: (f:Bool->Bool)->((x:Bool)->Box (f x))->(x:Bool)->Graph f x Bool.true->Box Bool.true;
