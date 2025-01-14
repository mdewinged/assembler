#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include "etc.hpp"
#include "assembler.hpp"


using namespace std;


// Initilizes all variables, modules, read input file, load files
Assembler::Assembler(int op, char* inputfile, char* outputfile){
    this->option = op;
    this->outputfile = outputfile;
    
    this->Err = new ErrorDealer(this->option);
    this->Lex = new LexicalAnalyzer(this->option, this->Err);
    this->Syn = new SyntacticAnalyzer(this->option, this->Err, &this->DirectivesTable, &this->InstructionsTable);
    this->Sem = new SemanticAnalyzer(this->option, this->Err, &this->SymbolsTable);
    this->ObjGen = new ObjectGenerator(this->option, this->Err, &this->DirectivesTable, &this->InstructionsTable, &this->SymbolsTable);

    this->text = this->read_file(inputfile);
    this->load_directives(DIRECTIVEFILE);
    this->load_instructions(INSTRUCTIONFILE);

    // Split text into lines
    istringstream iss(this->text); 
    for(string line; getline(iss, line);)
        this->lines.push_back(line);
}


// This function reads a file and returns a string
string Assembler::read_file(char* filename){
    ifstream infile(filename);
    stringstream buffer;
    buffer << infile.rdbuf();
    return buffer.str();
}


// Loads the directives especifications of this assembly language into the DirectivesTable
void Assembler::load_directives(const string filename){
    string aux = this->read_file((char*)filename.c_str());
    
    istringstream iss(aux); 
    for (string line; getline(iss, line);){
        // Split line into tokens separated by whitespace
        int lastindex = 0;
        vector<string> aux;
        for(int i=0; i<line.length(); i++){
            if(line[i] == ' '){
                int difference = i - lastindex;
                aux.push_back(line.substr(lastindex, difference));
                lastindex = i + 1;
            }
        }
        // Get last element
        int difference = line.length() - lastindex;
        aux.push_back(line.substr(lastindex, difference));
                
        string key = aux[0];
        int operands = stoi(aux[1]);
        int size = stoi(aux[2]);
        Directive dir = {operands, size};
        this->DirectivesTable[key] = dir; 
    }
}


// Loads the instructions especifications of this assembly language into the InstructionsTable
void Assembler::load_instructions(const string filename){
    string aux = this->read_file((char*)filename.c_str());
    
    istringstream iss(aux); 
    for (string line; getline(iss, line);){
        // Split line into tokens separated by whitespace
        int lastindex = 0;
        vector<string> aux;
        for(int i=0; i<line.length(); i++){
            if(line[i] == ' '){
                int difference = i - lastindex;
                aux.push_back(line.substr(lastindex, difference));
                lastindex = i + 1;
            }
        }
        // Get last element
        int difference = line.length() - lastindex;
        aux.push_back(line.substr(lastindex, difference));
        

        string key = aux[0];
        int operands = stoi(aux[1]);
        int opcode = stoi(aux[2]);
        int size = stoi(aux[3]);
        Instruction inst = {operands, opcode, size};
        this->InstructionsTable[key] = inst; 
    }
}


// This run the algorithm to assemble the file
void Assembler::run(){
    string auxline, line, label= "";
    bool status=true, ignore_next_line = false;
    
    // For each line inside the file
    for(; this->line_counter<=this->lines.size(); this->line_counter++){
        // As the line counter starts in 1, so subtract its value when acessing the current line
        line = this->lines[this->line_counter-1];
        // Adds the line into the preprocessed final file, if this line cannot be part of the preprocessed file, the following lines will remove it
        this->ObjGen->add_line_preprocessed_file(this->Lex->to_upper(line));
        
        // This variable represents when a IF directive is set to false
        if(ignore_next_line){
            ignore_next_line = false;
            this->ObjGen->remove_line_pre_option();
            continue;
        }

        // Ignoring comments and etc, if line is empty, goes to the next
        if(this->Lex->is_empty_line(line)) continue;

        // Clean comments, double whitespaces, tabs, breaklines
        auxline = this->Lex->to_upper(line);
        auxline = this->Lex->clean_line(auxline);
    
        // Split line into tokens
        vector<string> tokens = this->Lex->split(auxline);

        // If there is a definition of label
        if(this->Lex->is_label(tokens[0])){
            // In case of two labels been defined on different lines but without any commands
            if(!label.empty()){
                this->Err->register_err(this->line_counter, SEM_ERR_MULTIPLE_LABEL_DEF_ON_SAME_LINE);
                status = false;
            }
            // Remove de double dots at the end of label
            label = tokens[0].substr(0, tokens[0].length()-1);
            status &= this->Lex->is_valid_variable_name(label, this->line_counter);
            
            tokens.erase(tokens.begin());
            // If the line contains only the label, continue the process (it is equal to ignore breaks)
            if(tokens.size() == 0) continue;
        }
        
        // Make Syntactic analysis
        status &= this->Syn->analyze(tokens, this->line_counter);

        // Make Semantic analysis
        status &= this->Sem->analyze(tokens, label, this->line_counter);

        // After verifying if the first position is a label, it must be a command
        string command = tokens[0];
        
        // If the command is a directive
        if(this->Syn->is_directive(command)){
            // If the instruction is a macro, add it to the macro definition
            if(command.compare("MACRO") == 0){
                this->ObjGen->remove_line_mac_option();
                this->save_macro_lines = true;
                this->macrolabel = label;
                // Must set the macro name, because the semantic analyzer must know that if a word is not a directive or a instruction, it may be a macro definition
                this->Syn->set_macro_name(label);
                label.clear();
                continue;
            }
            // If finds the ENDMACRO, stop adding lines
            else if(command.compare("ENDMACRO") == 0){
                this->ObjGen->remove_line_mac_option();
                this->save_macro_lines = false;
            }
            // If finds the EQU add a definition to the EQU table
            else if(command.compare("EQU") == 0){
                this->ObjGen->add_equ_definition(label, tokens[1], false);
                // This directive must not be on preprocessed file
                this->ObjGen->remove_line_pre_option();
            }
            else if(command.compare("IF") == 0){
                // Get the symbol defined on IF
                EQU equ = this->ObjGen->get_equ(tokens[1]); 
                // Register that a EQU definition was used
                this->Sem->set_equ_used(tokens[1]);
                // If the EQU value is not 1 (true), ignore the next line
                if(stoi(equ.token) != 1) ignore_next_line = true;
                // This directive must not be on preprocessed file
                this->ObjGen->remove_line_pre_option();
            }
            else if(command.compare("SPACE") == 0)
                this->ObjGen->add_space_definition(label, 0);
            else if(command.compare("CONST") == 0)
                this->ObjGen->add_space_definition(label, stoi(tokens[1]));
            
            // SPACE and CONST will be added on position counter at the end of execution
            if(!(command.compare("SPACE") == 0) && !(command.compare("CONST") == 0))
                this->position_counter += this->ObjGen->get_directive_size(command);
        }

        // If there is a label definition, then add it to the symbols table
        if(!label.empty()){
            // Else, if the symbol is not defined, simply add it to the table
            if(!this->ObjGen->symbol_exists(label))
                this->ObjGen->add_symbol(label, true, -1, this->position_counter);
            // If the label is already exists but not defined, resolve all pending references
            else if(!this->ObjGen->is_symbol_defined(label)){
                // If it is not a space labels, they will be resolved at the end of execution
                if(!this->ObjGen->is_a_space_label(label)){
                    int lastoccurence = this->ObjGen->get_last_occurence_symbol(label);
                    this->ObjGen->update_symbol(label, true, -1, this->position_counter);
                    this->ObjGen->further_reference_dealer(label, lastoccurence);
                }
            }
        }

        // If found a MACRO and not a ENDMACRO, save lines and continue 
        if(this->save_macro_lines){
            this->macrodefinition.push_back(line); 
            this->ObjGen->remove_line_mac_option();
            // If the pre option was given, the EQU definitions must be changed inside the macro
            this->ObjGen->substitute_equ_variables_in_macro_definitions(tokens);
            label.clear();
            continue;
        }

        if(this->Syn->is_instruction(command)){
            this->ObjGen->add_to_objectfile(this->ObjGen->get_opcode(command));
            position_counter++;

            // Analyze each token
            for(int j=1; j<tokens.size(); j++){
                // If any further token is a label definition and already has been identifyed a label, só there is multiple label def
                if(this->Lex->is_label(tokens[j]) && !label.empty()){
                        this->Err->register_err(this->line_counter, SEM_ERR_MULTIPLE_LABEL_DEF_ON_SAME_LINE);
                        status &= true;
                }
                // If the token is defined as EQU, substitute it
                if(this->ObjGen->is_equ_definition(tokens[j])){
                    this->Sem->set_equ_used(tokens[j]);
                    string equtoken = this->ObjGen->resolve_equ_definitions(tokens[j]);
                    this->ObjGen->substitute_equ_pre_file(equtoken, tokens[j]);
                    tokens[j] = equtoken;
                }
                // If the command is COPY, then remove the ", " from the first parameter
                if(command.compare("COPY") == 0){
                    int index = tokens[1].find(',');
                    if(index != -1) tokens[1] = tokens[1].substr(0, index);
                }
                // If the symbol is defined, then add it to the object file
                if(this->ObjGen->symbol_exists(tokens[j])){
                    Symbol sym = this->ObjGen->get_symbol(tokens[j]);
                    // If the symbol has already been defined
                    if(this->ObjGen->is_symbol_defined(tokens[j]))
                        this->ObjGen->add_to_objectfile(sym.position);
                    // If the label has not been defined
                    else{
                        // Save the last occurence to resolve as further reference
                        this->ObjGen->add_to_objectfile(sym.last_occurence);
                        // Update the symbol last reference to the actual position counter
                        this->ObjGen->update_symbol(tokens[j], false, this->position_counter, -1);
                    }
                }
                // If the symbol is not defined yet, register a pendig reference to be resolved
                else{
                    // Sums +1 on position counter because of the opcode size
                    this->ObjGen->add_symbol(tokens[j], false, this->position_counter, -1);
                    this->ObjGen->add_to_objectfile(-1);
                }
                position_counter++;
            }
        }
        // If is not a directive or a instruction, it must be a MACRO definition
        else if(!this->Syn->is_directive(command)){
            // If is a macrolabel and option is different from -p, replace it with the lines
            if(command.compare(this->macrolabel) == 0){
                this->ObjGen->remove_line_mac_option();
                if (this->option != OPTION_PRE_NUM)
                    lines.insert(lines.begin()+this->line_counter, this->macrodefinition.begin(), this->macrodefinition.end());
                this->Sem->set_macro_used();
            }
        }

        // Limpar a definição de label 
        label.clear();
    }
    this->ObjGen->add_spaces_to_objectfile(this->position_counter);
    // Checks if all labels that were used in code, were defined
    status &= this->Sem->check_if_all_labels_defined();
    status &= this->Sem->check_if_all_EQU_used();
    status &= this->Sem->end_check_MACRO();

    // Show all errors
    this->Err->show();

    this->ObjGen->generate_file(this->outputfile);
}
