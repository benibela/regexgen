#include <QTextStream>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QVector>
#include <QTextCodec>
#include <stdio.h>
#include <stdlib.h>

#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)


QMap<QChar, QString> characterClasses;
QString ALL_CHARACTERS = "0-9A-Za-z_+-*/=<>()[]{}!.,;:$%\"'#~&\\\\|@^?";
bool printProgress = false;

struct CharBlock {
	//set on init
	enum Type {CBT_FIXED, CBT_CHOOSE, CBT_BACKTRACK_START, CBT_BACKTRACK_END, CBT_BACKTRACK};
	Type type;
	QByteArray slowData;
	int backtrack;

	//set when evaluating
	const char * data;
	int len;
	int choosen;
	int pos;

};
//typedef QList<CharBlock> BlockString;
//typedef QList<BlockString> BlockStringList;
#define BlockString QList< CharBlock >
#define BlockStringList QList< BlockString >

int randUntil(int until){
	return random() % until;
}

char charConv(const QChar& c){
	return c.toAscii();
}

int hexToInt(const char c){
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return 10 + c - 'a';
  if (c >= 'A' && c <= 'F') return 10 + c - 'A';
  throw "invalid character";
}
unsigned char hexCharacter(const QChar& c1, const QChar& c2){
fprintf(stderr, qPrintable(QString(">%1,%2<\n").arg(c1).arg(c2)));
	return (hexToInt(c1.toLatin1()) << 4) + hexToInt(c2.toLatin1());
}

void removePoint(int pos, QList<int>& points){
	for (int i = 0; i < points.size(); i++)
		if (points[i] > pos)
			points[i]--;
}
void insertPoint(int pos, int len, QList<int>& points){
	if (len == 0) return;
	for (int i = 0; i < points.size(); i++)
		if (points[i] > pos)
			points[i]+=len;
}

//Replace all backtrack references to bracket pairs by references to one certain character
BlockString purifyBacktracking(const BlockString& strin){
	//--Remove all ids of brackets not referenced by references--
	BlockString str = strin;
	QList<int> accessedMatches;
	for (int i=str.size()-1;i >= 0;i--)
		if (str[i].type == CharBlock::CBT_BACKTRACK && !accessedMatches.contains(str[i].backtrack))
			accessedMatches << str[i].backtrack;
	if (accessedMatches.isEmpty()) {
		for (int i=str.size()-1;i >= 0;i--)
			if (str[i].type == CharBlock::CBT_BACKTRACK_START || str[i].type == CharBlock::CBT_BACKTRACK_END)
				str.removeAt(i);
		return str; //optimized case, no back tracking used
	}
	for (int i=str.size()-1;i>=0;i--)
		if (str[i].type == CharBlock::CBT_BACKTRACK_START || str[i].type == CharBlock::CBT_BACKTRACK_END)
			if (!accessedMatches.contains( str[i].backtrack ) )
				str.removeAt(i);
	//--map remaining ids to [0,n]--
	QMap<int, int> btMap;
	foreach (int i, accessedMatches)
		btMap.insert(i, btMap.size());
	//--calculate string intervals--
	QList<int> btStart, btEnd;
	for (int i=0;i<accessedMatches.size();i++) btStart << -1;
	for (int i=0;i<accessedMatches.size();i++) btEnd << -1;
	for (int i=str.size()-1;i>=0;i--)
		if (str[i].type == CharBlock::CBT_BACKTRACK_START || str[i].type == CharBlock::CBT_BACKTRACK_END) {
			int rid = btMap.value(str[i].backtrack);			
			if (str[i].type == CharBlock::CBT_BACKTRACK_START && btStart[rid] == -1 )
				btStart[rid] = i;
			else if (btEnd[rid] == -1)
				btEnd[rid] = i;
		}

	if (btStart.contains(-1)) throw "invalid backtrack index (match bracket not opened)";
	if (btStart.contains(-1)) throw "invalid backtrack index (match bracket not closed)";
	//remove now useless bracket-ids
	for (int i=str.size()-1;i>=0;i--)
		if (str[i].type == CharBlock::CBT_BACKTRACK_START || str[i].type == CharBlock::CBT_BACKTRACK_END) {
			removePoint(i, btStart);
			removePoint(i, btEnd);
			str.removeAt(i);
		}
	//--expand all \i backtrack references to the size of the i-th match--
	//remap
	for (int i=0;i<str.size();i++)
		if (str[i].type == CharBlock::CBT_BACKTRACK)
			str[i].backtrack = btMap.value(str[i].backtrack);
	QVector<bool> done;
	done.resize(accessedMatches.size());
	//topological sort
	//if a bracket referenced by a backtracking reference contains another reference, latter reference must be expaned first
	for (int i=0;i<accessedMatches.size();i++) {
		if (done[i]) continue;
		QList<int> stack;
		QList<int> processing;
		stack.append(i);
		while (!stack.isEmpty()) {
			int c = stack.last();
			done[c] = true;
			processing << c;
			QList<int> higherPrio;
			for (int j=btStart[c]; j < btEnd[c]; j++)
				if (str[j].type == CharBlock::CBT_BACKTRACK && str[j].backtrack >= 0)
					if (!higherPrio.contains(str[j].backtrack) && !done[str[j].backtrack]) {
						if (processing.contains(str[j].backtrack)) throw "Recursive backtrack reference";
						higherPrio << str[j].backtrack;
					}
			if (!higherPrio.isEmpty()) {
				foreach (int j, higherPrio) {
					stack << j;
				}
			} else {
				stack.removeLast();
				processing.removeLast();
				//copy reference until we have one reference per character
				for (int j=btEnd[c]-1; j >= btStart[c]; j--){
					if (str[j].type == CharBlock::CBT_BACKTRACK && str[j].backtrack >= 0) {
						int l = btEnd[str[j].backtrack] - btStart[str[j].backtrack];
						if (l==0) {
							removePoint(i, btStart);
							removePoint(i, btEnd);
							str.removeAt(i);
						} else {
							insertPoint(j, l-1, btStart);
							insertPoint(j, l-1, btEnd);
							str[j].backtrack = - str[j].backtrack - 1; //invert backtrack id to mark processed references
							for (int k=1;k<l;k++) str.insert(j, str[j]);
						}
					}
				}
			}
		}
	}

	//copy reference until we have one reference per character (for references not in brackets)
	for (int j=str.length()-1; j >= 0; j--){
		if (str[j].type == CharBlock::CBT_BACKTRACK && str[j].backtrack >= 0) {
			int l = btEnd[str[j].backtrack] - btStart[str[j].backtrack];
			if (l==0) {
				removePoint(j, btStart);
				removePoint(j, btEnd);
				str.removeAt(j);
			} else {
				insertPoint(j, l-1, btStart);
				insertPoint(j, l-1, btEnd);
				str[j].backtrack = - str[j].backtrack - 1; //invert backtrack id to mark processed references
				for (int k=1;k<l;k++) str.insert(j, str[j]);
			}
		}
	}


	//now all backtrack references have the right length => replace backtrack by character index
	for (int i=0; i<str.length(); i++)
		if (str[i].type == CharBlock::CBT_BACKTRACK) {
			Q_ASSERT(str[i].backtrack < 0); //already processed
			int bt = -str[i].backtrack - 1;
			int l = btEnd[bt] - btStart[bt];
			for (int j=0;j<l;j++, i++) {
				Q_ASSERT(-str[i].backtrack - 1 == bt);
				str[i].backtrack = btStart[bt]+j;
			}
			i--;
		}

	//remove reference chains
	for (int i=0; i<str.length(); i++)
		if (str[i].type == CharBlock::CBT_BACKTRACK) {
			QList<int> cycleKill;
			int j = str[i].backtrack;
			while (str[j].type == CharBlock::CBT_BACKTRACK) {
				Q_ASSERT(!cycleKill.contains(j));
				cycleKill << j;
				j = str[j].backtrack;
			}
			str[i].backtrack = j;
		}

	return str;
}

int64_t countPossibilities(const QList<CharBlock>& blocks) {
	int64_t totalPos = 1;
	foreach (const CharBlock& b, blocks)
		if (b.type == CharBlock::CBT_CHOOSE){
			totalPos = totalPos * b.slowData.length();
		}
	return totalPos;
}

//takes a simplified like (\[.*\]|.)* and prints all possibilities to choose characters in the character sets
//(this will generate thousand-millions of equal-length words)
void printPossibilities(QList<CharBlock>& blocks, bool randomized, int maxLines, int64_t startTotalPos, int64_t combinedTotalPos){
	char word[blocks.length()+1];
	CharBlock vars[blocks.size()+1];
	CharBlock bts[blocks.size()+1];
	int actualBlockCount = 0;
	int actualBackTrackCount = 0;
	int64_t totalPos = 1;
	for (int i=0;i<blocks.size();i++){
		blocks[i].pos = i;
		switch (blocks[i].type){
		case CharBlock::CBT_FIXED: case CharBlock::CBT_CHOOSE:
			blocks[i].choosen = 0;
			blocks[i].len = blocks[i].slowData.length();
			blocks[i].data = blocks[i].slowData.data(); //QByteArray::data keeps the data in memory (in contrast to QString::toAscii and qPrintable which DON'T WORK HERE)
			word[i] = *blocks[i].data;
			totalPos*=blocks[i].len;
			Q_ASSERT(blocks[i].len > 0);
			if (blocks[i].type == CharBlock::CBT_FIXED) Q_ASSERT(blocks[i].len==1);
			else vars[actualBlockCount++] = blocks[i];
			break;
		case CharBlock::CBT_BACKTRACK:
			bts[actualBackTrackCount++] = blocks[i];
			break;
		default: Q_ASSERT(false);
		}

	}
//	for (int i=0;i<actualBlockCount;i++)
//		printf(">%s<\n",vars[i].data);
	word[blocks.length()]=0;
	if (actualBlockCount==0) {
		if (unlikely(actualBackTrackCount)) for (int j=0;j<actualBackTrackCount;j++)
			word[bts[j].pos] = word[bts[j].backtrack];
		printf("%s\n", word);
		return;
	}

	if (maxLines > 0) totalPos = maxLines;
	long int progressNext = printProgress?totalPos / 10:totalPos;

	if (!randomized) {
		long int r = 0;
		if (printProgress)
			fprintf(stderr, "      Progress: %li/%li (%i%%)\n", r, totalPos, ((long int)(r)*100)/totalPos);
		while (true) {
			int i=0;
			for (;i<actualBlockCount && vars[i].choosen == vars[i].len;  i++) {
				vars[i].choosen = 0;
				word[vars[i].pos] = *vars[i].data;
				vars[i+1].choosen++;
			}
			if (likely(i<actualBlockCount)) {
				word[vars[i].pos] = vars[i].data[vars[i].choosen];
				if (unlikely(actualBackTrackCount)) for (int j=0;j<actualBackTrackCount;j++)
					word[bts[j].pos] = word[bts[j].backtrack];
				printf("%s\n", word);
				r++;
				if (unlikely(r>=progressNext)) {
					if (printProgress) {
						fprintf(stderr, "      Progress: %li/%li (%i%%)  %li/%li (%i%%)\n", r, totalPos, ((long int)(r)*100)/totalPos, r + startTotalPos, combinedTotalPos, ((int64_t)(r + startTotalPos)*100)/combinedTotalPos);
						progressNext = qMin(totalPos, progressNext + totalPos/10);
					}
					if (maxLines > 0 && r>=maxLines) break;
				}
			} else {
	//			printf("%s\n", word);
				break;
			}
			vars[0].choosen+=1;
			word[vars[0].pos] = vars[0].data[vars[0].choosen];
		}
	} else {
		if (maxLines <= 0) maxLines = totalPos;
		for (int r=0;r<maxLines;r++) {
			for (int i=0;i<actualBlockCount;i++)
				word[vars[i].pos] = vars[i].slowData[randUntil(vars[i].len)];
			if (unlikely(actualBackTrackCount)) for (int j=0;j<actualBackTrackCount;j++)
				word[bts[j].pos] = word[bts[j].backtrack];
			printf("%s\n", word);
		}
	}
}

void printExpanded(const BlockString& bs, FILE* out = stdout) {
	bool bt = false;
	foreach (const CharBlock& cb, bs)
		if (cb.type == CharBlock::CBT_BACKTRACK)
			bt = true;
	const char * bo = bt?"(":"";
	const char * bc = bt?")":"";
	foreach (const CharBlock& cb, bs)
		if (cb.type == CharBlock::CBT_FIXED)
			fprintf(out,"%s%c%s", bo, cb.slowData[0], bc);
		else if (cb.type == CharBlock::CBT_CHOOSE)
			fprintf(out,"%s[%s]%s", bo, qPrintable(QString(cb.slowData).replace('\\', "\\\\").replace('[', "\\[").replace(']', "\\]")), bc);
		else if (cb.type == CharBlock::CBT_BACKTRACK)
			fprintf(out,"%s\\%i%s", bo, cb.backtrack+1, bc);
		else
			continue;
	fprintf(out,"\n");
}

//expands a simplified [...] range to a list of all character matched by it
//(simplified: no ^, no \, but -; not actually used for the ranges in an input regex but to define \d=[...])
QByteArray convertRange(const QString& temp){
	QByteArray res;
	for (int j=0;j<temp.length();j++)
		if (temp[j] != '-' || j==0) res += temp[j];
		else {
			j++;
			while (res.at(res.length()-1) < temp[j])
				res += QChar(res[res.length()-1] + 1);
		}
	return res;
}

//removes all chars in charset from the char universe
QByteArray invertCharset(const QString& charset){
	QByteArray t = convertRange(ALL_CHARACTERS);
	foreach (const QChar& c, charset) t.replace(c,"");
	return t;
}

QByteArray decodeEscape(const QString& range, int& i){
	Q_ASSERT(range[i] == '\\');
	if (range[++i] == 'x') {
		QByteArray temp; 
		temp += hexCharacter(range[i+1], range[i+2]);
		i+=2;
		return temp;
	} else {
		Q_ASSERT(characterClasses.contains(range[i]));
		const QString& temp = characterClasses.value(range[i]); //nested character class
		return convertRange(temp);
	}
}

//creates a block for a [..] range
CharBlock createBlock(const QString& range, bool caseInsensitive){
	Q_ASSERT(range.length() > 0);
	CharBlock cb;
//	qDebug("%s", qPrintable(range));
	if (range.length()==1) {
		cb.type = CharBlock::CBT_FIXED;
		cb.slowData = range.toLatin1();
	} else {
		cb.type = CharBlock::CBT_CHOOSE;
		for (int i=(range.startsWith('^')?1:0);i<range.length();i++) {
			if (range[i] == '\\') cb.slowData += decodeEscape(range, i);
			else if (range[i] != '-' || i == 0) cb.slowData += charConv(range[i]);
			else {
				i++;float x=3.;
				unsigned char lookAhead = charConv(range[i]);
				QByteArray temp;
				if (lookAhead == '\\') {
					temp=decodeEscape(range, i);
					lookAhead = temp[0];
					temp.remove(0,1);
				}
				while ((unsigned char)(cb.slowData.at(cb.slowData.length()-1)) < lookAhead)
					cb.slowData += (cb.slowData[cb.slowData.length()-1] + 1);
				cb.slowData += temp;
			}
		}

		if (range.startsWith("^"))
			cb.slowData = invertCharset(cb.slowData);
	}
	if (caseInsensitive) {
		for (int i=0;i<cb.slowData.size();i++) {
			char c = cb.slowData[i];
			if (c >= 'a' && c <= 'z' && !cb.slowData.contains(c-'a'+'A')) cb.slowData += c - 'a'+'A';
			else if (c >= 'A' && c <= 'Z' && !cb.slowData.contains(c-'A'+'a')) cb.slowData += c - 'A'+'a';
		}
		if (cb.slowData.size()>1)
			cb.type = CharBlock::CBT_CHOOSE;
	}
	return cb;
}



//add all strings from lists[-1] to list[-2], repeated between minRep and maxRep times; reduces list.size by 2 and increase by maxRep-minRep+1
void multiplyLists(QList<BlockStringList>& lists, int minRep = 1, int maxRep = 1){
	if (minRep == 0 && maxRep == 0 || lists.size() == 1) return;

	const BlockStringList& repeat = lists.takeLast();
	BlockStringList old;
	old = lists.takeLast();
	BlockStringList nev;
	BlockStringList result;
	if (minRep == 0) result << old;
	for (int r = 1; r <= maxRep;r++) {
		nev.clear();
		for (int i=0;i<old.length();i++)
			for (int j=0;j<repeat.length();j++)
				nev.append(BlockString() << old[i] << repeat[j]);
		if (r >= minRep)
			result << nev;
		old = nev;
	}
	lists.append(result);
}

void concatLists(QList<BlockStringList>& lists, bool merged) {
	if (!merged) multiplyLists(lists);
	BlockStringList enums =  lists.takeLast();
	lists.last().append(enums);
}

/*
  Regex-Match Generator

  It processes a given regex in three steps:
  1. Expand all ? {} + * | operators
     This create a few new regex each matching strings of fixed length
  2. Expand all backtracking operators
     Afterwards each character in the string is either a link to another
     or a character set
  3. Expand all character sets

*/

int main(int argc, char* argv[])
{
	//============Parameters (above are some more)===========
	int INFINITY_PLUS = 5;
	int INFINITY_STAR = 5;

	bool expandOnly = false;
	int maxExpandLines = -1, maxLength = 0;
	bool truncateLonger = false;
	bool chooseRandomized = false;

	//character classes
	characterClasses.insert('d', "0-9");
	characterClasses.insert('w', "0-9a-zA-Z_");
	characterClasses.insert('s', " \t\n\r");
	//escapes
	characterClasses.insert('[', "[");
	characterClasses.insert(']', "]");
	characterClasses.insert('^', "^");
	characterClasses.insert('\\', "\\");

	QMap<QChar, QString> escapes = characterClasses;
	characterClasses.insert('-', "-");
	//escapes
	escapes.insert('{', "{");
	escapes.insert('}', "}");
	escapes.insert('(', "(");
	escapes.insert(')', ")");

	escapes.insert('+', "+");
	escapes.insert('*', "*");
	escapes.insert('?', "?");
	escapes.insert('.', ".");
	escapes.insert('$', "$");
	escapes.insert('.', ".");
	escapes.insert('|', "|");
	escapes.insert('n', "\n");
	escapes.insert('r', "\r");
	escapes.insert('t', "\t");
	escapes.insert('a', "\x07");
	escapes.insert('e', "\x1B");
	escapes.insert('f', "\x0C");
	escapes.insert('v', "\x0B");

	escapes.insert('b', ""); //word sepator (ignored)
	escapes.insert('B', ""); //word sepator (ignored)
	escapes.insert('A', ""); //marker (ignored)
	escapes.insert('Z', ""); //marker (ignored)
	escapes.insert('z', ""); //marker f(ignored)

	for (int i = 1; i<argc;i++) {
		QString arg = argv[i];
		if (arg.startsWith("--")) arg = arg.mid(1);
		if (arg == "-inf" || arg == "-infinity") {
			INFINITY_PLUS = QString(argv[++i]).toInt();
			INFINITY_STAR = QString(argv[i]).toInt();
		} else if (arg=="-all") {
			ALL_CHARACTERS = QString(argv[++i]);
		} else if (arg=="-words"){
			QString o = QString(argv[++i]);
			escapes.insert('w', o);
			characterClasses.insert('w', o);
		} else if (arg=="-digits"){
			QString o = QString(argv[++i]);
			escapes.insert('d', o);
			characterClasses.insert('d', o);
		} else if (arg=="-reduce-to-charsets") expandOnly = true;
		else if (arg=="-choose-randomized") chooseRandomized = true;
		else if (arg=="-max-expand-lines") maxExpandLines=QString(argv[++i]).toInt();
		else if (arg=="-progress") printProgress = true;
		else if (arg=="-max-length") maxLength = QString(argv[++i]).toInt();
		else if (arg=="-max-length-action") truncateLonger = QString(argv[++i]).toLower() == "truncate";
		else  {
			printf("RegEx-Solution-Generator\n");
			printf("This program generates to a list of simple regexs all strings matching these regex\n");
			printf("The syntax is pretty general, the allowed expressions are:\n");
			printf("  [character set]\n");
			printf("  {min, max}\n");
			printf("  ?     equivalent to {0,1}\n");
			printf("  +     equivalent to {1,INF} = {1,5}\n");
			printf("  *     equivalent to {0,INF} = {1,5}\n");
			printf("  .     equivalent to [ALL] = [0-9A-Za-z_+-...]\n");
			printf("  \\i    backtrack to match i\n");
			printf("  (?i)  Active case insensitiveness\n");
			printf("  (?-i) Disable case insensitiveness\n");
			printf("INF is defined as 5, because no program can generate an infinite number of strings.\n");
			printf("(actually it could print an infinite number of strings to stdout, but quantifiers are expanded before printing in the current implementation)\n");
			printf("^ and $ are ignored within the regex, but removed from start and end.\n");
			printf("If you use nested optional brackets like (()?)? you may get duplicated strings.\n");
			printf("Program options:\n");
			printf("  --inf \"replacement of infinity\"\n");
			printf("  --all \"character range of .)\n");
			printf("  --words \"character range of \\w\"\n");
			printf("  --digits \"character range of \\d\"\n");
			printf("  --reduce-to-charsets\tGenerate a set of regex limited to charsets that match the same strings as the original regex\n");
			printf("  --choose-randomized\tDon't print all possible matches in a systematic way, but choose random ones\n");
			printf("  --max-expand-lines \"max matches printed foreach charset regex\"\n");
			printf("  --progress\tPrint progress to stderr\n");
			printf("  --max-length \"characters\"\tMaximal length\n");
			printf("  --max-length-action \"ignore|truncate\"\tIgnore (def) or truncate longer words\n");

			return 0;
		}
	}

	characterClasses.insert('D', invertCharset(convertRange(characterClasses.value('d'))));
	characterClasses.insert('W', invertCharset(convertRange(characterClasses.value('w'))));
	characterClasses.insert('S', invertCharset(convertRange(characterClasses.value('s'))));
	escapes.insert('D', characterClasses.value('D'));
	escapes.insert('W', characterClasses.value('W'));
	escapes.insert('S', characterClasses.value('S'));


	//========================Actual RegExp Processing=======================
	QTextStream in(stdin);
	//in.setCodec(QTextCodec::codecForLocale());
#ifndef Q_WS_WIN32
	in.setCodec("utf-8");
#endif
	int loopCount = 0;
	try{
	while (true) {
		QString cur = in.readLine();
		if (cur.isNull()) break;
		//printf(">%s<ü\n",cur.toUtf8().data());
		//printf("%i:",cur.length());
		//for (int i=0;i<cur.length();i++)
		//	printf("%u-", (256+charConv(cur[i])*1) % 256);
		if (cur.startsWith("^")) cur = cur.mid(1);
		if (cur.endsWith("$")) cur = cur.left(cur.size()-1);

		bool merged = true;
		bool caseInsensitive = false;
		int nestingLevel = 0;
		QList<int> nestedBrackets;
		loopCount++;
		if (printProgress) fprintf(stderr, "Processing regex %i: %s\n", loopCount, qPrintable(cur));
		//Stack of (block-)stringlists
		//Each stringlist contains all possible block match for a subterm of the regex (brackets or enums)
		//Everything except charsets is expanded
		//Lists can be added => Append all strings of list[-2] to list[-1]
		//And multiplied => Replace list[-2] by all concatenated pairs of list[-1] and list[-2]
		//Only | causes addition, everything else causes multiplication
		//The stack will look like this: (+list) (*list) (+list) (*list) ... (+list) (*list) (temporary list)
		QList< BlockStringList > lists;
		lists << (BlockStringList()) << (BlockStringList() << BlockString());

		for (int i=0;i<cur.length();i++){
			switch (cur[i].toAscii()) {
			case '[':{
				if (!merged) multiplyLists(lists);

				int n = i;
				for (; cur[n] != ']'; n++) if (cur[n] == '\\') n++;
				QString sub = cur.mid(i+1,n-1-i);
				i = n;

				lists.append(BlockStringList() << (BlockString() << createBlock(sub, caseInsensitive )));
				merged = false;
				break;
			}
			case '\\':{
				if (!merged) multiplyLists(lists);
				merged = true;
				QString sub;
				i++;
				if (cur[i].toAscii() == 'x') {
					sub = hexCharacter(cur[i+1], cur[i+2]);
					i+=2;
				} else if (cur[i] >= '0' && cur[i] <= '9'){
					int bt = cur[i].toLatin1() - '0';
					if (i+1 < cur.length() && cur[i+1] >= '0' && cur[i+1] <= '9'){
						bt*=10;
						bt += cur[i+1].toLatin1() - '0';
						i++;
					}
					CharBlock cb;
					cb.type = CharBlock::CBT_BACKTRACK;
					cb.backtrack = bt;
					lists.append(BlockStringList() << (BlockString() << cb));
					merged = false;
					break;
				} else if (cur[i] == 'Q') { //  \Q ... \E literal quotation
					BlockString temp;
					for (i++; cur[i] != '\\' || cur[i+1] != 'E'; i++)
						temp << createBlock(""+cur[i], caseInsensitive);
					i++;
					lists.append(BlockStringList() << temp);
					merged=false;
					break;
				} else {
					Q_ASSERT(escapes.contains(cur[i]));
					sub =  escapes.value(cur[i]);
					if (sub.isEmpty())
						break;
				}
				lists.append(BlockStringList() << (BlockString() << createBlock(sub,caseInsensitive)));
				merged = false;
				break;
			}
			case '.':
				if (!merged) multiplyLists(lists);
				lists.append(BlockStringList() << (BlockString() << createBlock(ALL_CHARACTERS, false)));
				merged = false;
				break;
			case '?':
				if (merged) break; //ignore lazy
				//Q_ASSERT(!merged); //don't be lazy
				multiplyLists(lists, 0, 1);
				merged = true;
				break;
			case '{': {
				int n = cur.indexOf('}',i);
				QString sub = cur.mid(i+1,n-1-i);
				i = n;

				QStringList temp = sub.split(',');
				Q_ASSERT(temp.size()==2 || temp.size()==1);
				multiplyLists(lists, temp.first().toInt(), temp.last().isEmpty()?INFINITY_STAR:temp.last().toInt());
				merged = true;
				break;
			}
			case '+':
				Q_ASSERT(!merged);
				multiplyLists(lists, 1, INFINITY_PLUS);
				merged = true;
				break;
			case '*':
				Q_ASSERT(!merged);
				multiplyLists(lists, 0, INFINITY_STAR);
				merged = true;
				break;
			case '(': if (cur[i+1] == '?') {
					Q_ASSERT(i + 3< cur.length());
					//special command
					i+=2;
					if (cur[i] == '#') ; //comment
					else if (cur[i] == 'i') caseInsensitive = true;
					else if (cur[i] == '-' && cur[i+1] == 'i') caseInsensitive = false;
					else throw "Unknown special (? bracket command";
					while (cur[i]!=')') i++;
				} else {
					if (!merged) multiplyLists(lists);
					nestingLevel++;
					CharBlock cb;
					cb.type = CharBlock::CBT_BACKTRACK_START;
					cb.backtrack = nestingLevel;
					lists.append(BlockStringList());
					lists.append(BlockStringList() << (BlockString() << cb));
					nestedBrackets << nestingLevel;
					merged = true;
				}
				break;
			case ')': {
				if (!merged) multiplyLists(lists);
				concatLists(lists,true);
				CharBlock cb;
				cb.type = CharBlock::CBT_BACKTRACK_END;
				cb.backtrack = nestedBrackets.takeLast();
				for (int i=0;i<lists.last().size();i++)
					lists.last()[i].append(cb);
				merged = false;
				break;
			}
			case '|':{
				concatLists(lists, merged);
				if (!nestedBrackets.isEmpty()) {
					CharBlock cb;
					cb.type = CharBlock::CBT_BACKTRACK_END;
					cb.backtrack = nestedBrackets.last();
					for (int i=0;i<lists.last().size();i++)
						lists.last()[i].append(cb);
					cb.type = CharBlock::CBT_BACKTRACK_START;
					lists.append(BlockStringList() << (BlockString() << cb));
				} else lists.append(BlockStringList() << BlockString());
				merged = true;
				break;
			//case ']': case '}': Q_ASSERT(false); return;
			} 
			default:
				if (!merged) multiplyLists(lists);
				lists.append(BlockStringList() << (BlockString() << createBlock(""+cur[i],caseInsensitive)));
				merged = false;
			}
		}

		concatLists(lists,merged);

		QList<BlockString> rbs;
		int64_t totalPos = 0, curPos = 0;
		for (int i=0;i<lists.first().length();i++) {
			BlockString bs = purifyBacktracking(lists.first()[i]);
			if (maxLength > 0 && bs.size() > maxLength) {
				if (!truncateLonger) continue;
				else bs.erase(bs.begin() + maxLength, bs.end());
			}
			rbs << bs;
			totalPos += countPossibilities(bs);
		}
		for (int i=0;i<rbs.size();i++) {
			BlockString& bs = rbs[i];
			if (!expandOnly) {
				if (printProgress) {
					fprintf(stderr, "    Printing subregex %i/%i of %i:%s\n",  i+1, lists.first().length(), loopCount, qPrintable(cur), curPos, totalPos);
					printExpanded(bs,stderr);
				}
				printPossibilities(bs,chooseRandomized,maxExpandLines,curPos, totalPos);
			} else {
				printExpanded(bs);
			}
			curPos += countPossibilities(bs);
		}
		Q_ASSERT(lists.size()==1);
	}
	} catch (const char* err)  {
		fprintf(stderr, "Error: %s\n\n", err);
	}
	return 0;
}


