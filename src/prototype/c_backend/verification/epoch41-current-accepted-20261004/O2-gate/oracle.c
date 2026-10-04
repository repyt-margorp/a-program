#include "mockup.h"
#include <assert.h>
static uint32_t fingerprint(const uint32_t *a, size_t n) { uint32_t code = 0; assert(n <= 4);
while (n) { assert(a[n-1] < 4); code = code * 5 + a[--n] + 1; } return code; }
int main(void) {
{ uint32_t a[4] = {0,0,0,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,0,out,4,&n,0) && n == 0 && fingerprint(out,n) == UINT32_C(0)); }
{ uint32_t a[4] = {0,0,0,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,1,out,4,&n,0) && n == 1 && fingerprint(out,n) == UINT32_C(1)); }
{ uint32_t a[4] = {1,0,0,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,1,out,4,&n,0) && n == 1 && fingerprint(out,n) == UINT32_C(2)); }
{ uint32_t a[4] = {2,0,0,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,1,out,4,&n,0) && n == 1 && fingerprint(out,n) == UINT32_C(3)); }
{ uint32_t a[4] = {3,0,0,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,1,out,4,&n,0) && n == 1 && fingerprint(out,n) == UINT32_C(4)); }
{ uint32_t a[4] = {0,0,0,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,2,out,4,&n,0) && n == 2 && fingerprint(out,n) == UINT32_C(6)); }
{ uint32_t a[4] = {1,0,0,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,2,out,4,&n,0) && n == 2 && fingerprint(out,n) == UINT32_C(11)); }
{ uint32_t a[4] = {2,0,0,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,2,out,4,&n,0) && n == 2 && fingerprint(out,n) == UINT32_C(16)); }
{ uint32_t a[4] = {3,0,0,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,2,out,4,&n,0) && n == 2 && fingerprint(out,n) == UINT32_C(21)); }
{ uint32_t a[4] = {0,1,0,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,2,out,4,&n,0) && n == 2 && fingerprint(out,n) == UINT32_C(11)); }
{ uint32_t a[4] = {1,1,0,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,2,out,4,&n,0) && n == 2 && fingerprint(out,n) == UINT32_C(12)); }
{ uint32_t a[4] = {2,1,0,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,2,out,4,&n,0) && n == 2 && fingerprint(out,n) == UINT32_C(17)); }
{ uint32_t a[4] = {3,1,0,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,2,out,4,&n,0) && n == 2 && fingerprint(out,n) == UINT32_C(22)); }
{ uint32_t a[4] = {0,2,0,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,2,out,4,&n,0) && n == 2 && fingerprint(out,n) == UINT32_C(16)); }
{ uint32_t a[4] = {1,2,0,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,2,out,4,&n,0) && n == 2 && fingerprint(out,n) == UINT32_C(17)); }
{ uint32_t a[4] = {2,2,0,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,2,out,4,&n,0) && n == 2 && fingerprint(out,n) == UINT32_C(18)); }
{ uint32_t a[4] = {3,2,0,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,2,out,4,&n,0) && n == 2 && fingerprint(out,n) == UINT32_C(23)); }
{ uint32_t a[4] = {0,3,0,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,2,out,4,&n,0) && n == 2 && fingerprint(out,n) == UINT32_C(21)); }
{ uint32_t a[4] = {1,3,0,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,2,out,4,&n,0) && n == 2 && fingerprint(out,n) == UINT32_C(22)); }
{ uint32_t a[4] = {2,3,0,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,2,out,4,&n,0) && n == 2 && fingerprint(out,n) == UINT32_C(23)); }
{ uint32_t a[4] = {3,3,0,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,2,out,4,&n,0) && n == 2 && fingerprint(out,n) == UINT32_C(24)); }
{ uint32_t a[4] = {0,0,0,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(31)); }
{ uint32_t a[4] = {1,0,0,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(56)); }
{ uint32_t a[4] = {2,0,0,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(81)); }
{ uint32_t a[4] = {3,0,0,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(106)); }
{ uint32_t a[4] = {0,1,0,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(56)); }
{ uint32_t a[4] = {1,1,0,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(61)); }
{ uint32_t a[4] = {2,1,0,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(86)); }
{ uint32_t a[4] = {3,1,0,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(111)); }
{ uint32_t a[4] = {0,2,0,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(81)); }
{ uint32_t a[4] = {1,2,0,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(86)); }
{ uint32_t a[4] = {2,2,0,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(91)); }
{ uint32_t a[4] = {3,2,0,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(116)); }
{ uint32_t a[4] = {0,3,0,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(106)); }
{ uint32_t a[4] = {1,3,0,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(111)); }
{ uint32_t a[4] = {2,3,0,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(116)); }
{ uint32_t a[4] = {3,3,0,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(121)); }
{ uint32_t a[4] = {0,0,1,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(56)); }
{ uint32_t a[4] = {1,0,1,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(61)); }
{ uint32_t a[4] = {2,0,1,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(86)); }
{ uint32_t a[4] = {3,0,1,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(111)); }
{ uint32_t a[4] = {0,1,1,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(61)); }
{ uint32_t a[4] = {1,1,1,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(62)); }
{ uint32_t a[4] = {2,1,1,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(87)); }
{ uint32_t a[4] = {3,1,1,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(112)); }
{ uint32_t a[4] = {0,2,1,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(86)); }
{ uint32_t a[4] = {1,2,1,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(87)); }
{ uint32_t a[4] = {2,2,1,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(92)); }
{ uint32_t a[4] = {3,2,1,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(117)); }
{ uint32_t a[4] = {0,3,1,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(111)); }
{ uint32_t a[4] = {1,3,1,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(112)); }
{ uint32_t a[4] = {2,3,1,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(117)); }
{ uint32_t a[4] = {3,3,1,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(122)); }
{ uint32_t a[4] = {0,0,2,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(81)); }
{ uint32_t a[4] = {1,0,2,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(86)); }
{ uint32_t a[4] = {2,0,2,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(91)); }
{ uint32_t a[4] = {3,0,2,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(116)); }
{ uint32_t a[4] = {0,1,2,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(86)); }
{ uint32_t a[4] = {1,1,2,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(87)); }
{ uint32_t a[4] = {2,1,2,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(92)); }
{ uint32_t a[4] = {3,1,2,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(117)); }
{ uint32_t a[4] = {0,2,2,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(91)); }
{ uint32_t a[4] = {1,2,2,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(92)); }
{ uint32_t a[4] = {2,2,2,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(93)); }
{ uint32_t a[4] = {3,2,2,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(118)); }
{ uint32_t a[4] = {0,3,2,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(116)); }
{ uint32_t a[4] = {1,3,2,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(117)); }
{ uint32_t a[4] = {2,3,2,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(118)); }
{ uint32_t a[4] = {3,3,2,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(123)); }
{ uint32_t a[4] = {0,0,3,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(106)); }
{ uint32_t a[4] = {1,0,3,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(111)); }
{ uint32_t a[4] = {2,0,3,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(116)); }
{ uint32_t a[4] = {3,0,3,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(121)); }
{ uint32_t a[4] = {0,1,3,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(111)); }
{ uint32_t a[4] = {1,1,3,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(112)); }
{ uint32_t a[4] = {2,1,3,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(117)); }
{ uint32_t a[4] = {3,1,3,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(122)); }
{ uint32_t a[4] = {0,2,3,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(116)); }
{ uint32_t a[4] = {1,2,3,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(117)); }
{ uint32_t a[4] = {2,2,3,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(118)); }
{ uint32_t a[4] = {3,2,3,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(123)); }
{ uint32_t a[4] = {0,3,3,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(121)); }
{ uint32_t a[4] = {1,3,3,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(122)); }
{ uint32_t a[4] = {2,3,3,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(123)); }
{ uint32_t a[4] = {3,3,3,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,3,out,4,&n,0) && n == 3 && fingerprint(out,n) == UINT32_C(124)); }
{ uint32_t a[4] = {0,0,0,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(156)); }
{ uint32_t a[4] = {1,0,0,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(281)); }
{ uint32_t a[4] = {2,0,0,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(406)); }
{ uint32_t a[4] = {3,0,0,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(531)); }
{ uint32_t a[4] = {0,1,0,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(281)); }
{ uint32_t a[4] = {1,1,0,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(306)); }
{ uint32_t a[4] = {2,1,0,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(431)); }
{ uint32_t a[4] = {3,1,0,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(556)); }
{ uint32_t a[4] = {0,2,0,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(406)); }
{ uint32_t a[4] = {1,2,0,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(431)); }
{ uint32_t a[4] = {2,2,0,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(456)); }
{ uint32_t a[4] = {3,2,0,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(581)); }
{ uint32_t a[4] = {0,3,0,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(531)); }
{ uint32_t a[4] = {1,3,0,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(556)); }
{ uint32_t a[4] = {2,3,0,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(581)); }
{ uint32_t a[4] = {3,3,0,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(606)); }
{ uint32_t a[4] = {0,0,1,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(281)); }
{ uint32_t a[4] = {1,0,1,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(306)); }
{ uint32_t a[4] = {2,0,1,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(431)); }
{ uint32_t a[4] = {3,0,1,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(556)); }
{ uint32_t a[4] = {0,1,1,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(306)); }
{ uint32_t a[4] = {1,1,1,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(311)); }
{ uint32_t a[4] = {2,1,1,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(436)); }
{ uint32_t a[4] = {3,1,1,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(561)); }
{ uint32_t a[4] = {0,2,1,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(431)); }
{ uint32_t a[4] = {1,2,1,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(436)); }
{ uint32_t a[4] = {2,2,1,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(461)); }
{ uint32_t a[4] = {3,2,1,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(586)); }
{ uint32_t a[4] = {0,3,1,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(556)); }
{ uint32_t a[4] = {1,3,1,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(561)); }
{ uint32_t a[4] = {2,3,1,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(586)); }
{ uint32_t a[4] = {3,3,1,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(611)); }
{ uint32_t a[4] = {0,0,2,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(406)); }
{ uint32_t a[4] = {1,0,2,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(431)); }
{ uint32_t a[4] = {2,0,2,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(456)); }
{ uint32_t a[4] = {3,0,2,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(581)); }
{ uint32_t a[4] = {0,1,2,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(431)); }
{ uint32_t a[4] = {1,1,2,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(436)); }
{ uint32_t a[4] = {2,1,2,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(461)); }
{ uint32_t a[4] = {3,1,2,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(586)); }
{ uint32_t a[4] = {0,2,2,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(456)); }
{ uint32_t a[4] = {1,2,2,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(461)); }
{ uint32_t a[4] = {2,2,2,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(466)); }
{ uint32_t a[4] = {3,2,2,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(591)); }
{ uint32_t a[4] = {0,3,2,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(581)); }
{ uint32_t a[4] = {1,3,2,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(586)); }
{ uint32_t a[4] = {2,3,2,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(591)); }
{ uint32_t a[4] = {3,3,2,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(616)); }
{ uint32_t a[4] = {0,0,3,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(531)); }
{ uint32_t a[4] = {1,0,3,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(556)); }
{ uint32_t a[4] = {2,0,3,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(581)); }
{ uint32_t a[4] = {3,0,3,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(606)); }
{ uint32_t a[4] = {0,1,3,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(556)); }
{ uint32_t a[4] = {1,1,3,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(561)); }
{ uint32_t a[4] = {2,1,3,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(586)); }
{ uint32_t a[4] = {3,1,3,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(611)); }
{ uint32_t a[4] = {0,2,3,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(581)); }
{ uint32_t a[4] = {1,2,3,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(586)); }
{ uint32_t a[4] = {2,2,3,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(591)); }
{ uint32_t a[4] = {3,2,3,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(616)); }
{ uint32_t a[4] = {0,3,3,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(606)); }
{ uint32_t a[4] = {1,3,3,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(611)); }
{ uint32_t a[4] = {2,3,3,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(616)); }
{ uint32_t a[4] = {3,3,3,0}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(621)); }
{ uint32_t a[4] = {0,0,0,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(281)); }
{ uint32_t a[4] = {1,0,0,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(306)); }
{ uint32_t a[4] = {2,0,0,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(431)); }
{ uint32_t a[4] = {3,0,0,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(556)); }
{ uint32_t a[4] = {0,1,0,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(306)); }
{ uint32_t a[4] = {1,1,0,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(311)); }
{ uint32_t a[4] = {2,1,0,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(436)); }
{ uint32_t a[4] = {3,1,0,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(561)); }
{ uint32_t a[4] = {0,2,0,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(431)); }
{ uint32_t a[4] = {1,2,0,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(436)); }
{ uint32_t a[4] = {2,2,0,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(461)); }
{ uint32_t a[4] = {3,2,0,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(586)); }
{ uint32_t a[4] = {0,3,0,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(556)); }
{ uint32_t a[4] = {1,3,0,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(561)); }
{ uint32_t a[4] = {2,3,0,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(586)); }
{ uint32_t a[4] = {3,3,0,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(611)); }
{ uint32_t a[4] = {0,0,1,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(306)); }
{ uint32_t a[4] = {1,0,1,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(311)); }
{ uint32_t a[4] = {2,0,1,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(436)); }
{ uint32_t a[4] = {3,0,1,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(561)); }
{ uint32_t a[4] = {0,1,1,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(311)); }
{ uint32_t a[4] = {1,1,1,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(312)); }
{ uint32_t a[4] = {2,1,1,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(437)); }
{ uint32_t a[4] = {3,1,1,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(562)); }
{ uint32_t a[4] = {0,2,1,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(436)); }
{ uint32_t a[4] = {1,2,1,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(437)); }
{ uint32_t a[4] = {2,2,1,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(462)); }
{ uint32_t a[4] = {3,2,1,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(587)); }
{ uint32_t a[4] = {0,3,1,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(561)); }
{ uint32_t a[4] = {1,3,1,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(562)); }
{ uint32_t a[4] = {2,3,1,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(587)); }
{ uint32_t a[4] = {3,3,1,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(612)); }
{ uint32_t a[4] = {0,0,2,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(431)); }
{ uint32_t a[4] = {1,0,2,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(436)); }
{ uint32_t a[4] = {2,0,2,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(461)); }
{ uint32_t a[4] = {3,0,2,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(586)); }
{ uint32_t a[4] = {0,1,2,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(436)); }
{ uint32_t a[4] = {1,1,2,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(437)); }
{ uint32_t a[4] = {2,1,2,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(462)); }
{ uint32_t a[4] = {3,1,2,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(587)); }
{ uint32_t a[4] = {0,2,2,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(461)); }
{ uint32_t a[4] = {1,2,2,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(462)); }
{ uint32_t a[4] = {2,2,2,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(467)); }
{ uint32_t a[4] = {3,2,2,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(592)); }
{ uint32_t a[4] = {0,3,2,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(586)); }
{ uint32_t a[4] = {1,3,2,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(587)); }
{ uint32_t a[4] = {2,3,2,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(592)); }
{ uint32_t a[4] = {3,3,2,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(617)); }
{ uint32_t a[4] = {0,0,3,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(556)); }
{ uint32_t a[4] = {1,0,3,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(561)); }
{ uint32_t a[4] = {2,0,3,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(586)); }
{ uint32_t a[4] = {3,0,3,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(611)); }
{ uint32_t a[4] = {0,1,3,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(561)); }
{ uint32_t a[4] = {1,1,3,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(562)); }
{ uint32_t a[4] = {2,1,3,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(587)); }
{ uint32_t a[4] = {3,1,3,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(612)); }
{ uint32_t a[4] = {0,2,3,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(586)); }
{ uint32_t a[4] = {1,2,3,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(587)); }
{ uint32_t a[4] = {2,2,3,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(592)); }
{ uint32_t a[4] = {3,2,3,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(617)); }
{ uint32_t a[4] = {0,3,3,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(611)); }
{ uint32_t a[4] = {1,3,3,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(612)); }
{ uint32_t a[4] = {2,3,3,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(617)); }
{ uint32_t a[4] = {3,3,3,1}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(622)); }
{ uint32_t a[4] = {0,0,0,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(406)); }
{ uint32_t a[4] = {1,0,0,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(431)); }
{ uint32_t a[4] = {2,0,0,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(456)); }
{ uint32_t a[4] = {3,0,0,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(581)); }
{ uint32_t a[4] = {0,1,0,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(431)); }
{ uint32_t a[4] = {1,1,0,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(436)); }
{ uint32_t a[4] = {2,1,0,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(461)); }
{ uint32_t a[4] = {3,1,0,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(586)); }
{ uint32_t a[4] = {0,2,0,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(456)); }
{ uint32_t a[4] = {1,2,0,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(461)); }
{ uint32_t a[4] = {2,2,0,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(466)); }
{ uint32_t a[4] = {3,2,0,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(591)); }
{ uint32_t a[4] = {0,3,0,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(581)); }
{ uint32_t a[4] = {1,3,0,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(586)); }
{ uint32_t a[4] = {2,3,0,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(591)); }
{ uint32_t a[4] = {3,3,0,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(616)); }
{ uint32_t a[4] = {0,0,1,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(431)); }
{ uint32_t a[4] = {1,0,1,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(436)); }
{ uint32_t a[4] = {2,0,1,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(461)); }
{ uint32_t a[4] = {3,0,1,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(586)); }
{ uint32_t a[4] = {0,1,1,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(436)); }
{ uint32_t a[4] = {1,1,1,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(437)); }
{ uint32_t a[4] = {2,1,1,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(462)); }
{ uint32_t a[4] = {3,1,1,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(587)); }
{ uint32_t a[4] = {0,2,1,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(461)); }
{ uint32_t a[4] = {1,2,1,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(462)); }
{ uint32_t a[4] = {2,2,1,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(467)); }
{ uint32_t a[4] = {3,2,1,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(592)); }
{ uint32_t a[4] = {0,3,1,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(586)); }
{ uint32_t a[4] = {1,3,1,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(587)); }
{ uint32_t a[4] = {2,3,1,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(592)); }
{ uint32_t a[4] = {3,3,1,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(617)); }
{ uint32_t a[4] = {0,0,2,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(456)); }
{ uint32_t a[4] = {1,0,2,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(461)); }
{ uint32_t a[4] = {2,0,2,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(466)); }
{ uint32_t a[4] = {3,0,2,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(591)); }
{ uint32_t a[4] = {0,1,2,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(461)); }
{ uint32_t a[4] = {1,1,2,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(462)); }
{ uint32_t a[4] = {2,1,2,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(467)); }
{ uint32_t a[4] = {3,1,2,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(592)); }
{ uint32_t a[4] = {0,2,2,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(466)); }
{ uint32_t a[4] = {1,2,2,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(467)); }
{ uint32_t a[4] = {2,2,2,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(468)); }
{ uint32_t a[4] = {3,2,2,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(593)); }
{ uint32_t a[4] = {0,3,2,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(591)); }
{ uint32_t a[4] = {1,3,2,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(592)); }
{ uint32_t a[4] = {2,3,2,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(593)); }
{ uint32_t a[4] = {3,3,2,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(618)); }
{ uint32_t a[4] = {0,0,3,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(581)); }
{ uint32_t a[4] = {1,0,3,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(586)); }
{ uint32_t a[4] = {2,0,3,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(591)); }
{ uint32_t a[4] = {3,0,3,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(616)); }
{ uint32_t a[4] = {0,1,3,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(586)); }
{ uint32_t a[4] = {1,1,3,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(587)); }
{ uint32_t a[4] = {2,1,3,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(592)); }
{ uint32_t a[4] = {3,1,3,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(617)); }
{ uint32_t a[4] = {0,2,3,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(591)); }
{ uint32_t a[4] = {1,2,3,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(592)); }
{ uint32_t a[4] = {2,2,3,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(593)); }
{ uint32_t a[4] = {3,2,3,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(618)); }
{ uint32_t a[4] = {0,3,3,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(616)); }
{ uint32_t a[4] = {1,3,3,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(617)); }
{ uint32_t a[4] = {2,3,3,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(618)); }
{ uint32_t a[4] = {3,3,3,2}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(623)); }
{ uint32_t a[4] = {0,0,0,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(531)); }
{ uint32_t a[4] = {1,0,0,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(556)); }
{ uint32_t a[4] = {2,0,0,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(581)); }
{ uint32_t a[4] = {3,0,0,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(606)); }
{ uint32_t a[4] = {0,1,0,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(556)); }
{ uint32_t a[4] = {1,1,0,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(561)); }
{ uint32_t a[4] = {2,1,0,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(586)); }
{ uint32_t a[4] = {3,1,0,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(611)); }
{ uint32_t a[4] = {0,2,0,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(581)); }
{ uint32_t a[4] = {1,2,0,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(586)); }
{ uint32_t a[4] = {2,2,0,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(591)); }
{ uint32_t a[4] = {3,2,0,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(616)); }
{ uint32_t a[4] = {0,3,0,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(606)); }
{ uint32_t a[4] = {1,3,0,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(611)); }
{ uint32_t a[4] = {2,3,0,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(616)); }
{ uint32_t a[4] = {3,3,0,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(621)); }
{ uint32_t a[4] = {0,0,1,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(556)); }
{ uint32_t a[4] = {1,0,1,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(561)); }
{ uint32_t a[4] = {2,0,1,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(586)); }
{ uint32_t a[4] = {3,0,1,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(611)); }
{ uint32_t a[4] = {0,1,1,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(561)); }
{ uint32_t a[4] = {1,1,1,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(562)); }
{ uint32_t a[4] = {2,1,1,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(587)); }
{ uint32_t a[4] = {3,1,1,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(612)); }
{ uint32_t a[4] = {0,2,1,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(586)); }
{ uint32_t a[4] = {1,2,1,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(587)); }
{ uint32_t a[4] = {2,2,1,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(592)); }
{ uint32_t a[4] = {3,2,1,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(617)); }
{ uint32_t a[4] = {0,3,1,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(611)); }
{ uint32_t a[4] = {1,3,1,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(612)); }
{ uint32_t a[4] = {2,3,1,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(617)); }
{ uint32_t a[4] = {3,3,1,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(622)); }
{ uint32_t a[4] = {0,0,2,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(581)); }
{ uint32_t a[4] = {1,0,2,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(586)); }
{ uint32_t a[4] = {2,0,2,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(591)); }
{ uint32_t a[4] = {3,0,2,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(616)); }
{ uint32_t a[4] = {0,1,2,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(586)); }
{ uint32_t a[4] = {1,1,2,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(587)); }
{ uint32_t a[4] = {2,1,2,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(592)); }
{ uint32_t a[4] = {3,1,2,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(617)); }
{ uint32_t a[4] = {0,2,2,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(591)); }
{ uint32_t a[4] = {1,2,2,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(592)); }
{ uint32_t a[4] = {2,2,2,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(593)); }
{ uint32_t a[4] = {3,2,2,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(618)); }
{ uint32_t a[4] = {0,3,2,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(616)); }
{ uint32_t a[4] = {1,3,2,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(617)); }
{ uint32_t a[4] = {2,3,2,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(618)); }
{ uint32_t a[4] = {3,3,2,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(623)); }
{ uint32_t a[4] = {0,0,3,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(606)); }
{ uint32_t a[4] = {1,0,3,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(611)); }
{ uint32_t a[4] = {2,0,3,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(616)); }
{ uint32_t a[4] = {3,0,3,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(621)); }
{ uint32_t a[4] = {0,1,3,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(611)); }
{ uint32_t a[4] = {1,1,3,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(612)); }
{ uint32_t a[4] = {2,1,3,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(617)); }
{ uint32_t a[4] = {3,1,3,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(622)); }
{ uint32_t a[4] = {0,2,3,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(616)); }
{ uint32_t a[4] = {1,2,3,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(617)); }
{ uint32_t a[4] = {2,2,3,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(618)); }
{ uint32_t a[4] = {3,2,3,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(623)); }
{ uint32_t a[4] = {0,3,3,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(621)); }
{ uint32_t a[4] = {1,3,3,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(622)); }
{ uint32_t a[4] = {2,3,3,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(623)); }
{ uint32_t a[4] = {3,3,3,3}, out[4]; size_t n;
assert(!qs_mockup_sort(a,4,out,4,&n,0) && n == 4 && fingerprint(out,n) == UINT32_C(624)); }
return 0; }
