#ifndef COMMAND_H
#define COMMAND_H
#include "common.h"
#include <string>
#include <iostream>
#include <sstream>
#include <map>

using namespace std;
namespace command {

// keyword - packs a string which represents a special keyword
struct keyword {
	string word;
	inline keyword(string word) :
		word(word)
	{ }
};

// readValue - reads a value of the specified type T from the specified istream is. //
template<typename T>
inline optional<T> readValue(istream& is) {
	T r;
	if (is >> r) {
		return optional<T>(r);
	} else {
		return optional<T>();
	}
};

template<>
inline optional<string> readValue<string>(istream& is) {
	while (is.peek() == ' ') { is.ignore(1); }
	if (is.peek() == '\"') {
		is.ignore(1);
		stringstream ss;
		char c;
		while(is.good()) {
			c = is.get();
			if (!is.good()) {
				return optional<string>();
			}
			switch(c) {
				case '\\': {
					c = is.get();
					if (!is.good()) {
						return optional<string>();
					}
					switch(c) {
						case 'n': ss.put('\n'); break;
						case 'r': ss.put('\r'); break;
						case 't': ss.put('\t'); break;
						default: ss.put(c); break;
					}
					break;
				}
				case '\"': {
					return optional<string>(ss.str());
				}
				default: ss.put(c); break;
			}
		}
		// Should never reach here.
		return optional<string>();
	} else {
		string r;
		is >> r;
		return optional<string>(r);
	}
};

// writeValue - writes the specified value of the specified type T to the specified ostream os.
template<typename T>
inline void writeValue(ostream& os, T arg) {
	os << arg;
}

template<>
inline void writeValue<string>(ostream& os, string arg) {
	os.put('\"');
	for(int i=0;i<arg.length();i++) {
		switch(arg[i]) {
			case '\"':	os.put('\\'); os.put('"'); break;
			case '\n':	os.put('\\'); os.put('n'); break;
			case '\r':	os.put('\\'); os.put('r'); break;
			case '\t':	os.put('\\'); os.put('t'); break;
			default: os.put(arg[i]); break;
		}
	}
	os.put('\"');
}

template<>
inline void writeValue<const char*>(ostream& os, const char *arg) {
	writeValue<string>(os, string(arg));
}

template<>
inline void writeValue<char*>(ostream& os, char *arg) {
	writeValue<const char*>(os, (const char*)arg);
}

template<>
inline void writeValue<keyword>(ostream& os, keyword arg) {
	os << arg.word;
}


// writeValues - writes multiple values to the specified ostream
#pragma push_macro("SPACE")
#define SPACE os << " "
template<typename T1> inline void writeValues(ostream& os, T1 t1) { writeValue(os, t1); }
template<typename T1, typename T2> inline void writeValues(ostream& os, T1 t1, T2 t2) { writeValue(os, t1); SPACE; writeValue(os, t2); }
template<typename T1, typename T2, typename T3> inline void writeValues(ostream& os, T1 t1, T2 t2, T3 t3) { writeValue(os, t1); SPACE; writeValue(os, t2); SPACE; writeValue(os, t3); }
template<typename T1, typename T2, typename T3, typename T4> inline void writeValues(ostream& os, T1 t1, T2 t2, T3 t3, T4 t4) { writeValue(os, t1); SPACE; writeValue(os, t2); SPACE; writeValue(os, t3); SPACE; writeValue(os, t4); }
template<typename T1, typename T2, typename T3, typename T4, typename T5> inline void writeValues(ostream& os, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5) { writeValue(os, t1); SPACE; writeValue(os, t2); SPACE; writeValue(os, t3); SPACE; writeValue(os, t4); SPACE; writeValue(os, t5); }
template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6> inline void writeValues(ostream& os, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6) { writeValue(os, t1); SPACE; writeValue(os, t2); SPACE; writeValue(os, t3); SPACE; writeValue(os, t4); SPACE; writeValue(os, t5); SPACE; writeValue(os, t6); }
#pragma pop_macro("SPACE")

// readValues - reads multiple values from the specified ostream to the specified references
#pragma push_macro("READARG")
#define READARG(T,t) { optional<T> _##t = readValue<T>(is); if (!_##t.hasValue()) { return false; } *t = _##t.value(); return true; }
template<typename T1> inline bool readValues(istream& is, T1* t1) { READARG(T1,t1); }
template<typename T1,typename T2> inline bool readValues(istream& is, T1* t1, T2* t2) { READARG(T1,t1); READARG(T2,t2); }
template<typename T1,typename T2,typename T3> inline bool readValues(istream& is, T1* t1, T2* t2, T3* t3) { READARG(T1,t1); READARG(T2,t2); READARG(T3,t3); }
template<typename T1,typename T2,typename T3,typename T4> inline bool readValues(istream& is, T1* t1, T2* t2, T3* t3,T4* t4) { READARG(T1,t1); READARG(T2,t2); READARG(T3,t3); READARG(T4,t4); }
template<typename T1,typename T2,typename T3,typename T4,typename T5> inline bool readValues(istream& is, T1* t1, T2* t2, T3* t3,T4* t4,T5* t5) { READARG(T1,t1); READARG(T2,t2); READARG(T3,t3); READARG(T4,t4); READARG(T5,t5); }
template<typename T1,typename T2,typename T3,typename T4,typename T5,typename T6> inline bool readValues(istream& is, T1* t1, T2* t2, T3* t3,T4* t4,T5* t5,T6* t6) { READARG(T1,t1); READARG(T2,t2); READARG(T3,t3); READARG(T4,t4); READARG(T5,t5); READARG(T6,t6); }
#pragma pop_macro("READARG")

#define writetemplates(FNAME,OUT,PREARGS,PREWRITE,POSTWRITE) \
template<typename T1> inline void FNAME(PREARGS T1 t1) { PREWRITE writeValues(OUT, t1); POSTWRITE } \
template<typename T1, typename T2> inline void FNAME(PREARGS T1 t1, T2 t2) { PREWRITE writeValues(OUT, t1, t2); POSTWRITE } \
template<typename T1, typename T2, typename T3> inline void FNAME(PREARGS T1 t1, T2 t2, T3 t3) { PREWRITE writeValues(OUT, t1, t2, t3); POSTWRITE } \
template<typename T1, typename T2, typename T3, typename T4> inline void FNAME(PREARGS T1 t1, T2 t2, T3 t3, T4 t4) { PREWRITE writeValues(OUT, t1, t2, t3, t4); POSTWRITE } \
template<typename T1, typename T2, typename T3, typename T4, typename T5> inline void FNAME(PREARGS T1 t1, T2 t2, T3 t3, T4 t4, T5 t5) { PREWRITE writeValues(OUT, t1, t2, t3, t4, t5); POSTWRITE } \
template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6> inline void FNAME(PREARGS T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6) { PREWRITE writeValues(OUT, t1, t2, t3, t4, t5, t6); POSTWRITE }

#define readtemplates(FNAME,IN,RETURNTYPE,PREARGS,PREREAD,POSTREAD) \
template<typename T1> inline RETURNTYPE FNAME(PREARGS T1* t1) { PREREAD bool r=readValues(IN, t1); POSTREAD } \
template<typename T1, typename T2> inline RETURNTYPE FNAME(PREARGS T1* t1, T2* t2) { PREREAD bool r=readValues(IN, t1, t2); POSTREAD } \
template<typename T1, typename T2, typename T3> inline RETURNTYPE FNAME(PREARGS T1* t1, T2* t2, T3* t3) { PREREAD bool r=readValues(IN, t1, t2, t3); POSTREAD } \
template<typename T1, typename T2, typename T3, typename T4> inline RETURNTYPE FNAME(PREARGS T1* t1, T2* t2, T3* t3, T4* t4) { PREREAD bool r=readValues(IN, t1, t2, t3, t4); POSTREAD } \
template<typename T1, typename T2, typename T3, typename T4, typename T5> inline RETURNTYPE FNAME(PREARGS T1* t1, T2* t2, T3* t3, T4* t4, T5* t5) { PREREAD bool r=readValues(IN, t1, t2, t3, t4, t5); POSTREAD } \
template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6> inline RETURNTYPE FNAME(PREARGS T1* t1, T2* t2, T3* t3, T4* t4, T5* t5, T6 *t6) { PREREAD bool r=readValues(IN, t1, t2, t3, t4, t5, t6); POSTREAD }

#define COMMA ,
#define SEMICOLON ;

inline void output(string name) { cerr << name << endl; }
writetemplates(output,cerr,string name COMMA,cerr << name << " " SEMICOLON,cerr << endl SEMICOLON)

class CommandContext {
	istream& is;
	ostream& os;
	int contextid;
	bool done;
public:
	inline CommandContext(istream& is, ostream& os, int contextid) :
		is(is),
		os(os),
		contextid(contextid),
		done(false)
	{}

	inline void succeeded() {
		checkdone();
		printcontextid();
		os << "succeeded";
		endline();
	}
	template<typename T1> inline void succeeded(T1 t1) {
		checkdone();
		printcontextid();
		os << "succeeded ";
		writeValue(os, t1);
		endline();
	}

	inline void failed() {
		checkdone();
		printcontextid();
		os << "failed";
		endline();
	}
	template<typename T1> void failed(T1 t1) {
		checkdone();
		printcontextid();
		os << "failed ";
		writeValue(os, t1);
		endline();
	}
	writetemplates(writeline,os,,checkdone() SEMICOLON printcontextid() SEMICOLON,endline() SEMICOLON)
	readtemplates(read,is,bool,,,return r SEMICOLON)

private:
	inline void printcontextid() { if (contextid != 0) { os << contextid << " "; } }
	inline void endline() { os << endl; }
	inline void checkdone() { if (done) { cerr << "Warning: command already done!" << endl; } }
};


typedef void (*handler)(CommandContext& context);

inline void repl(map<string, handler>& commands) {
    for (;;) {
    	int contextid;
    	string line;
		if (!getline(cin, line)) {
			break;
		}
		istringstream iss(line);
		string command;
		iss >> contextid;

		// Check whether getting the contextid succeeded
		if (!iss.good()) {
			// Rewind the stream when it did not succeed (the command was read instead of an integer)
			iss.seekg(ios::beg);
			iss.clear();
		}
		iss >> command;

		CommandContext context(iss, cout, contextid);
		
		map<string, handler>::iterator it = commands.find(command);
		if (it == commands.end()) {
			context.failed("command not found");
		} else {
			handler ch = it->second;
			ch(context);
		}
    }
}

}

#endif