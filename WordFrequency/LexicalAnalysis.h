#pragma once
#include <memory>
#include <string>

class LexicalAnalyzer {
public:
	LexicalAnalyzer();
	~LexicalAnalyzer();
	void setStopWords(const char *stopWordsFileName);
	bool getWord(std::ifstream &file, char *term);

private:
	struct Impl;
	std::unique_ptr<Impl> mImpl;
};