#include <cstdlib>
#include <fstream>
#include <string>
#include <iostream>
#include "LexicalAnalysis.h"
#include "Stopwatch.h"
#include <cstdio>

int main(int argc, char *argv[]) {

	//std::ios_base::sync_with_stdio(false);

	LexicalAnalyzer lex;

	StopwatchChrono sw("dfa build");
	lex.setStopWords("stopwords.txt");
	sw.stop();

	sw.start("text scanning");
	for (int i = 0; i != 1; ++i) {

		std::ifstream in("warpeace.txt");
		if (!in) return EXIT_FAILURE;

		
		char term[100];
		size_t cnt = 0;
		while (lex.getWord(in, term)) {
			//std::cout << term << std::endl;
			//term.clear();
			++cnt;
		}
		
		std::cout << cnt << " terms found." << std::endl;
	}
	sw.stop();
	//std::cin.get();
	return EXIT_SUCCESS;
}