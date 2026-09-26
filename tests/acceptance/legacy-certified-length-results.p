import Nat;
import NatList;
import length;
import lengthCertified;
import LengthResult;
import main;

zero := Nat.zero;
one := Nat.succ zero;
two := Nat.succ one;
three := Nat.succ two;
sample := NatList.cons two (NatList.cons zero (NatList.cons one NatList.nil));
original := main;
empty := length NatList.nil;
many := length sample;
certificate := lengthCertified sample;
certificate :: LengthResult sample;
