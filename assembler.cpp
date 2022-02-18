#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include "assembler.hpp"


using namespace std;


Assembler::Assembler(int op, char* filename){
    option = op;
    read_file(filename);
    cout << text;
    SymbolsTable["ZERO"] = [0, 20];
    DirectivesTable["SPACE"] = [0,1];
    InstructionsTable ["ADD"] = [1, 1, 2];
}


void Assembler::read_file(char* filename){
    ifstream infile(filename);
    stringstream buffer;
    buffer << infile.rdbuf();
    text = buffer.str();
}


void Assembler::run(){
    return;
}