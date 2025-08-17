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

unordered_set<string> delimiters;
unordered_set<string> reservedWords;
vector<string> integerTable;
vector<string> realTable;
vector<string> identifierTable;
vector<vector<Token>> tokens;

static string toUpper(const string &s) {
    string r = s;
    for (char &c : r) c = toupper(static_cast<unsigned char>(c));
    return r;
}

static void loadDelimiters(const string &file) {
    ifstream in(file); string s;
    while (getline(in, s)) {
        if (!s.empty() && s.back()=='\r') s.pop_back();
        if (!s.empty()) delimiters.insert(s);
    }
}

static void loadReserved(const string &file) {
    ifstream in(file); string s;
    while (getline(in, s)) {
        if (!s.empty() && s.back()=='\r') s.pop_back();
        if (!s.empty()) reservedWords.insert(toUpper(s));
    }
}

static int addInteger(const string &s) {
    auto it = find(integerTable.begin(), integerTable.end(), s);
    if (it != integerTable.end()) return distance(integerTable.begin(), it) + 1;
    integerTable.push_back(s); return integerTable.size();
}

static int addReal(const string &s) {
    auto it = find(realTable.begin(), realTable.end(), s);
    if (it != realTable.end()) return distance(realTable.begin(), it) + 1;
    realTable.push_back(s); return realTable.size();
}

static int addIdentifier(const string &s) {
    auto it = find(identifierTable.begin(), identifierTable.end(), s);
    if (it != identifierTable.end()) return distance(identifierTable.begin(), it) + 1;
    identifierTable.push_back(s); return identifierTable.size();
}

static void tokenize(const string &inputFile) {
    ifstream in(inputFile);
    string line;
    while (getline(in, line)) {
        if (!line.empty() && line.back()=='\r') line.pop_back();
        vector<Token> lineTokens; size_t i = 0;
        while (i < line.size()) {
            char c = line[i];
            if (isspace(static_cast<unsigned char>(c))) { ++i; continue; }
            if (delimiters.count(string(1,c))) {
                lineTokens.push_back({Delimiter,string(1,c),-1});
                ++i; continue;
            }
            if (isalpha(static_cast<unsigned char>(c))) {
                string word;
                while (i<line.size() && (isalnum(static_cast<unsigned char>(line[i]))||line[i]=='_'))
                    word += line[i++];
                string upper = toUpper(word);
                if (reservedWords.count(upper))
                    lineTokens.push_back({Reserved,upper,-1});
                else {
                    int idx = addIdentifier(word);
                    lineTokens.push_back({Identifier,word,idx});
                }
                continue;
            }
            if (isdigit(static_cast<unsigned char>(c))) {
                string num; bool isReal=false;
                while (i<line.size() && isdigit(static_cast<unsigned char>(line[i]))) num += line[i++];
                if (i<line.size() && line[i]=='.') {
                    isReal=true; num+=line[i++];
                    while (i<line.size() && isdigit(static_cast<unsigned char>(line[i]))) num += line[i++];
                }
                int idx;
                if (isReal) { idx=addReal(num); lineTokens.push_back({Real,num,idx}); }
                else { idx=addInteger(num); lineTokens.push_back({Integer,num,idx}); }
                continue;
            }
            ++i;
        }
        tokens.push_back(lineTokens);
    }
}

// ----- Semantic info -----
struct SymbolInfo { string type; bool isArray=false; int arraySize=0; };
unordered_map<string,SymbolInfo> symbolTable;

static void addVariable(const string &name,const string &type){ symbolTable[name].type=type; }
static void addArray(const string &name,const string &type,int size){ symbolTable[name]={type,true,size}; }

// ----- IR builder -----
struct Quadruple { string op,arg1,arg2,result; };
vector<Quadruple> quads;
int tempCount=0;
int labelCount=0;

static string newTemp(){ return "T" + to_string(++tempCount); }
static string newLabel(){ return "L" + to_string(++labelCount); }
static void addQuad(const string& op,const string& a1,const string& a2,const string& res){
    quads.push_back({op,a1,a2,res});
}
static void writeQuadTable(const string& file){
    ofstream out(file); int idx=1;
    for (auto &q:quads)
        out << idx++ << " ("<<q.op<<", "<<q.arg1<<", "<<q.arg2<<", "<<q.result<<")\n";
}

// ----- Parser helpers -----
static string tokenValue(const Token &t){ return t.lexeme; }

static string parseExpression(const vector<Token>& line,size_t &i){
    string left = tokenValue(line[i]); ++i;
    if (i<line.size() && line[i].lexeme=="+"){
        ++i; string right=tokenValue(line[i]); ++i;
        string temp=newTemp(); addQuad("ADD",left,right,temp); return temp;
    }
    return left;
}

static void parseAssignment(const vector<Token>& line,size_t i){
    if (i+1<line.size() && line[i+1].lexeme=="(") {
        string arrayName=line[i].lexeme;
        string index=line[i+2].lexeme;
        size_t pos=i+5; string value=parseExpression(line,pos);
        addQuad("STORE",value,index,arrayName);
    } else {
        string lhs=line[i].lexeme; size_t pos=i+2;
        string value=parseExpression(line,pos);
        addQuad("ASSIGN",value,"",lhs);
    }
}

static void parseVariable(const vector<Token>& line,size_t i){
    if (i<line.size() && line[i].type==Reserved){
        string type=line[i].lexeme; i+=2;
        while (i<line.size()){
            if (line[i].type==Identifier) addVariable(line[i].lexeme,type);
            ++i; if (i<line.size() && line[i].lexeme==",") ++i; else break;
        }
    }
}

static void parseDimension(const vector<Token>& line,size_t i){
    if (i<line.size() && line[i].type==Reserved){
        string type=line[i].lexeme; i+=2;
        if (i<line.size() && line[i].type==Identifier){
            string name=line[i].lexeme; i+=2;
            if (i<line.size() && line[i].type==Integer){
                int size=stoi(line[i].lexeme); addArray(name,type,size);
            }
        }
    }
}

static void parseGoto(const vector<Token>& line,size_t i){
    if (i<line.size() && line[i].type==Identifier)
        addQuad("GOTO","","",line[i].lexeme);
}

static void parseIf(const vector<Token>& line,size_t i){
    string left=tokenValue(line[i]);
    string relop=line[i+1].lexeme;
    string right=tokenValue(line[i+2]);
    string tempCond=newTemp();
    addQuad(relop,left,right,tempCond);
    size_t pos=i+3;
    if (pos<line.size() && line[pos].lexeme=="THEN") ++pos;
    if (pos<line.size() && line[pos].lexeme=="GTO"){
        string trueLabel=line[pos+1].lexeme;
        string elseLabel=newLabel();
        addQuad("IF_FALSE",tempCond,"",elseLabel);
        addQuad("GOTO","","",trueLabel);
        addQuad("LABEL","","",elseLabel);
        pos+=2;
        if (pos<line.size() && line[pos].lexeme=="ELSE") ++pos;
        if (pos<line.size() && line[pos].type==Identifier)
            parseAssignment(line,pos);
    }
}

static void parseProgram(){
    for (const auto &line : tokens){
        if (line.empty()) continue;
        size_t i=0;
        if (line[i].type==Identifier && line.size()>1 && line[1].type==Reserved){
            addQuad("LABEL","","",line[i].lexeme);
            ++i;
        }
        if (i>=line.size()) continue;
        const Token &tok=line[i];
        if (tok.type==Reserved){
            const string &word=tok.lexeme;
            if (word=="VARIABLE") parseVariable(line,i+1);
            else if (word=="DIMENSION") parseDimension(line,i+1);
            else if (word=="IF") parseIf(line,i+1);
            else if (word=="GTO") parseGoto(line,i+1);
            else if (word=="ENP") addQuad("ENP","","","");
        } else if (tok.type==Identifier) {
            parseAssignment(line,i);
        }
    }
}

int main(){
    loadDelimiters("table1.table");
    loadReserved("table2.table");
    string filename;
    cout << "Enter source filename: ";
    if (!(cin >> filename)) {
        cerr << "No input file provided." << endl;
        return 1;
    }
    tokenize(filename);
    parseProgram();
    writeQuadTable("table6.table");
    cout << "Compilation finished. IR written to table6.table" << endl;
    return 0;
}

