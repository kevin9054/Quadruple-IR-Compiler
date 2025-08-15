#include <bits/stdc++.h>
using namespace std;

struct Quad {
    string op;
    string arg1;
    string arg2;
    string result;
};

static int tempCount = 1;
string newTemp() {
    return "T" + to_string(tempCount++);
}

vector<string> tokenize(const string &line) {
    vector<string> tokens;
    string cur;
    auto flush=[&](){ if(!cur.empty()){ tokens.push_back(cur); cur.clear(); }};
    for(char c: line){
        if(isspace((unsigned char)c)) { flush(); continue; }
        if(isalnum((unsigned char)c) || c=='_' || c=='.') { cur.push_back(c); continue; }
        flush();
        string s(1,c);
        tokens.push_back(s);
    }
    flush();
    return tokens;
}

bool isOperator(const string &tok){
    return tok=="+"||tok=="-"||tok=="*"||tok=="/"||tok=="^";
}
int precedence(const string &op){
    if(op=="^") return 3;
    if(op=="*"||op=="/") return 2;
    if(op=="+"||op=="-") return 1;
    return 0;
}

struct ExprResult{
    vector<Quad> quads;
    string place;
};

ExprResult parseExpression(const vector<string> &tokens){
    vector<string> output; // postfix
    vector<string> opstack;
    for(const string &tok: tokens){
        if(isOperator(tok)){
            while(!opstack.empty() && precedence(opstack.back()) >= precedence(tok)){
                output.push_back(opstack.back());
                opstack.pop_back();
            }
            opstack.push_back(tok);
        }else if(tok=="("){
            opstack.push_back(tok);
        }else if(tok==")"){
            while(!opstack.empty() && opstack.back()!="("){
                output.push_back(opstack.back());
                opstack.pop_back();
            }
            if(!opstack.empty()) opstack.pop_back();
        }else{
            output.push_back(tok);
        }
    }
    while(!opstack.empty()){
        output.push_back(opstack.back());
        opstack.pop_back();
    }
    vector<string> st;
    vector<Quad> quads;
    for(const string &tok: output){
        if(isOperator(tok)){
            if(st.size()<2){
                continue; // error
            }
            string b=st.back(); st.pop_back();
            string a=st.back(); st.pop_back();
            string t=newTemp();
            string opName;
            if(tok=="+") opName="ADD";
            else if(tok=="-") opName="SUB";
            else if(tok=="*") opName="MUL";
            else if(tok=="/") opName="DIV";
            else if(tok=="^") opName="POW";
            quads.push_back({opName,a,b,t});
            st.push_back(t);
        }else{
            st.push_back(tok);
        }
    }
    ExprResult res;
    res.quads=quads;
    if(!st.empty()) res.place=st.back();
    return res;
}

vector<Quad> processLine(const string &origLine, int lineNo, string &err){
    string line=origLine;
    // remove comments if any (after //)
    auto pos=line.find("//");
    if(pos!=string::npos) line=line.substr(0,pos);
    vector<string> tokens=tokenize(line);
    vector<string> upper=tokens;
    for(string &t: upper){
        for(char &c: t) c=toupper(c);
    }
    vector<Quad> quads;
    if(tokens.empty()) return quads;
    string first=upper[0];
    auto expectSemi=[&](const vector<string> &v){ return !v.empty() && v.back()==";"; };
    if(first=="PROGRAM" && tokens.size()>=2){
        quads.push_back({"PROGRAM","","",tokens[1]});
    }else if(first=="GTO" && tokens.size()>=2){
        quads.push_back({"GTO","","",tokens[1]});
    }else if(first=="SUBROUTINE" && tokens.size()>=2){
        quads.push_back({"SUBROUTINE","","",tokens[1]});
    }else if(first=="VARIABLE" && tokens.size()>=2){
        for(size_t i=1;i<tokens.size();++i){
            if(tokens[i]==","||tokens[i]==";") continue;
            quads.push_back({"VAR","","",tokens[i]});
        }
    }else if(first=="CALL" && tokens.size()>=2){
        quads.push_back({"CALL","","",tokens[1]});
    }else if(first=="LABEL" && tokens.size()>=2){
        quads.push_back({"LABEL","","",tokens[1]});
    }else if(first=="INPUT" && tokens.size()>=2){
        for(size_t i=1;i<tokens.size();++i){
            if(tokens[i]==","||tokens[i]==";") continue;
            quads.push_back({"INPUT","","",tokens[i]});
        }
    }else if(first=="DIMENSION" && tokens.size()>=4){
        string var=tokens[1];
        string size;
        size_t p=line.find('(');
        if(p!=string::npos){
            size_t q=line.find(')',p);
            if(q!=string::npos){
                size=line.substr(p+1,q-p-1);
            }
        }
        quads.push_back({"DIM",size,"",var});
    }else if(first=="OUTPUT" && tokens.size()>=2){
        for(size_t i=1;i<tokens.size();++i){
            if(tokens[i]==","||tokens[i]==";") continue;
            quads.push_back({"OUTPUT","","",tokens[i]});
        }
    }else if(first=="IF"){
        vector<string> relOps={"EQ","NE","GT","LT","GE","LE"};
        size_t opPos=SIZE_MAX; string relOp;
        for(size_t i=1;i<upper.size();++i){
            for(const string &ro: relOps){
                if(upper[i]==ro){ opPos=i; relOp=ro; break; }
            }
            if(opPos!=SIZE_MAX) break;
        }
        size_t thenPos=SIZE_MAX, elsePos=SIZE_MAX;
        for(size_t i=1;i<upper.size();++i){
            if(upper[i]=="THEN") thenPos=i;
            else if(upper[i]=="ELSE") elsePos=i;
        }
        if(opPos==SIZE_MAX || thenPos==SIZE_MAX){
            err="Syntax error"; return quads;
        }
        vector<string> expr1(tokens.begin()+1, tokens.begin()+opPos);
        vector<string> expr2(tokens.begin()+opPos+1, tokens.begin()+thenPos);
        ExprResult e1=parseExpression(expr1);
        ExprResult e2=parseExpression(expr2);
        quads.insert(quads.end(), e1.quads.begin(), e1.quads.end());
        quads.insert(quads.end(), e2.quads.begin(), e2.quads.end());
        string labelTrue = tokens[thenPos+1];
        string labelFalse = (elsePos!=SIZE_MAX && elsePos+1<tokens.size())?tokens[elsePos+1]:"";
        string opName="IF_"+relOp;
        quads.push_back({opName,e1.place,e2.place,labelTrue});
        if(!labelFalse.empty()) quads.push_back({"GOTO","","",labelFalse});
    }else{
        // check assignment
        auto it=find(tokens.begin(), tokens.end(), "=");
        if(it!=tokens.end() && tokens.size()>=3){
            string lhs=tokens[0];
            vector<string> expr(it+1,tokens.end());
            if(!expr.empty() && expr.back()==";") expr.pop_back();
            ExprResult e=parseExpression(expr);
            quads.insert(quads.end(), e.quads.begin(), e.quads.end());
            quads.push_back({"ASSIGN",e.place,"",lhs});
        }else{
            err="Syntax error";
        }
    }
    return quads;
}

int main(int argc, char *argv[]){
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    istream *in=&cin;
    if(argc>1){
        ifstream *fin=new ifstream(argv[1]);
        if(fin->good()) in=fin; else { cerr<<"Cannot open input file\n"; return 1; }
    }
    string line; int lineNo=1;
    while(getline(*in,line)){
        string err;
        auto quads=processLine(line,lineNo,err);
        if(!err.empty()){
            cout << "Line " << lineNo << ": " << err << " -> " << line << '\n';
        }else{
            for(size_t i=0;i<quads.size();++i){
                const Quad &q=quads[i];
                cout << '(' << q.op << ',' << q.arg1 << ',' << q.arg2 << ',' << q.result << ')';
                if(i==quads.size()-1) cout << " [" << line << ']';
                cout << '\n';
            }
        }
        lineNo++;
    }
    return 0;
}

