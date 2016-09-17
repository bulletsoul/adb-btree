#include <iostream>
#include <cstdlib>
#include <fstream>
#include <string.h>
#include <sstream>
#include <stdio.h>
#include <cctype>
#include <cstdio>
#include <ctime>


using namespace std;

const int MaxKeys = 10;   // max number of keys in a node
const int MaxKeysPlusOne = MaxKeys + 1;  // Order of the B Tree
const int MinKeys = 5;    // min number of keys in a node
const long NilPtr = -1L;   // the L indicates a long int
int KeyFieldMax = 15; // Max allowed length of key
const int KFMaxPlus1 = 15; //KeyFieldMax + 1;

// Max allowed length of Data in a line
// For now set it to 85, but ideally, should be the sum of all table column's size
int DataFieldMax = 85; 
int DFMaxPlus1 = DataFieldMax + 1;
const int NULLCHAR = '\0';  // NULL character used to mark end of a string
const int LineMax = 85;//KeyFieldMax + DFMaxPlus1;
int ListCount = 0;
long offset=0;

char* InputFileName;
char* IndexFileName;
char FileNameArray[255] = "";
char KeyLengthArray[255] = "";
char dummyData[255] = "";

typedef char StringType[LineMax];
typedef char KeyFieldType[KFMaxPlus1];

typedef long DataFieldType;


/**
* Input data file will be parsed as KeyField and DataField
*/
typedef struct
    {
        KeyFieldType KeyField;
        DataFieldType DataField;
    } ItemType;

typedef struct
    {
        int Count;               		// Number of keys stored in the current node
        ItemType Key[MaxKeys];   		// Warning: indexing starts at 0, not 1
        long Branch[MaxKeysPlusOne];   	// Fake pointers to child nodes
        bool     Deleted[MaxKeys];		// List of deleted nodes
    } NodeType;

/**
* Not really needed; In habit now
*/
class TableBaseClass
{
   public:
      virtual bool Empty(void) const = 0;
      virtual bool Insert(const ItemType & Item) = 0;
      virtual bool Retrieve(KeyFieldType SearchKey, ItemType & Item) = 0;
      virtual bool RetrieveList(KeyFieldType SearchKey, ItemType & Item) = 0;
   protected:
      fstream DataFile;   // the file stream for the table data
      long NumItems;      // number of records of type ItemType in the table
      char OpenMode;      // r or w (read or write) mode for the table
};

void Error(const char * msg)
{
    cerr << msg << endl;
    exit(1);
}

/**
* Class Declaration
* For better modularity, Can be shifted to another class file
*/
class BTTableClass: public TableBaseClass
{
    public:
    BTTableClass(char Mode, char * FileName);
    ~BTTableClass(void);
    bool Empty(void) const;
    bool Insert(const ItemType & Item);
    void DeleteItem(KeyFieldType SearchKey);
    bool Retrieve(KeyFieldType SearchKey, ItemType & Item);
    bool RetrieveList(KeyFieldType SearchKey, ItemType & Item);
    private:
    bool SearchNode(const KeyFieldType Target, int & location) const;
    void AddItem(const ItemType & NewItem, long NewRight,
        NodeType & Node, int Location);
    void Split(const ItemType & CurrentItem, long CurrentRight,
    long CurrentRoot, int Location, ItemType & NewItem,
        long & NewRight);
    void PushDown(const ItemType & CurrentItem, long CurrentRoot,
        bool & MoveUp, ItemType & NewItem, long & NewRight);
    long Root;       // fake pointer to the root node
    long NumNodes;   // number of nodes in the B-tree
    int NodeSize;    // number of bytes per node
    NodeType CurrentNode;   // storage for current node being worked on
};

/**
* Constructor for BTTableClass object
* Read mode opens the file skipping the first 1024 bytes which contains
* information about the index file name and key length
* Write mode opens the file, writes informaion about the above metadata
* and also creates a dummy node.
* Mode - char(r or w) to indicate read or write mode
* FileName  - char string holding the external filename
*/
BTTableClass::BTTableClass(char Mode, char * FileName)
{
    OpenMode = Mode;
    NodeSize = sizeof(NodeType);

    if (Mode == 'r')
    {
        DataFile.open(FileName, ios::in | ios::binary);
        if (DataFile.fail())
            Error("File cannot be opened");

        // Reading first 1024 bytes of the file
        DataFile.seekg(0, ios::beg);
        DataFile.read(FileNameArray, 256);
        DataFile.read(KeyLengthArray, 256);
        InputFileName = FileNameArray;
        KeyFieldMax = atoi(KeyLengthArray);

        DataFile.seekg(1024, ios::beg);

        DataFile.read(reinterpret_cast <char *> (&CurrentNode), NodeSize);
        if (DataFile.fail())
        {   // assume the Btree is empty if you cannot read from the file
            NumItems = 0;
            NumNodes = 0;
            Root = NilPtr;
        }
        else   // Node zero is not a normal node, it contains the following:
        {
            NumItems = CurrentNode.Branch[0];
            NumNodes = CurrentNode.Branch[1];
            Root = CurrentNode.Branch[2];
        }
    }
    else if (Mode == 'w')
    {
        DataFile.open(FileName, ios::in | ios::out | ios:: trunc |
        ios::binary);

        if (DataFile.fail())
            Error("File cannot be opened");

        // Writing First 1024 of Index File at the time of creation

        DataFile.seekp(0, ios::beg);
        std::ostringstream ss;
        ss << KeyFieldMax;
        strcpy(FileNameArray,InputFileName);
        ss.str().copy(KeyLengthArray,256,0);

        DataFile.write(FileNameArray, 256);
        DataFile.write(KeyLengthArray, 256);
        DataFile.write(dummyData, 256);
        DataFile.write(dummyData, 256);

        Root = NilPtr;
        NumItems = 0;
        NumNodes = 0;   // number does not include the special node zero
        CurrentNode.Branch[0] = NumItems;
        CurrentNode.Branch[1] = NumNodes;
        CurrentNode.Branch[2] = Root;
        DataFile.seekp(1024, ios::beg);
        DataFile.write(reinterpret_cast <char *> (&CurrentNode), NodeSize);
    }
    else
    {
		Error("Incorrect mode given to BTTableClass constructor");
	}
}

/**
* destructor for BTTableClass object
*/
BTTableClass::~BTTableClass(void)
{

    if (OpenMode == 'w')
    {
        //  Be sure to write out the updated node zero:
        CurrentNode.Branch[0] = NumItems;
        CurrentNode.Branch[1] = NumNodes;
        CurrentNode.Branch[2] = Root;

        DataFile.seekp(1024, ios::beg);
        DataFile.write(reinterpret_cast <char *> (&CurrentNode), NodeSize);
    }
    DataFile.close();
}

/**
* check if implicit table object is empty.
* Return - true if the table object is empty, false otherwise.
*/
bool BTTableClass::Empty(void) const
{   // we could read node zero, but this is faster:
    return (Root == NilPtr);
}

/**
* Target - The value to look for in the CurrentNode field
* Return - return true if found, false otherwise
* Location - index of where Target was found. If not found, index
* and index + 1 are the indices between which Target would fit.
*/
bool BTTableClass::SearchNode(const KeyFieldType Target,
   int & Location) const
{
    bool Found = false;
    
    if (strcmp(Target, CurrentNode.Key[0].KeyField) < 0)
        Location = -1;
    else
    { // do a sequential search, right to left:
        Location = CurrentNode.Count - 1;
        while ((strcmp(Target, CurrentNode.Key[Location].KeyField) < 0)
         && (Location > 0))
            Location--;

        if (strcmp(Target, CurrentNode.Key[Location].KeyField) == 0)
            Found = true;
    }
    return Found;
}

/**
* Adding Item to Node at index Location, and add NewRight
* as the branch just to the right of NewItem. This is
* done by moving the needed keys and branches right by 1 in order
* to clear out index Location for NewItem

* NewItem - Item to add to Node
* NewRight - Pseudopointer to right subtree below NewItem
* Node - The node to be added to
* Location - The index at which to add newItem
* Return - Updated node.
*/
void BTTableClass::AddItem(const ItemType & NewItem, long NewRight,
   NodeType & Node, int Location)
{
    int j;

    for (j = Node.Count; j > Location; j--)
    {
        Node.Key[j] = Node.Key[j - 1];
        Node.Branch[j + 1] = Node.Branch[j];
    }

    Node.Key[Location] = NewItem;
    Node.Branch[Location + 1] = NewRight;
    Node.Count++;
}

/**
* To split the node that CurrentRoot points to into 2 nodes
* CurrentItem - Item to be placed in node
* CurrentRight - Pseudopointer to the child to the right of CurrentItem
* CurrentRoot - Pseudopointer to the node to be split
* Location - Index of where CurrentItem should go in this node
* Return - NewItem - The item to be moved up into the parent node.
*          NewRight - a pointer to the new RightNode
*/
void BTTableClass::Split(const ItemType & CurrentItem, long CurrentRight,
   long CurrentRoot, int Location, ItemType & NewItem, long & NewRight)
{
    int j, Median;
    NodeType RightNode;

    if (Location < MinKeys)
        Median = MinKeys;
    else
        Median = MinKeys + 1;

    DataFile.seekg(CurrentRoot * NodeSize + 1024, ios::beg);
    DataFile.read(reinterpret_cast <char *> (&CurrentNode), NodeSize);

    for (j = Median; j < MaxKeys; j++)
    {  // move half of the items to the RightNode
        RightNode.Key[j - Median] = CurrentNode.Key[j];
        RightNode.Branch[j - Median + 1] = CurrentNode.Branch[j + 1];
    }

    RightNode.Count = MaxKeys - Median;
    CurrentNode.Count = Median;   // is then incremented by AddItem

    // put CurrentItem in place
    if (Location < MinKeys)
        AddItem(CurrentItem, CurrentRight, CurrentNode, Location + 1);
    else
        AddItem(CurrentItem, CurrentRight, RightNode,
        Location - Median + 1);

    NewItem = CurrentNode.Key[CurrentNode.Count - 1];
    RightNode.Branch[0] = CurrentNode.Branch[CurrentNode.Count];
    CurrentNode.Count--;

    DataFile.seekp(CurrentRoot * NodeSize + 1024, ios::beg);
    DataFile.write(reinterpret_cast <char *> (&CurrentNode), NodeSize);

    NumNodes++;
    NewRight = NumNodes;
    DataFile.seekp(NewRight * NodeSize + 1024, ios::beg);
    DataFile.write(reinterpret_cast <char *> (&RightNode), NodeSize);
}

/**
* Function used for standard pushdown in b+ tree
* CurrentItem - The item to be inserted
* CurrentRoot - Pseudopointer to root of current subtree
* Return - MoveUp - True if NewItem must be placed in the parent node due to
*                   splitting, false otherwise
*          NewItem - Item to be placed into parent node if a split was done
*          NewRight - Pseudopointer to child to the right of NewItem
*/
void BTTableClass::PushDown(const ItemType & CurrentItem, long CurrentRoot,
   bool & MoveUp, ItemType & NewItem, long & NewRight)
{
    int Location;

    if (CurrentRoot == NilPtr)   // stopping case
    {   // cannot insert into empty tree
        MoveUp = true;
        NewItem = CurrentItem;
        NewRight = NilPtr;
    }
    else   // recursive case
    {
        DataFile.seekg(CurrentRoot * NodeSize + 1024, ios::beg);
        DataFile.read(reinterpret_cast <char *> (&CurrentNode), NodeSize);

        if (SearchNode(CurrentItem.KeyField, Location))
        {
			cout << CurrentItem.KeyField << '\n';
			Error("Error: attempt to put a duplicate into B-tree");
		}

        PushDown(CurrentItem, CurrentNode.Branch[Location + 1], MoveUp,
         NewItem, NewRight);

        if (MoveUp)
        {
            DataFile.seekg(CurrentRoot * NodeSize + 1024, ios::beg);
            DataFile.read(reinterpret_cast <char *> (&CurrentNode), NodeSize);

            if (CurrentNode.Count < MaxKeys)
                {
                    MoveUp = false;
                    AddItem(NewItem, NewRight, CurrentNode, Location + 1);
                    DataFile.seekp(CurrentRoot * NodeSize + 1024, ios::beg);
                    DataFile.write(reinterpret_cast <char *> (&CurrentNode),
                    NodeSize);
                }
            else
            {
                MoveUp = true;
                Split(NewItem, NewRight, CurrentRoot, Location,
                NewItem, NewRight);
            }
        }
    }
}

/**
* Function used to add new Item to the table
* Item - To add to the table.
* Return - true to indicate success
*/
bool BTTableClass::Insert(const ItemType & Item)
{
    bool MoveUp;
    long NewRight;
    ItemType NewItem;

    PushDown(Item, Root, MoveUp, NewItem, NewRight);

    if (MoveUp)   // create a new root node
    {
        CurrentNode.Count = 1;
        CurrentNode.Key[0] = NewItem;
        CurrentNode.Branch[0] = Root;
        CurrentNode.Branch[1] = NewRight;
        NumNodes++;
        Root = NumNodes;
        DataFile.seekp(NumNodes * NodeSize +1024, ios::beg);
        DataFile.write(reinterpret_cast <char *> (&CurrentNode), NodeSize);
    }

    NumItems++;
    return true;
}

/**
* Function used to check SearchKey in the table
* Return - true if SearchKey was found
*          Item - The item where SearchKey was found
*/
bool BTTableClass::Retrieve(KeyFieldType SearchKey, ItemType & Item)
{
    long CurrentRoot = Root;
    bool Found = false;
    int Location;

    while ((CurrentRoot != NilPtr) && (! Found))
    {
        DataFile.seekg(CurrentRoot * NodeSize + 1024, ios::beg);
        DataFile.read(reinterpret_cast <char *> (&CurrentNode), NodeSize);

        if (SearchNode(SearchKey, Location))
        {
			/*if (!CurrentNode.Deleted[Location])
			{
				Found = true;
				Item = CurrentNode.Key[Location];
			}*/
				Found = true;
				Item = CurrentNode.Key[Location];
			cout << CurrentNode.Deleted[Location] << "<-retrieve location\n";
			break;
        }
        else
            CurrentRoot = CurrentNode.Branch[Location + 1];
    }
    
    return Found;
}

/**
* Function used to check return list of records followed by first match
* Return - true if SearchKey was found
*          Item - The item where SearchKey was found
*/
bool BTTableClass::RetrieveList(KeyFieldType SearchKey, ItemType & Item)
{
    long CurrentRoot;
    int Location;
    bool Found;
    int initialLocation;
    Found = false;
    CurrentRoot = Root;

    while ((CurrentRoot != NilPtr) && (! Found))
    {
        DataFile.seekg(CurrentRoot * NodeSize + 1024, ios::beg);
        DataFile.read(reinterpret_cast <char *> (&CurrentNode), NodeSize);

        if (SearchNode(SearchKey, Location))
        {
            initialLocation = Location;
            Found = true;
            for(int i=0;i<ListCount;i++)
            {
                Item = CurrentNode.Key[Location];
                if (Item.DataField == -1)
                {
                    // TODO
                }
                fstream OldInputFile;
                string line;
                OldInputFile.open(InputFileName, ios::in | ios::binary);
                while (!OldInputFile.eof())
                {
                    if (offset == Item.DataField)
                    {
                        getline(OldInputFile, line);
                        cout << "At " << offset << ", record: " << line << endl;
                        break;
                    }
                    getline(OldInputFile, line);
                    int currentLength = line.length() -1;
                    offset += currentLength;
                }
                OldInputFile.close();
                offset = 0;
                Location++;
            }
        }
        else
            CurrentRoot = CurrentNode.Branch[Location + 1];
    }
    return Found;
}


/**
 * TODO
 * Function to delete node 
 */
void BTTableClass::DeleteItem(KeyFieldType SearchKey)
{    
    long CurrentRoot = Root;
    bool Found = false;
    int Location;

    while ((CurrentRoot != NilPtr) && (! Found))
    {
        DataFile.seekg(CurrentRoot * NodeSize + 1024, ios::beg);
        DataFile.read(reinterpret_cast <char *> (&CurrentNode), NodeSize);

        if (SearchNode(SearchKey, Location))
        {
			cout << "loc: " << Location << '\n';	//debug
			CurrentNode.Deleted[Location] = true;
			cout << "del: " << CurrentNode.Deleted[Location] << '\n';	//debug
            DataFile.seekp(CurrentRoot * NodeSize, ios::beg);
            DataFile.write(reinterpret_cast <char *> (&CurrentNode), NodeSize);
			return;
        }
        else
            CurrentRoot = CurrentNode.Branch[Location + 1];
    }
}


/**
* Function used to readline from the InputFile
* InputFile - file stream already opened for input on input text file
* Return - Word - char array, the word read in
*           Definition - char array, the definition read in
*/
long ReadLine(fstream & InputFile, KeyFieldType Word,
   DataFieldType Definition)
{
    char Line[LineMax];
    int k, ch;
    InputFile.getline(Line, LineMax);

    for (k = 0; k < KeyFieldMax; k++)
    {
		Word[k] = Line[k];
	}
        
    Word[KeyFieldMax] = NULLCHAR;

    int currentLength = strlen(Line);
    Definition = offset;
    offset += currentLength;
    return Definition;
}

/**
* Function to read the data from InputFile and load it into the Table
* InputFile - file stream already opened for input
* Return - Table containing the data
*/
void Load(fstream & InputFile, BTTableClass & Table)
{
    ItemType Item;
    int Count;
    Count = 0;
    Item.DataField = ReadLine(InputFile, Item.KeyField, Item.DataField);
    
    // DEBUGGING
    cout << " LoadKey: " << Item.KeyField << endl;
    cout << " LoadOffset: " << Item.DataField << endl;
    while (! InputFile.fail())
    {
        Table.Insert(Item);
        Item.DataField = ReadLine(InputFile, Item.KeyField, Item.DataField);
    }
}

int main(int argc, char* argv[])
{
	clock_t start;
    double duration;
        		
    // Creating New File
    if (strcmp(argv[1],"-create") == 0)
    {
        KeyFieldMax = atoi(argv[2]);
        InputFileName = argv[3];
        IndexFileName = argv[4];
        
        fstream Source;
        BTTableClass BTTable('w', IndexFileName);

        Source.open(InputFileName, ios::in);
        if (Source.fail())
        {
            cerr << "ERROR: Unable to open file "<< InputFileName << endl;
            exit(1);
        }

        Load(Source, BTTable);
        Source.close();
        return 0;
    }

    // Searching record by key
    if (strcmp(argv[1],"-find") == 0)
    {
        ItemType Item;
        char TempSearchKey[30];
        char SearchKey [30];
        IndexFileName = argv[2];
        strcpy(TempSearchKey,argv[3]);
        BTTableClass BTTable('r', IndexFileName);
        if (BTTable.Empty())
            Error("Table is empty");
        int i;
        for (i=0;i<KeyFieldMax;i++)
        {
            SearchKey[i]=TempSearchKey[i];
        }
        SearchKey[i]= NULLCHAR;
        
        // start timing the retrieval operation
		start = clock();

        if (BTTable.Retrieve(SearchKey, Item))
        {	
            fstream OldInputFile;
            string line;
            OldInputFile.open(InputFileName, ios::in | ios::binary);
            while (!OldInputFile.eof())
            {
                if (offset == Item.DataField)
                {
                    getline(OldInputFile, line);
                    cout << "At " << offset << ", record: " << line << endl;
                    break;
                }
                getline(OldInputFile, line);
                int currentLength = line.length();
                offset += currentLength;
            }
            
            // end timing of retrieval operation
			duration = ( clock() - start ) / (double) CLOCKS_PER_SEC;
			cout << "Query ran in " << duration << "ms\n";
        }
        else
        {
			cout << "Not found" << endl;
		}
            
		return 0;
    }

    // Inserting new Record into Index file
    if (strcmp(argv[1],"-insert") == 0)
    {
        ItemType Item;
        IndexFileName = argv[2];
        char Record [50];
        char SearchKey [15];
        bool notfound = false;
        strcpy(Record,argv[3]);
        // ** Really Good Stuff **
        // Parenthesis used to call destructor automatically
        {
            BTTableClass BTTable('r', IndexFileName);
            if (BTTable.Empty())
                Error("Table is empty");
            int i;
            for (i=0;i<KeyFieldMax;i++)
            {
                SearchKey[i]=Record[i];
            }
            SearchKey[i]= NULLCHAR;
            if (BTTable.Retrieve(SearchKey, Item))
            {
                cout << "Record Found; Not Inserted";
            }
            else
                notfound = true;
        }
        if (notfound)
        {
            // Recreate new file
            // TODO: Dynamic update
            fstream OldInputFile;
            string line;
            OldInputFile.open(InputFileName, ios::out | ios::app);
            OldInputFile << Record << endl;
            OldInputFile.close();
            fstream Source;
            BTTableClass BTTable('w', IndexFileName);

            Source.open(InputFileName, ios::in);
            if (Source.fail())
            {
                cerr << "ERROR: Unable to open file "<<InputFileName << endl;
                exit(1);
            }
            Load(Source, BTTable);
            Source.close();
        }
        return 0;
    }

    if(strcmp(argv[1],"-delete") == 0)
    {
		ItemType Item;
        IndexFileName = argv[2];
        char Record [50];
        char SearchKey [15];
        bool notfound = false;
        strcpy(Record,argv[3]);
        
        {
            BTTableClass BTTable('r', IndexFileName);
            if (BTTable.Empty())
                Error("Table is empty");
            int i;
            for (i=0;i<KeyFieldMax;i++)
            {
                SearchKey[i]=Record[i];
            }
            SearchKey[i]= NULLCHAR;
            
            BTTable.DeleteItem(SearchKey);
        }
	}
    
    if (strcmp(argv[1],"-list") == 0)
    {
        // Retrieves list of records in that particular branch
        ItemType Item;
        IndexFileName = argv[2];
        char TempSearchKey [30];
        char SearchKey [30];
        strcpy(TempSearchKey,argv[3]);
        ListCount = atoi(argv[4]);

        BTTableClass BTTable('r', IndexFileName);
        if (BTTable.Empty())
            Error("Table is empty");
        int i;
        for (i=0;i<KeyFieldMax;i++)
        {
            SearchKey[i]=TempSearchKey[i];
        }
        SearchKey[i]= NULLCHAR;
        BTTable.RetrieveList(SearchKey, Item);
        /*if (BTTable.RetrieveNext(SearchKey, Item))
        {
            fstream OldInputFile;
            string line;
            OldInputFile.open(InputFileName, ios::in | ios::binary);
            while (!OldInputFile.eof())
            {
                if (offset == Item.DataField)
                {
                    getline(OldInputFile, line);
                    cout << "At " << offset << ", record: " << line << endl;
                    break;
                }
                getline(OldInputFile, line);
                int currentLength = line.length() -1;
                offset += currentLength;
            }
        }*/
    }
}
