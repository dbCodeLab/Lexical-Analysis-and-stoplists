#include "LexicalAnalysis.h"
#include <vector>
#include <memory>
#include <algorithm>
#include <iterator>
#include <fstream>
#include <iostream>
#include <string>
#include <cctype>
#include <cstring>

using std::vector;
using std::string;
using std::istream;

struct LexicalAnalyzer::Impl {

	using IndexType = size_t;

	struct Word {
		IndexType start;   // index first char
		IndexType	end;   // index of last char + 1

		Word() = default;
		Word(IndexType start, IndexType	end) : start(start), end(end) { }
	};

	struct WordCollection {
		vector<char> wordsCharBuffer;
		vector<Word> words;
	};

	struct DFA {
	private:
		
		using SignatureType = unsigned int;
		using LabelType = vector<Word>;

		/* All states must be searched by their labels to make sure that the minimum number of states are used.  */
		/* This operation is sped up by hashing the labels to a signature value, then storing the signatures and */
		/* labels in a hash table of binary search trees. These support structures are destroyed once the DFA    */
		/* is fully constructed.                                                                                 */
		struct SignatureSearchTree {

			struct TreeNode {
				SignatureType signature; /* hashed label to speed searching     */
				IndexType state;		 /* whose label is represented by node  */
				TreeNode *left;			 /* left binary search subtree          */
				TreeNode *right;		 /* right binary search subtree         */

				~TreeNode() {
					delete left;
					delete right;
				}
			};
			
			~SignatureSearchTree() {
				delete root;
				root = nullptr;
			}

			TreeNode *root = nullptr;
		};
		
		struct State {
			IndexType label;	  /* used during build				   */
			IndexType arcOffset;  /* for finding arcs in the arc table */
			uint8_t numArcs;	  /* in the arc table			       */
			bool isFinal;		  /* TRUE iff this is a final state    */
								
			State(IndexType labelIdx) : label(labelIdx) {}
		};


		#define INITIAL_TABLE_SIZE    1024
		vector<State> stateTable;
		//vector<Arc> arcTable;
		vector<char> arcLabels;
		vector<IndexType> arcTargets;
		std::unique_ptr<vector<LabelType>> pLabelsTable;
				
		#define HASH_TABLE_SIZE 53
		SignatureSearchTree htable[HASH_TABLE_SIZE];

		WordCollection *wordCollectionPtr = nullptr;
		
		void sortAndRemoveDuplicateWords(vector<Word> &words);
		int cmpWords(const Word &a, const Word &b);
		int cmpLabel(const vector<Word> &label1, const vector<Word> &label2);
		IndexType getState(vector<Word> &label, SignatureType signature);
		void addArc(State &state, char arcLabel, vector<Word> &stateLabel, SignatureType stateSignature);
		void destroySearchStructures();
		void destroyLabels();
		SignatureType startHashValue();
		SignatureType updateHashValue(SignatureType curHashValue, const vector<char> &wc, IndexType start, IndexType end);

		bool dead;
		IndexType curState;

	public:

		void	build(WordCollection &words);
		void    init();
		void    next(char c);
		bool    wordRecognized();
	};


	char buffer[256+1+256+1+256];
	char *separator, *token, *lower;

	void loadStopWords(const char *fileName, Impl::WordCollection &wc);
	bool isDelimiter(int c);
	void buildCharTables();
	//int getNextChar(FILE *file);

	DFA machine; /* built from the stop list */
};



void LexicalAnalyzer::Impl::DFA::sortAndRemoveDuplicateWords(vector<Word>& words) {

	const char *charBuffer = wordCollectionPtr->wordsCharBuffer.data();

	auto predSort = [cbuf = charBuffer](const Word &a, const Word &b) -> bool {

		IndexType first1 = a.start;
		IndexType last1 = a.end;
		IndexType first2 = b.start;
		IndexType last2 = b.end;

		
		while (first1 != last1) {
			if (first2 == last2 || cbuf[first2] < cbuf[first1]) return false;
			else if (cbuf[first1] < cbuf[first2]) return true;
			++first1;
			++first2;
		}
		
		return (first2 != last2);
		
	};

	std::sort(std::begin(words), std::end(words), predSort);

	auto predEqual = [cbuf = charBuffer](const Word &a, const Word &b) -> bool {

		IndexType first1 = a.start;
		IndexType last1 = a.end;
		IndexType first2 = b.start;
		IndexType last2 = b.end;

		if (last1 - first1 == last2 - first2) {
			while (first1 != last1) {
				if (cbuf[first1] != cbuf[first2]) {
					return false;
				}
				++first1;
				++first2;
			}

			return true;
		}

		return false;
	};

	auto it = std::unique(std::begin(words), std::end(words), predEqual);
	words.resize(std::distance(std::begin(words), it));
}

int LexicalAnalyzer::Impl::DFA::cmpWords(const Word & a, const Word & b) {

	const vector<char> &charBuffer = wordCollectionPtr->wordsCharBuffer;

	IndexType first1 = a.start;
	IndexType last1 = a.end;
	IndexType first2 = b.start;
	IndexType last2 = b.end;
	size_t len1 = last1 - first1;
	size_t len2 = last2 - first2;

	if (len1 == len2) {
		for (IndexType i = 0; i != len1; ++i) {
			char a = charBuffer[first1++];
			char b = charBuffer[first2++];
			if (a < b) return -1;
			else if (a > b) return 1;
		}
		return 0;
	}
	else if (len1 < len2)
		return -1;
	else return 1;
}

int LexicalAnalyzer::Impl::DFA::cmpLabel(const vector<Word>& label1, const vector<Word>& label2) {

	size_t len1 = label1.size();
	size_t len2 = label2.size();

	if (len1 == len2) {
		int cmp = 0;
		for (IndexType i = 0; i != len1 && !cmp; ++i) {
			cmp = cmpWords(label1[i], label2[i]);
		}
		return cmp;
	}
	else if (len1 < len2)
		return -1;
	else return 1;
}

LexicalAnalyzer::Impl::IndexType LexicalAnalyzer::Impl::DFA::getState(vector<Word> &label, SignatureType signature) {

	/* Part 1: Search the hash table/tree for the requested state */

	using NodePtr = SignatureSearchTree::TreeNode *;

	NodePtr *ptrNode = &htable[signature%HASH_TABLE_SIZE].root;
	NodePtr node;

	/* 
	//linear search
	while (node = *ptrNode) {
		if (cmpLabel(label, *node->label) == 0) {
			goto found;
		}
		ptrNode = &node->left;
	}
	*/

	while (node = *ptrNode) {
		if (signature == node->signature) {
			switch (cmpLabel(label, (*pLabelsTable)[node->state])) {
			case -1: goto left;
			case  0: goto found;
			case  1: goto right;
			}
		}
		else if (signature < node->signature) {
		left:				ptrNode = &node->left;
		}
		else {
		right:				ptrNode = &node->right;
		}
	}

	/* create new label and state */

	pLabelsTable->emplace_back(std::move(label));
	stateTable.emplace_back(pLabelsTable->size()-1);

	/* create a new node and fill in its fields */

	NodePtr newNode = new SignatureSearchTree::TreeNode;
	newNode->state = stateTable.size()-1;
	//newNode->label = stateTable.size()-1; // same index
	newNode->signature = signature;
	newNode->left = newNode->right = nullptr;
	*ptrNode = newNode;

found:
	label.clear();

	return (*ptrNode)->state;
}

void LexicalAnalyzer::Impl::DFA::addArc(State &state, char arcLabel, vector<Word>& stateLabel, SignatureType stateSignature) {

	/* Part 1: Search for the target state among existing states */
	sortAndRemoveDuplicateWords(stateLabel);
	IndexType stateTarget = getState(stateLabel, stateSignature);

	/* Part 3: Add the new arc */
	arcLabels.emplace_back(arcLabel);
	arcTargets.emplace_back(stateTarget);
	++state.numArcs;
}

void LexicalAnalyzer::Impl::DFA::destroySearchStructures()
{
	for (auto &tree: htable) {
		delete tree.root;
		tree.root = nullptr;
	}
}

void LexicalAnalyzer::Impl::DFA::destroyLabels()
{
	pLabelsTable.reset(nullptr);
}

inline LexicalAnalyzer::Impl::DFA::SignatureType LexicalAnalyzer::Impl::DFA::startHashValue() {
	const SignatureType HASH_START = 5775863;
	return HASH_START;
}

inline LexicalAnalyzer::Impl::DFA::SignatureType LexicalAnalyzer::Impl::DFA::updateHashValue(SignatureType curHashValue, 
	const vector<char> &wc, IndexType start, IndexType end) {
	const SignatureType HASH_INCREMENT = 38873647;
	if (start != end) {
		curHashValue += (wc[start] + 1) * HASH_INCREMENT;
		do {
			curHashValue += wc[start++];
		} while (start != end);
	}
	else curHashValue += HASH_INCREMENT;

	return curHashValue;
}

void LexicalAnalyzer::Impl::DFA::build(WordCollection &wordCollection)
{
	wordCollectionPtr = &wordCollection;

	stateTable.clear();
	arcLabels.clear();
	arcTargets.clear();
	
	stateTable.reserve(INITIAL_TABLE_SIZE);
	arcLabels.reserve(INITIAL_TABLE_SIZE);
	arcTargets.reserve(INITIAL_TABLE_SIZE);
	pLabelsTable.reset(new vector<LabelType>);
	pLabelsTable->reserve(INITIAL_TABLE_SIZE);
	pLabelsTable->emplace_back(std::move(wordCollection.words));
	stateTable.emplace_back(0);
	sortAndRemoveDuplicateWords((*pLabelsTable)[0]);

	vector<Word> targetLabel;
	
	for (IndexType state = 0; state != stateTable.size(); ++state) {
		/* The current state has nothing but a label,       */
		/* so the first order of business is to set up some */
		/* of its other major fields						*/

		stateTable[state].arcOffset = arcLabels.size();
		stateTable[state].numArcs = 0;
		stateTable[state].isFinal = false;


		/* Add arcs to the arc table for the current state  */
		/* based on the state's derived set.  Also set the  */
		/* state's final flag if the empty string is found  */
		/* in the suffix list                               */

		targetLabel.clear();
		targetLabel.reserve((*pLabelsTable)[state].size());

		SignatureType targetSignature = startHashValue();
		char arcLabel = -1;
			
		for (const auto &word : (*pLabelsTable)[state]) {
			
			/* the empty string means mark this state as final */
			if (word.start == word.end) {
				stateTable[state].isFinal = true;
				continue;
			}

			char ch = wordCollectionPtr->wordsCharBuffer[word.start];

			/* make sure we have a legitimate arc_label */
			if (arcLabel == -1) arcLabel = ch;

			/* if the first character is new, then we must */
			/* add an arc for the previous first character */
			if (ch != arcLabel) {
				addArc(stateTable[state], arcLabel, targetLabel, targetSignature);
				targetLabel.reserve((*pLabelsTable)[state].size());
				targetSignature = startHashValue();
				arcLabel = ch;
			}

			/* add the current suffix to the target state label */

			targetLabel.emplace_back(word.start + 1, word.end);

			targetSignature = updateHashValue(targetSignature, wordCollection.wordsCharBuffer,word.start + 1, word.end);
		
		}

		/* On loop exit we have not added an arc for the  */
		/* last bunch of suffixes, so we must do so, as   */
		/* long as the last set of suffixes is not empty  */
		/* (which happens when the current state label    */
		/* is the singleton set of the empty string).     */

		if (targetLabel.size() != 0) {
			addArc(stateTable[state], arcLabel, targetLabel, targetSignature);
		}
		
	}
	
	/* Part 4: Deallocate the hash table/binary search trees and the state labels */
	destroySearchStructures();
	destroyLabels();
	
	stateTable.shrink_to_fit();
	arcLabels.shrink_to_fit();
	arcTargets.shrink_to_fit();

	wordCollectionPtr = nullptr;

}

void LexicalAnalyzer::Impl::DFA::init()
{
	dead = false;
	curState = 0;
}

inline void LexicalAnalyzer::Impl::DFA::next(char c)
{
	if (dead) return;
	
	IndexType arcStart = stateTable[curState].arcOffset;
	IndexType arcEnd = arcStart + stateTable[curState].numArcs;

	
	for (IndexType i = arcStart; i != arcEnd; ++i) {
		if (arcLabels[i] == c) {
			curState = arcTargets[i];
			return;
		}
	}

	dead = true;
	
}

inline bool LexicalAnalyzer::Impl::DFA::wordRecognized()
{
	return !dead && stateTable[curState].isFinal;
}

LexicalAnalyzer::LexicalAnalyzer() : mImpl(new Impl)
{

}

LexicalAnalyzer::~LexicalAnalyzer()
{
	
}

void LexicalAnalyzer::setStopWords(const char *stopWordsFileName)
{
	Impl::WordCollection stopWords;
	
	mImpl->loadStopWords(stopWordsFileName, stopWords);

	mImpl->machine.build(stopWords);

	mImpl->buildCharTables();

}



void LexicalAnalyzer::Impl::loadStopWords(const char *fileName, Impl::WordCollection &wc)
{
	std::ifstream in(fileName);
	if (!in)
	{
		throw(errno);
	}

	std::string line;
	wc.wordsCharBuffer.reserve(1024);
	wc.words.reserve(1024);

	while (std::getline(in, line)) {
		if (line.empty()) continue;
		IndexType start = wc.wordsCharBuffer.size();
		std::for_each(std::begin(line), std::end(line), [](char &c) { c = tolower(c);});
		std::copy(line.begin(), line.end(), std::back_inserter(wc.wordsCharBuffer));
		IndexType end = wc.wordsCharBuffer.size();
		wc.words.emplace_back(start, end);
	}

	wc.wordsCharBuffer.shrink_to_fit();
	wc.words.shrink_to_fit();
}

bool LexicalAnalyzer::Impl::isDelimiter(int c)
{
	return !(isalpha(c));
}

void LexicalAnalyzer::Impl::buildCharTables()
{
	*buffer = 0; // eof not a separator
	separator = buffer + 1;
	for (int c = 0; c != 255; ++c) {
		separator[c] = !isalpha(c); //!(isalnum(c)) || isdigit(c);
	}
	token = buffer + 256 + 1;
	*(token-1) = 0; // eof not a separator
	for (int c = 0; c != 255; ++c) {
		token[c] = isalpha(c) || c == '\''; //!(isalnum(c)) || isdigit(c);
	}
	lower = token + 256;
	for (int c = 0; c != 255; ++c) {
		lower[c] = tolower(c);
	}
}

static const int BUF_SIZE = 4096;
static char readbuf[BUF_SIZE];
static size_t ic = BUF_SIZE;
static size_t size = 0;

static inline bool endFile(std::ifstream &file) {
	return file.eof() && ic >= size;
}

static inline int getNextChar(std::ifstream &file)
{
	
	if (ic == BUF_SIZE) {
		ic = 0;
		//size = fread(readbuf, sizeof(char), BUF_SIZE, file);
		file.read(readbuf, BUF_SIZE);
		size = file.gcount();
		if (size != BUF_SIZE) {
			if (file.eof()) {
				readbuf[size] = EOF;
			} else if (file.fail() || file.bad()) {
				// ...
			}
		}
	}

	return readbuf[ic++];
}



bool LexicalAnalyzer::getWord(std::ifstream &file, char *term)
{

	if (endFile(file)) {
		return false;
	}

	char *separator = mImpl->separator;
	char *token = mImpl->token;
	char *lower = mImpl->lower;

	int ch;
	char *dst = term;

	do {
		/* recognize: separator* token */
		ch = getNextChar(file);
		while (separator[ch]) {
			ch = getNextChar(file);//fgetc _fgetc_nolock getNextChar
		}

		mImpl->machine.init();

		while (token[ch]) {
			char chlower = lower[ch];
			*dst++ = chlower;
			mImpl->machine.next(chlower);
			ch = getNextChar(file);
		}

		if (mImpl->machine.wordRecognized()) {
			dst = term;
		}
		
		*dst = 0;
		
	} while (ch != EOF && !*term);

	return *term;
}

