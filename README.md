# BTree Indexing

## Introduction
Implement BTree Indexing. Read a text file containing data and build the index, treating the first n columns as the key.
The length of the key will be specified when you create the index and must be stored in the metadata.
Long (8-byte) record address for your pointers, "pointers" are the byte offset in the text file of the data record

The structure of the index is as follows:  First 256 bytes: Name of the text file you have indexed. This must be blank-filled on the right. You may need other metadata in this first block (such as the key length), hence first 1K data blocked for the purpose. Included input file is btree.txt. Assuming the name of program is INDEX.

## Usage
### Creating an Index
Parameters - key length, a text file name, and an output index.  If output index exists, overwrite it.
Duplicates should not be inserted

	$ INDEX –create <length> <input file> <output file>

### Find a record by key
Displays the entire record, including the key, and gives its position, in bytes, within the file.
If the key is not in your index, the program must give a message to that effect.
If the key supplied is longer than the key length specified for the index, truncate it. If it is shorter, pad with blanks on the right.

	$ INDEX -find <index filename> <key>

### Insert a new text record
First n bytes must contain a unique key, where n was specified when you created the index. 
If the key is not in the index, write the record at the end of the data file (remembering the position, since you will need it), then inserting the key into your index structure.
If the key is already in the index, return a message to that effect and do not insert the record.
The record needs to be in quotes because it could contain spaces and other punctuation.

	$ INDEX -insert <index filename> "new text line to be inserted."

### List sequential records
Show the record containing the given key and the next n records following it, if there are that many.
If no record exists with the given key, show records that contain the next-larger key and give a message that the actual key was not found.
Show the records in order by key and their position within the text file.

	$ INDEX -list <index filename> <starting key> <count>
	
## Example

	$ INDEX –create 14 CS6360Asg5.txt btree.index
	
	$ INDEX -find btree.index 64541668700164B
	$ At 2127, record:  64541668700164B ANESTH, BIOPSY OF NOSE
	
	$ INDEX -insert btree.index "64541668700164B Some new Record"
	
	$ INDEX -list btree.index "64541668700164B" 10
	