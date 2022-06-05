# ZLibraries

My collection of general tools and reusable code.

## CopyByteRange
Simply copies bytes from one file/offset into another file/offset. 

## DupeScanner
Performs two functions.
1) Performs a binary diff between two sets of files. (Single file or entire folder structure.)
2) Performs a dupe search within a set of files. (Single file or entire folder structure.)

This is done by first "Indexing", breaking up source data into fixed size blocks and computing fast (Rabin Karp) rolling hashes and slow (SHA256) hashes for each block.
Once indexed the second set of data is searched on every byte offset for any matching blocks from the indexed data. Rolling hashes are done for fast rejection, SHA256 hashes done for true matches.

## FileGen
Generates one or many files filled with either specific value, cyclical values or random values. Particularly useful for generating data sets.
