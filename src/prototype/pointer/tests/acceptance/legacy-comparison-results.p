// Import the unchanged legacy comparison, not a copy specialized for this test.
import Nat;
import LE;
import Either;
import compareNat;

zero := Nat.zero;
one := Nat.succ zero;
two := Nat.succ one;
baseLeft := compareNat zero two;
baseLeftExpected := (Either (LE zero two) (LE two zero)).left (LE.zero two);
baseRight := compareNat two zero;
baseRightExpected := (Either (LE two zero) (LE zero two)).right (LE.zero two);
less := compareNat one two;
lessExpected := (Either (LE one two) (LE two one)).left
	(LE.succ zero one (LE.zero one));
greater := compareNat two one;
greaterExpected := (Either (LE two one) (LE one two)).right
	(LE.succ zero one (LE.zero one));
same := compareNat two two;
sameExpected := (Either (LE two two) (LE two two)).left
	(LE.succ one one (LE.succ zero zero (LE.zero zero)));
