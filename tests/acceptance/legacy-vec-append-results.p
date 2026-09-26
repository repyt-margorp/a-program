import Nat;
import Vec;
import append;
import main;

original := main;

one := Nat.succ Nat.zero;
two := Nat.succ one;
nil := (Vec Nat).nil;
left := (Vec Nat).cons Nat.zero Nat.zero nil;
right := (Vec Nat).cons Nat.zero one nil;
pair := (Vec Nat).cons one Nat.zero right;
longLeft := (Vec Nat).cons one one left;
triple := (Vec Nat).cons two one pair;
expected := (Vec Nat).cons one Nat.zero left;

empty := append Nat Nat.zero nil Nat.zero nil;
leftEmpty := append Nat Nat.zero nil one right;
rightEmpty := append Nat one right Nat.zero nil;
ordered := append Nat one left one right;
recursive := append Nat two longLeft one right;
