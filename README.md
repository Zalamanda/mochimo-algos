# Mochimo Algorithms
Algorithms used on the Mochimo Cryptocurrency Network re-written in C for ease of use in further works.

### Basic Usage
There are 3 standardized Mochimo Algorithm functions:
 - `*_check()`, checks the validity of a block trailer,
 - `*_solve()`, initializes a mining state, and
 - `*_generate()`, generates valid haiku output using the specified algorithm.

[Trigg Algorithm](src/trigg.c)...
```c
int trigg_check(const BTRAILER *bt);
void trigg_solve(TRIGG_ALGO *T, const BTRAILER *bt);
int trigg_generate(TRIGG_ALGO *T, void *out);

/****************************/
/* Recommended Mining Usage */

BTRAILER bt;
TRIGG_ALGO T;
int n;

/* ... prep block trailer ... */

trigg_solve(&T, &bt);
for(n = 0; trigg_generate(&T, bt.nonce); n++) {
   if(n & 0xfffff == 0) {
      /* report hash per second */
      n = 0;
   }
}

if(trigg_check(&bt)) {
   /* solved! */
}
```

[Peach Algorithm](src/peach.c)...
```c
int peach_check(const BTRAILER *bt);
int peach_solve(PEACH_ALGO *P, const BTRAILER *bt);
int peach_generate(PEACH_ALGO *P, void *out);

/****************************/
/* Recommended Mining Usage */

BTRAILER bt;
PEACH_ALGO P;
int n;

/* ... prep block trailer ... */

if(peach_solve(&P, &bt)) {
   printf("Error: could not initialize Peach algorithm. Check memory usage.");
   return;
}

for(n = 0; peach_generate(&P, bt.nonce); n++) {
   if(n & 0xfffff == 0) {
      /* report hash per second */
      n = 0;
   }
}

if(peach_check(&bt)) {
   /* solved! */
}
```

### Example usage
The [Algorithm tests](test/algotest.c) file is provided as an example of basic usage and testing, which checks the algorithms against Known Answer Tests (KATs) and the underlying mining functionality.

#### Self Compilation and Execution:
Self compilation helper files, [testWIN.bat](testWIN.bat) & [testUNIX.sh](testUNIX.sh), are provided for easy compilation and execution of the [Algorithm tests](test/algotest.c) file.  
> testWIN.bat; Requires `Microsoft Visual Studio 2017 Community Edition` installed. Tested on x86_64 architecture running Windows 10 Pro v10.0.18362.  
> testUNIX.sh; Requires the `build-essential` package installed. Tested on x86_64 architecture running Ubuntu 16.04.1.

### More information
See individual source files for descriptions of the file, data type structures, compiler MACRO expansions, and function operations.

## License
These works are modifications to the original [Mochimo Cryptocurrency Network's source](https://github.com/mochimodev/mochimo), made with the intent to benfit the Mochimo Network. This repository is released under the same MPL2.0 derivative open source [LICENSE](LICENSE.PDF) released with the original source.  
Please read and understand this license before use.
