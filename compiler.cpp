#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cctype>
#include <algorithm>

using namespace std;

// ----- Lexer data -----
enum TokenType { Delimiter, Reserved, Identifier, Integer, Real, End };
struct Token { TokenType type; string lexeme; int index; };

// store delimiters/reserved words with index for table lookup
unordered_map<string,int> delimiterIndex;
vector<string> delimiterList;
unordered_map<string,int> reservedIndex;
vector<string> reservedList;
vector<string> integerTable;
vector<string> realTable;
vector<string> identifierTable;
vector<vector<Token>> tokens;

// collect declared variables and labels for output ordering
unordered_set<string> variableSet;
vector<string> variableList;
unordered_set<string> labelSet;
vector<string> labelList;

string toUpper(const string &str) {
    string lower = str;
    for (char &c : lower) c = toupper(c);
    return lower;
}

bool loadDelimiters(const string &file) {
    ifstream in(file);
    if (!in) {
        cerr << "Error: could not open " << file << endl;
        return false;
    }
    string s; int idx = 1;
    while (getline(in, s)) {
        if (!s.empty() && s.back()=='\r') s.pop_back();
        if (!s.empty()) {
            delimiterList.push_back(s);
            delimiterIndex[s] = idx++;
        }
    }
    return true;
}

bool loadReserved(const string &file) {
    ifstream in(file);
    if (!in) {
        cerr << "Error: could not open " << file << endl;
        return false;
    }
    string s;
    int idx = 1;
    while (getline(in, s)) {
        if (!s.empty() && s.back()=='\r') s.pop_back();
        if (!s.empty()) {
            string up = toUpper(s);
            reservedList.push_back(up);
            reservedIndex[up] = idx++;
        }
    }
    return true;
}

// compute sum of ASCII codes and map into table via modulo 100
int asciiIndex(const string &s) {
    int sum = 0;
    for (unsigned char c : s) sum += c;
    return sum % 100;
}

int addInteger(const string &s) {
    if (integerTable.size() < 100) integerTable.resize(100);
    int idx = asciiIndex(s);
    int size = integerTable.size();
    for (int i = 0; i < size; ++i) {
        int probe = (idx + i) % size;
        if (integerTable[probe].empty()) {
            integerTable[probe] = s;
            return probe;
        }
        if (integerTable[probe] == s) return probe;
    }
    return idx;
}

int addReal(const string &s) {
    if (realTable.size() < 100) realTable.resize(100);
    int idx = asciiIndex(s);
    int size = realTable.size();
    for (int i = 0; i < size; ++i) {
        int probe = (idx + i) % size;
        if (realTable[probe].empty()) {
            realTable[probe] = s;
            return probe;
        }
        if (realTable[probe] == s) return probe;
    }
    return idx;
}

// Store identifiers at ASCII index derived from sum of characters modulo 100
int addIdentifier(const string &s) {
    if (identifierTable.size() < 100) identifierTable.resize(100);
    int idx = asciiIndex(s);
    int size = identifierTable.size();
    for (int i = 0; i < size; ++i) {
        int probe = (idx + i) % size;
        if (identifierTable[probe].empty()) {
            identifierTable[probe] = s;
            return probe;
        }
        if (identifierTable[probe] == s) return probe;
    }
    return idx;
}

bool tokenize(const string &inputFile) {
    ifstream in(inputFile);
    if (!in) return false;
    tokens.clear();
    string line;
    while (getline(in, line)) {
        if (!line.empty() && line.back()=='\r') line.pop_back();
        vector<Token> lineTokens; int i = 0;
        while (i < line.size()) {
            char c = line[i];
            if (isspace((c))) { ++i; continue; }
            if (delimiterIndex.count(string(1,c))) {
                lineTokens.push_back({Delimiter,string(1,c),-1});
                ++i;
                continue;
            }
            if (isalpha((c))) {
                string word;
                while (i<line.size() && (isalnum((line[i]))||line[i]=='_'))
                    word += line[i++];
                string upper = toUpper(word);
                if (reservedIndex.count(upper))
                    lineTokens.push_back({Reserved,upper,-1});
                else {
                    int idx = addIdentifier(word);
                    lineTokens.push_back({Identifier,word,idx});
                }
                continue;
            }
            if (isdigit((c))) {
                string num; bool isReal=false;
                while (i<line.size() && isdigit((line[i]))) num += line[i++];
                if (i<line.size() && line[i]=='.') {
                    isReal=true;
                    num+=line[i++];
                    while (i<line.size() && isdigit((line[i]))) num += line[i++];
                }
                int idx;
                if (isReal) {
                    idx=addReal(num);
                    lineTokens.push_back({Real,num,idx});
                }
                else {
                    idx=addInteger(num);
                    lineTokens.push_back({Integer,num,idx});
                }
                continue;
            }
            ++i;
        }
        tokens.push_back(lineTokens);
    }
    return true;
}

// ----- Semantic info -----
struct SymbolInfo { string type; bool isArray=false; int arraySize=0; };
unordered_map<string,SymbolInfo> symbolTable;

 void addVariable(const string &name, const string &type) {
     symbolTable[name].type = type;
}
 void addArray(const string &name, const string &type, int size) {
     symbolTable[name] = {type, true, size};
}

// ----- IR builder -----
struct Ref {
     int table = 0;
     int index = 0;
};
struct Quadruple {
     Ref op, arg1, arg2, result;
     string comment;
};
vector<Quadruple> quads;
int tempCount = 0;
int labelCount = 0;

 string newTemp() {
      return "T" + to_string(++tempCount);
}

 string newLabel() {
    string lbl="L" + to_string(++labelCount);
    addIdentifier(lbl);
    return lbl;
}

 bool isIntegerStr(const string &s) {
     return !s.empty() && all_of(s.begin(), s.end(), ::isdigit);
}

 bool isRealStr(const string &s) {
     return s.find('.') != string::npos && all_of(s.begin(), s.end(), [](char c){return isdigit(c) || c=='.';});
}
Ref toRef(const string &lex) {
    if (lex.empty()) return {0,0};
    if (delimiterIndex.count(lex)) return {1, delimiterIndex[lex]};
    string up = toUpper(lex);
    if (reservedIndex.count(up)) return {2, reservedIndex[up]};
    if (lex[0]=='T' && isIntegerStr(lex.substr(1))) return {0, stoi(lex.substr(1))};
    if (isIntegerStr(lex)) return {3, addInteger(lex)};
    if (isRealStr(lex)) return {4, addReal(lex)};
    int idx = addIdentifier(lex);
    return {5, idx};
}

void addQuad(const string& op, const string& a1, const string& a2, const string& res, const string& comment) {
    quads.push_back({toRef(op), toRef(a1), toRef(a2), toRef(res), comment});
}

string refToStr(const Ref &r) {
    if (r.table==0 && r.index==0) return "";
    return "(" + to_string(r.table) + "," + to_string(r.index) + ")";
}

void writeQuadTable(const string& file) {
    ofstream out(file);
    int idx=1;
    Ref empty;
    for (const auto &v : variableList){
        Ref r = toRef(v);
        out << idx++ << "\t(" << refToStr(r) << "," << "     "
            << refToStr(empty) << "," << "     "
            << refToStr(empty) << "," << "     "
            << refToStr(empty) << ")\t" << v << "\n";
    }
    for (const auto &l : labelList){
        Ref r = toRef(l);
        out << idx++ << "\t(" << refToStr(r) << "," << "     "
            << refToStr(empty) << "," << "     "
            << refToStr(empty) << "," << "     "
            << refToStr(empty) << ")\t" << l << "\n";
    }
    for (auto &q:quads) {
        out << idx++ << "\t("<<refToStr(q.op);
        if (refToStr(q.op).size() == 0) out << "     ";
        out << "," <<refToStr(q.arg1);
        if (refToStr(q.arg1).size() == 0) out << "     ";
        out << "," <<refToStr(q.arg2);
        if (refToStr(q.arg2).size() == 0) out << "     ";
        out << "," <<refToStr(q.result);
        if (refToStr(q.result).size() == 0) out << "     ";
        out <<")\t"<<q.comment<<"\n";
    }
}

// ----- Parser helpers -----
 string tokenValue(const Token &t) {
     return t.lexeme;
}


string parseFactor(const vector<Token>& line,int &i);
string parseTerm(const vector<Token>& line,int &i);

// Parse expressions with + and - operators
string parseExpression(const vector<Token>& line,int &i) {
    string left = parseTerm(line,i);
    while (i<line.size() && (line[i].lexeme=="+" || line[i].lexeme=="-")) {
        string op = line[i].lexeme;
        ++i;
        string right = parseTerm(line,i);
        string temp = newTemp();
        addQuad(op,left,right,temp,temp+"="+left+op+right);
        left = temp;
    }
    return left;
}

// Parse terms with * and / operators
string parseTerm(const vector<Token>& line,int &i) {
    string left = parseFactor(line,i);
    while (i<line.size() && (line[i].lexeme=="*" || line[i].lexeme=="/")) {
        string op = line[i].lexeme;
        ++i;
        string right = parseFactor(line,i);
        string temp = newTemp();
        addQuad(op,left,right,temp,temp+"="+left+op+right);
        left = temp;
    }
    return left;
}

// Parse factors and handle exponentiation
string parseFactor(const vector<Token>& line,int &i) {
    string left = tokenValue(line[i]);
    ++i;
    if (i<line.size() && line[i].lexeme=="^") {
        ++i;
        string right = parseFactor(line,i);
        string temp = newTemp();
        addQuad("^",left,right,temp,temp+"="+left+"^"+right);
        return temp;
    }
    return left;
}

void parseAssignment(const vector<Token>& line, int i) {
    if (i+1<line.size() && line[i+1].lexeme=="(") {
        string arrayName=line[i].lexeme;
        string index=line[i+2].lexeme;
        int pos=i+5;
        string value=parseExpression(line,pos);
        addQuad("=",value,index,arrayName,arrayName+"("+index+")="+value);
    }
    else {
        string lhs=line[i].lexeme;
        int pos=i+2;
        string value=parseExpression(line,pos);
        addQuad("=",value,"",lhs,lhs+"="+value);
    }
}

void parseVariable(const vector<Token>& line,int i) {
    if (i<line.size() && line[i].type==Reserved){
        string type=line[i].lexeme;
        i+=2;
        while (i<line.size()) {
            if (line[i].type==Identifier) {
                addVariable(line[i].lexeme,type);
                if (variableSet.insert(line[i].lexeme).second)
                    variableList.push_back(line[i].lexeme);
            }
            ++i;
            if (i<line.size() && line[i].lexeme==",") ++i;
            else break;
        }
    }
}

void parseDimension(const vector<Token>& line,int i){
    if (i<line.size() && line[i].type==Reserved){
        string type=line[i].lexeme;
        i+=2;
        if (i<line.size() && line[i].type==Identifier){
            string name=line[i].lexeme; i+=2;
            if (variableSet.insert(name).second) variableList.push_back(name);
            if (i<line.size() && line[i].type==Integer){
                int size=stoi(line[i].lexeme);
                addArray(name,type,size);
            }
        }
    }
}

void parseGoto(const vector<Token>& line,int i){
    if (i<line.size() && line[i].type==Identifier)
        addQuad("GTO","","",line[i].lexeme,"GTO "+line[i].lexeme);
}

void parseLabel(const vector<Token>& line,int i){
    while (i<line.size()) {
        if (line[i].type==Identifier){
            if (labelSet.insert(line[i].lexeme).second)
                labelList.push_back(line[i].lexeme);
        }
        ++i;
        if (i<line.size() && line[i].lexeme==",") ++i;
        else break;
    }
}

// forward declaration for recursive parsing
void parseIf(const vector<Token>& line,int i);
void parseStatement(const vector<Token>& line);

void parseStatement(const vector<Token>& line){
    if (line.empty()) return;
    if (line[0].type==Reserved){
        const string &word=line[0].lexeme;
        if (word=="VARIABLE") parseVariable(line,1);
        else if (word=="DIMENSION") parseDimension(line,1);
        else if (word=="LABEL") parseLabel(line,1);
        else if (word=="IF") parseIf(line,1);
        else if (word=="GTO") parseGoto(line,1);
        else if (word=="ENP") addQuad("ENP","","","","ENP");
    }
    else if (line[0].type==Identifier){
        parseAssignment(line,0);
    }
}

void parseIf(const vector<Token>& line,int i){
    string left=tokenValue(line[i]);
    string relop=line[i+1].lexeme;
    string right=tokenValue(line[i+2]);
    string tempCond=newTemp();
    addQuad(relop,left,right,tempCond,tempCond+"="+left+" "+relop+" "+right);
    int pos=i+3;
    if (pos<line.size() && line[pos].lexeme=="THEN") ++pos;
    int elsePos=pos;
    while (elsePos<line.size() && line[elsePos].lexeme!="ELSE") ++elsePos;
    string falseLabel=newLabel();
    string endLabel=newLabel();
    addQuad("IF",tempCond,"",falseLabel,"IF "+tempCond+" == FALSE GO TO "+falseLabel);
    vector<Token> thenPart(line.begin()+pos,line.begin()+elsePos);
    bool thenIsGoto = !thenPart.empty() && thenPart[0].lexeme == "GTO";
    parseStatement(thenPart);
    if (elsePos<line.size()){
        if (!thenIsGoto)
            addQuad("GTO","","",endLabel,"GTO "+endLabel);
        addQuad("LABEL","","",falseLabel,falseLabel);
        vector<Token> elsePart(line.begin()+elsePos+1,line.end());
        parseStatement(elsePart);
        if (!thenIsGoto)
            addQuad("LABEL","","",endLabel,endLabel);
    }
    else {
        addQuad("LABEL","","",falseLabel,falseLabel);
    }
}

void parseProgram() {
    for (const auto &line : tokens){
        if (line.empty()) continue;
        int i=0;
        if (line[i].type==Identifier && line.size()>1 && line[1].type==Reserved) {
            addQuad("LABEL","","",line[i].lexeme,line[i].lexeme);
            if (labelSet.insert(line[i].lexeme).second) labelList.push_back(line[i].lexeme);
            ++i;
        }
        if (i >= line.size()) continue;
        const Token &tok=line[i];
        if (tok.type==Reserved){
            const string &word=tok.lexeme;
            if (word=="VARIABLE") parseVariable(line,i+1);
            else if (word=="DIMENSION") parseDimension(line,i+1);
            else if (word=="LABEL") parseLabel(line,i+1);
            else if (word=="IF") parseIf(line,i+1);
            else if (word=="GTO") parseGoto(line,i+1);
            else if (word=="ENP") addQuad("ENP","","","","ENP");
        }
        else if (tok.type==Identifier) {
            parseAssignment(line,i);
        }
    }
}

int main(){
    if (!loadDelimiters("table1.table") || !loadReserved("table2.table")) {
        return 1;
    }
    string filename;
    while (true) {
        cout << "Enter source filename: ";
        if (!(cin >> filename)) {
            cerr << "No input file provided." << endl;
            return 1;
        }
        if (tokenize(filename)) break;
        cerr << "Unable to open source file '" << filename << "'. Please try again." << endl;
    }
    parseProgram();
    writeQuadTable("table6.table");
    cout << "Compilation finished. IR written to table6.table" << endl;
    return 0;
}
