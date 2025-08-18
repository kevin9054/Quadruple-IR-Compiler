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
vector<string> sourceLines;

// collect declared variables and labels for output ordering
unordered_set<string> variableSet;
vector<string> variableList;
unordered_set<string> labelSet;
vector<string> labelList;

bool syntaxError = false;
struct SyntaxErr { int line; string message; };
vector<SyntaxErr> syntaxErrors;
int currentLine = 0;
void reportSyntaxError(const string &msg) {
    syntaxError = true;
    syntaxErrors.push_back({currentLine+1, msg});
    cerr << "Syntax Error on line " << currentLine+1 << ": " << msg << endl;
    if (currentLine > 0)
        cerr << currentLine << ": " << sourceLines[currentLine-1] << endl;
    cerr << currentLine+1 << ": " << sourceLines[currentLine] << endl;
    if (currentLine+1 < sourceLines.size())
        cerr << currentLine+2 << ": " << sourceLines[currentLine+1] << endl;
}

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
    sourceLines.clear();
    string line;
    while (getline(in, line)) {
        if (!line.empty() && line.back()=='\r') line.pop_back();
        sourceLines.push_back(line);
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
struct SymbolInfo {
    string type;
    bool isArray = false;
    vector<int> dimensions; // store sizes for each dimension
};

// ----- Symbol table with scope -----
vector<unordered_map<string,SymbolInfo>> symbolTables(1); // global scope at index 0

unordered_map<string,SymbolInfo>& currentTable() {
    return symbolTables.back();
}

void pushScope() {
    symbolTables.push_back({});
}

void popScope() {
    if (symbolTables.size() > 1)
        symbolTables.pop_back();
}

void addVariable(const string &name, const string &type) {
    currentTable()[name].type = type;
}

void addArray(const string &name, const string &type, const vector<int>& dims) {
    currentTable()[name] = {type, true, dims};
}

bool lookupSymbol(const string &name, SymbolInfo &info) {
    for (int s = (int)symbolTables.size() - 1; s >= 0; --s) {
        auto it = symbolTables[s].find(name);
        if (it != symbolTables[s].end()) {
            info = it->second;
            return true;
        }
    }
    return false;
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
    for (const auto &e : syntaxErrors) {
        out << "Error line " << e.line << ": " << e.message << "\n";
        if (e.line > 1)
            out << e.line-1 << ": " << sourceLines[e.line-2] << "\n";
        out << e.line << ": " << sourceLines[e.line-1] << "\n";
        if (e.line < sourceLines.size())
            out << e.line+1 << ": " << sourceLines[e.line] << "\n";
    }
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

struct ExprResult {
    string name;
    string type; // "INTEGER" or "REAL"
};

ExprResult parseExpression(const vector<Token>& line,int &i);
ExprResult parseArrayRef(const vector<Token>& line,int &i);
ExprResult parseFactor(const vector<Token>& line,int &i);
ExprResult parseTerm(const vector<Token>& line,int &i);

// Parse expressions with + and - operators
ExprResult parseExpression(const vector<Token>& line,int &i) {
    ExprResult left = parseTerm(line,i);
    while (i<line.size() && (line[i].lexeme=="+" || line[i].lexeme=="-")) {
        string op = line[i].lexeme;
        ++i;
        ExprResult right = parseTerm(line,i);
        string temp = newTemp();
        addQuad(op,left.name,right.name,temp,temp+"="+left.name+op+right.name);
        string type = (left.type=="REAL" || right.type=="REAL") ? "REAL" : "INTEGER";
        left = {temp, type};
    }
    return left;
}

// Parse terms with * and / operators
ExprResult parseTerm(const vector<Token>& line,int &i) {
    ExprResult left = parseFactor(line,i);
    while (i<line.size() && (line[i].lexeme=="*" || line[i].lexeme=="/")) {
        string op = line[i].lexeme;
        ++i;
        ExprResult right = parseFactor(line,i);
        string temp = newTemp();
        addQuad(op,left.name,right.name,temp,temp+"="+left.name+op+right.name);
        string type = (op=="/" || left.type=="REAL" || right.type=="REAL") ? "REAL" : "INTEGER";
        left = {temp, type};
    }
    return left;
}

// Parse array references like A(I) or B(I,J)
ExprResult parseArrayRef(const vector<Token>& line,int &i) {
    string base = line[i].lexeme; // array name
    ++i; // move past identifier
    if (i>=line.size() || line[i].lexeme!="(") {
        reportSyntaxError("Expected '(' after array name");
        return {base, "INTEGER"};
    }
    ++i; // consume '('
    vector<ExprResult> indices;
    while (i<line.size() && line[i].lexeme!=")") {
        ExprResult idx = parseExpression(line,i);
        indices.push_back(idx);
        if (i<line.size() && line[i].lexeme==",") ++i; else break;
    }
    if (i>=line.size() || line[i].lexeme!=")") {
        reportSyntaxError("Missing ')' in array reference");
    } else {
        ++i; // consume ')'
    }
    SymbolInfo info; string elemType="INTEGER";
    if (lookupSymbol(base, info)) elemType = info.type;
    string tempBase = base;
    for (const auto &idx : indices) {
        string tmp = newTemp();
        addQuad("[]",tempBase,idx.name,tmp,tmp+"="+tempBase+"("+idx.name+")");
        tempBase = tmp;
    }
    return {tempBase, elemType};
}

// Parse factors and handle exponentiation
ExprResult parseFactor(const vector<Token>& line,int &i) {
    if (i>=line.size()) {
        reportSyntaxError("Unexpected end of expression");
        return {"", "INTEGER"};
    }

    ExprResult left;

    // Handle parenthesized subexpressions
    if (line[i].type==Identifier && i+1<line.size() && line[i+1].lexeme=="(") {
        left = parseArrayRef(line,i);
    }
    else if (line[i].lexeme == "(") {
        ++i; // consume '('
        left = parseExpression(line,i);
        if (i>=line.size() || line[i].lexeme != ")") {
            reportSyntaxError("Missing ')' in expression");
        } else {
            ++i; // consume ')'
        }
    } else {
        left.name = tokenValue(line[i]);
        if (line[i].type==Real) left.type="REAL";
        else if (line[i].type==Integer) left.type="INTEGER";
        else if (line[i].type==Identifier) {
            SymbolInfo info;
            left.type = lookupSymbol(line[i].lexeme, info) ? info.type : "INTEGER";
        } else left.type = "INTEGER";
        ++i;
    }

    // Handle exponentiation with right associativity
    if (i<line.size() && line[i].lexeme=="^") {
        ++i;
        if (i>=line.size()) {
            reportSyntaxError("Missing right operand for '^'");
            return left;
        }
        ExprResult right = parseFactor(line,i);
        string temp = newTemp();
        addQuad("^",left.name,right.name,temp,temp+"="+left.name+"^"+right.name);
        string type = (left.type=="REAL" || right.type=="REAL") ? "REAL" : "INTEGER";
        return {temp, type};
    }
    return left;
}

void parseAssignment(const vector<Token>& line, int i) {
    if (i+1<line.size() && line[i+1].lexeme=="(") {
        string arrayName=line[i].lexeme;
        int pos=i+2;
        ExprResult index=parseExpression(line,pos);
        if (pos>=line.size() || line[pos].lexeme!=")") {
            reportSyntaxError("Missing ')' in array assignment");
            return;
        }
        ++pos;
        if (pos>=line.size() || line[pos].lexeme!="=") {
            reportSyntaxError("Expected '=' in array assignment");
            return;
        }
        ++pos;
        ExprResult value=parseExpression(line,pos);
        SymbolInfo info; string elemType="INTEGER";
        if (lookupSymbol(arrayName, info)) elemType = info.type;
        string rhs = value.name;
        if (elemType=="INTEGER" && value.type=="REAL") {
            string tmp = newTemp();
            addQuad("INT", value.name, "", tmp, tmp+"=INT("+value.name+")");
            rhs = tmp;
        }
        addQuad("=",rhs,index.name,arrayName,arrayName+"("+index.name+")="+rhs);
    } else {
        if (i+1>=line.size() || line[i+1].lexeme!="=") {
            reportSyntaxError("Expected '=' in assignment");
            return;
        }
        string lhs=line[i].lexeme;
        int pos=i+2;
        ExprResult value=parseExpression(line,pos);
        SymbolInfo info; string lhsType="INTEGER";
        if (lookupSymbol(lhs, info)) lhsType = info.type;
        string rhs = value.name;
        if (lhsType=="INTEGER" && value.type=="REAL") {
            string tmp = newTemp();
            addQuad("INT", value.name, "", tmp, tmp+"=INT("+value.name+")");
            rhs = tmp;
        }
        addQuad("=",rhs,"",lhs,lhs+"="+rhs);
    }
}

void parseVariable(const vector<Token>& line,int i) {
    if (i<line.size() && line[i].type==Reserved){
        string type=line[i].lexeme;
        if (i+1>=line.size() || line[i+1].lexeme != ":") {
            reportSyntaxError("Expected ':' after type in VARIABLE declaration");
            return;
        }
        i+=2;
        if (i>=line.size()) {
            reportSyntaxError("Missing identifier in VARIABLE declaration");
            return;
        }
        while (i<line.size()) {
            if (line[i].type==Identifier) {
                addVariable(line[i].lexeme,type);
                if (variableSet.insert(line[i].lexeme).second)
                    variableList.push_back(line[i].lexeme);
            } else {
                reportSyntaxError("Expected identifier in VARIABLE declaration");
                break;
            }
            ++i;
            if (i<line.size() && line[i].lexeme==",") ++i;
            else break;
        }
    } else {
        reportSyntaxError("VARIABLE declaration missing type");
    }
}

void parseProgramStart(const vector<Token>& line,int i){
    if (i<line.size() && line[i].type==Identifier){
        addQuad("PROGRAM","","",line[i].lexeme,"PROGRAM "+line[i].lexeme);
    } else {
        reportSyntaxError("PROGRAM missing program name");
    }
}

void parseSubroutine(const vector<Token>& line,int i){
    if (i>=line.size() || line[i].type!=Identifier) {
        reportSyntaxError("SUBROUTINE missing name");
        return;
    }
    string name=line[i].lexeme;
    addQuad("SUBROUTINE","","",name,"SUBROUTINE "+name);
    pushScope();
    ++i;
    if (i<line.size() && line[i].lexeme=="(") {
        ++i;
        while (i<line.size() && line[i].lexeme!=")") {
            if (i>=line.size() || line[i].type!=Reserved) {
                reportSyntaxError("Expected type in parameter list");
                break;
            }
            string type=line[i].lexeme; ++i;
            if (i>=line.size() || line[i].lexeme != ":") {
                reportSyntaxError("Expected ':' in parameter list");
                break;
            }
            ++i;
            while (i<line.size() && line[i].type==Identifier) {
                string param=line[i].lexeme;
                addVariable(param,type);
                if (variableSet.insert(param).second) variableList.push_back(param);
                addQuad("PARAM","","",param,"PARAM "+param);
                ++i;
                if (i<line.size() && line[i].lexeme==",") { ++i; continue; }
                else break;
            }
            if (i<line.size() && line[i].lexeme==";") { ++i; continue; }
            else break;
        }
        if (i>=line.size() || line[i].lexeme!=")")
            reportSyntaxError("Missing ')' in parameter list");
        else
            ++i;
    }
}

void parseDimension(const vector<Token>& line,int i){
    if (i>=line.size() || line[i].type!=Reserved){
        reportSyntaxError("DIMENSION missing type");
        return;
    }
    string type=line[i].lexeme;
    if (i+1>=line.size() || line[i+1].lexeme != ":") {
        reportSyntaxError("Expected ':' after type in DIMENSION");
        return;
    }
    i+=2; // move past type and ':'
    while (i<line.size()) {
        if (line[i].type!=Identifier) {
            reportSyntaxError("Expected array name in DIMENSION");
            return;
        }
        string name=line[i].lexeme; ++i;
        if (i>=line.size() || line[i].lexeme!="(") {
            reportSyntaxError("Expected '(' after array name in DIMENSION");
            return;
        }
        ++i; // consume '('
        vector<int> dims;
        while (i<line.size() && line[i].type==Integer) {
            dims.push_back(stoi(line[i].lexeme));
            ++i;
            if (i<line.size() && line[i].lexeme==",") ++i;
            else break;
        }
        if (i>=line.size() || line[i].lexeme!=")") {
            reportSyntaxError("Expected ')' after size in DIMENSION");
            return;
        }
        ++i; // consume ')'
        if (variableSet.insert(name).second) variableList.push_back(name);
        addArray(name,type,dims);
        if (i<line.size() && line[i].lexeme==",") { ++i; continue; }
        else break;
    }
}

void parseGoto(const vector<Token>& line,int i){
    if (i<line.size() && line[i].type==Identifier)
        addQuad("GTO","","",line[i].lexeme,"GTO "+line[i].lexeme);
    else
        reportSyntaxError("GTO missing label");
}

void parseCall(const vector<Token>& line,int i){
    if (i>=line.size() || line[i].type!=Identifier){
        reportSyntaxError("CALL missing subroutine name");
        return;
    }
    string name=line[i].lexeme;
    ++i;
    int paramCount=0;
    if (i<line.size() && line[i].lexeme=="(") {
        ++i; // consume '('
        while (i<line.size() && line[i].lexeme!=")") {
            ExprResult arg=parseExpression(line,i);
            addQuad("ARG","","",arg.name,"ARG "+arg.name);
            ++paramCount;
            if (i<line.size() && line[i].lexeme==",") ++i; else break;
        }
        if (i>=line.size() || line[i].lexeme!=")") {
            reportSyntaxError("Missing ')' in CALL");
        } else {
            ++i;
        }
    }
    addQuad("CALL",to_string(paramCount),"",name,"CALL "+name);
}

void parseIO(const string& op,const vector<Token>& line,int i){
    while (i<line.size()){
        if (line[i].type==Identifier)
            addQuad(op,"","",line[i].lexeme,op+" "+line[i].lexeme);
        else
            reportSyntaxError(op+" expects identifier");
        ++i;
        if (i<line.size() && line[i].lexeme==",") ++i;
        else break;
    }
}

void parseLabel(const vector<Token>& line,int i){
    while (i<line.size()) {
        if (line[i].type==Identifier){
            if (labelSet.insert(line[i].lexeme).second)
                labelList.push_back(line[i].lexeme);
        } else {
            reportSyntaxError("LABEL expects identifier");
            break;
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
        else if (word=="PROGRAM") parseProgramStart(line,1);
        else if (word=="SUBROUTINE") parseSubroutine(line,1);
        else if (word=="CALL") parseCall(line,1);
        else if (word=="INPUT") parseIO("INPUT",line,1);
        else if (word=="OUTPUT") parseIO("OUTPUT",line,1);
        else if (word=="ENS") { addQuad("ENS","","","","ENS"); popScope(); }
        else if (word=="ENP") addQuad("ENP","","","","ENP");
        else reportSyntaxError("Unknown reserved word '"+word+"'");
    }
    else if (line[0].type==Identifier){
        parseAssignment(line,0);
    }
    else {
        reportSyntaxError("Unrecognized statement");
    }
}

void parseIf(const vector<Token>& line,int i){
    if (i+2>=line.size()) {
        reportSyntaxError("Incomplete IF condition");
        return;
    }
    string left=tokenValue(line[i]);
    string relop=line[i+1].lexeme;
    string right=tokenValue(line[i+2]);
    string tempCond=newTemp();
    addQuad(relop,left,right,tempCond,tempCond+"="+left+" "+relop+" "+right);
    int pos=i+3;
    if (pos>=line.size() || line[pos].lexeme!="THEN") {
        reportSyntaxError("IF missing THEN");
        return;
    }
    ++pos;
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
    for (size_t lineNum = 0; lineNum < tokens.size(); ++lineNum){
        currentLine = lineNum;
        const auto &line = tokens[lineNum];
        if (line.empty()) continue;
        int i=0;
        if (line[i].type==Identifier && line.size()>1 && line[1].type==Reserved) {
            addQuad("LABEL","","",line[i].lexeme,line[i].lexeme);
            if (labelSet.insert(line[i].lexeme).second) labelList.push_back(line[i].lexeme);
            ++i;
        }
        if (i >= line.size()) continue;
        vector<Token> rest(line.begin()+i,line.end());
        parseStatement(rest);
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
    if (syntaxError)
        cerr << "Compilation finished with syntax errors." << endl;
    else
        cout << "Compilation finished. IR written to table6.table" << endl;
    return 0;
}
