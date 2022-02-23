#ifndef SYNTATIC_ANALYZER_HPP_INCLUDED
#define SYNTATIC_ANALYZER_HPP_INCLUDED


#include <string>
#include <vector>
#include <unordered_map>
#include "etc.hpp"
#include "error_dealer.hpp"


using namespace std;


class SyntaticAnalyzer{
    private:
        int option;
        ErrorDealer* Err;
        unordered_map<string, Directive> DirectivesTable;
        unordered_map<string, Instruction> InstructionsTable;
    public:
    SyntaticAnalyzer(int option, ErrorDealer* Err, unordered_map<string, Directive> DirectivesTable, unordered_map<string, Instruction> InstructionsTable);
    void analyze(vector<string> tokens);
};


#endif