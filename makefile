all: btree.cpp
	g++ -o btree btree.cpp
	
	$(info **************** Usage ****************)
	$(info create: -create <key length> <source file> <index file>)
	$(info search: -find <index file> <key>)
	$(info insert: -insert <source file> "row values")
	$(info ***************************************)
