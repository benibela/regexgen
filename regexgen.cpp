#include <QTextStream>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QTextCodec>
#include <stdio.h>
#include <stdlib.h>
QMap<QChar, QString> characterClasses;
QString ALL_CHARACTERS = "0-9A-Za-z_+-*/=<>()[]{}!.,;:$%\"'#~";
bool printProgress = false;

struct CharBlock {
	//set on init
	enum Type {CBT_FIXED, CBT_CHOOSE};
	Type type;
	QByteArray slowData;

	//set when evaluating
	const char * data;
	int len;
	int choosen;
	int pos;

};

int randUntil(int until){
	return random() % until;
}

char charConv(const QChar& c){
	return c.toAscii();
}

//takes a simplified like (\[.*\]|.)* and prints all possibilities to choose characters in the character sets
//(this will generate thousand-millions of equal-length words)
void printPossibilities(QList<CharBlock>& blocks, bool randomized, int maxLines){
	char word[blocks.length()+1];
	CharBlock vars[blocks.size()+1];
	int actualBlockCount = 0;
	long int totalPos = 1;
	for (int i=0;i<blocks.size();i++){
		blocks[i].pos = i;
		blocks[i].choosen = 0;
		blocks[i].len = blocks[i].slowData.length();
		blocks[i].data = blocks[i].slowData.data(); //QByteArray::data keeps the data in memory (in contrast to QString::toAscii and qPrintable which DON'T WORK HERE)
		word[i] = *blocks[i].data;
		totalPos*=blocks[i].len;
		Q_ASSERT(blocks[i].len > 0);
		if (blocks[i].type == CharBlock::CBT_FIXED) Q_ASSERT(blocks[i].len==1);
		else vars[actualBlockCount++] = blocks[i];
	}
//	for (int i=0;i<actualBlockCount;i++)
//		printf(">%s<\n",vars[i].data);
	word[blocks.length()]=0;
	if (actualBlockCount==0) {printf("%s\n", word); return;}

	if (maxLines > 0) totalPos = maxLines;
	int progressNext = printProgress?totalPos / 10:totalPos;

	if (!randomized) {
		int r = 0;
		while (true) {
			int i=0;
			for (;i<actualBlockCount && vars[i].choosen == vars[i].len;  i++) {
				vars[i].choosen = 0;
				word[vars[i].pos] = *vars[i].data;
				vars[i+1].choosen++;
			}
			if (i<actualBlockCount) {
				word[vars[i].pos] = vars[i].data[vars[i].choosen];
				printf("%s\n", word);
				r++;
				if (r>=progressNext) {
					if (printProgress) {
						fprintf(stderr, "      Progress: %i/%li (%i%%)\n", r, totalPos, ((long int)(r)*100)/totalPos);
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
			printf("%s\n", word);
		}
	}
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

//creates a block for a [..] range
CharBlock createBlock(const QString& range){
	Q_ASSERT(range.length() > 0);
	CharBlock cb;
//	qDebug("%s", qPrintable(range));
	if (range.length()==1) {
		cb.type = CharBlock::CBT_FIXED;
		cb.slowData = range.toLatin1();
	} else {
		cb.type = CharBlock::CBT_CHOOSE;
		for (int i=(range.startsWith('^')?1:0);i<range.length();i++) {
			if (range[i] == '\\') {
				if (range[++i] == 'x') {
					cb.slowData += '\0' + 16*(range[i+1].toLatin1()-'0') + (range[i+2].toLatin1()-'0');
					Q_ASSERT(range[i+1].toLatin1() >= '0' && range[i+1].toLatin1() <= '9');
					Q_ASSERT(range[i+2].toLatin1() >= '0' && range[i+2].toLatin1() <= '9');
					i+=2;
				} else {
					Q_ASSERT(characterClasses.contains(range[i]));
					const QString& temp = characterClasses.value(range[i]); //nested character class
					cb.slowData += convertRange(temp);
				}
			} else if (range[i] != '-' || i == 0) cb.slowData += charConv(range[i]);
			else {
				i++;
				while (cb.slowData.at(cb.slowData.length()-1) < charConv(range[i]))
					cb.slowData += (cb.slowData[cb.slowData.length()-1] + 1);
			}
		}

		if (range.startsWith("^"))
			cb.slowData = invertCharset(cb.slowData);
	}
	return cb;
}

typedef QList<CharBlock> BlockString;
typedef QList<BlockString> BlockStringList;


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


int main(int argc, char* argv[])
{
	//============Parameters (above are some more)===========
	int INFINITY_PLUS = 5;
	int INFINITY_STAR = 5;

	bool specialCaseInsensitiveOperator = true;
	bool expandOnly = false;
	int maxExpandLines = -1;
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
	if (specialCaseInsensitiveOperator) {
		escapes.insert('<', "<");
		escapes.insert('>', ">");
	}
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
		} else if (arg=="-case-insensitiveness"){
			QString o = QString(argv[++i]);
			if (o == "local") specialCaseInsensitiveOperator = true;
			else if (o == "disable") specialCaseInsensitiveOperator = false;
			else Q_ASSERT(false);
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
		else  {
			printf("RegEx-Solution-Generator\n");
			printf("This program generates to a list of simple regexs all strings matching these regex\n");
			printf("The syntax is pretty general, the allowed expressions are:\n");
			printf("  [character set]\n");
			printf("  {min, max}\n");
			printf("  ?   equivalent to {0,1}\n");
			printf("  +   equivalent to {1,INF} = {1,5}\n");
			printf("  *   equivalent to {0,INF} = {1,5}\n");
			printf("  .   equivalent to [ALL] = [0-9A-Za-z_+-...]\n");
			printf("  <case ignore>  this is not standard regex, but <xyz> will match XYZ case insensitive.\n");
			printf("INF is defined as 5, because no program can generate an infinite number of strings.\n");
			printf("(actually it could print an infinite number of strings to stdout, but quantifiers are expanded before printing in the current implementation)\n");
			printf("^ and $ are ignored within the regex, but removed from start and end.\n");
			printf("If you use nested optional brackets like (()?)? you may get duplicated strings.\n");
			printf("Program options:\n");
			printf("  --inf \"replacement of infinity\"\n");
			printf("  --all \"character range of .)\n");
			printf("  --case-insensitiveness \"local|disabled\"\tdisable <..>\n");
			printf("  --words \"character range of \\w\"\n");
			printf("  --digits \"character range of \\d\"\n");
			printf("  --reduce-to-charsets\tGenerate a set of regex limited to charsets that match the same strings as the original regex\n");
			printf("  --choose-randomized\tDon't print all possible matches in a systematic way, but choose random ones\n");
			printf("  --max-expand-lines \"max matches printed foreach charset regex\"\n");
			printf("  --progress\tPrint progress to stderr\n");
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

				lists.append(BlockStringList() << (BlockString() << createBlock(sub)));
				merged = false;
				break;
			}
			case '\\':{
				QString sub = escapes.value(cur[++i]);
				if (cur[i].toAscii() == 'x') {
					sub = '\0' + 16*(cur[i+1].toLatin1()-'0') + (cur[i+2].toLatin1()-'0');
					Q_ASSERT(cur[i+1].toLatin1() >= '0' && cur[i+1].toLatin1() <= '9');
					Q_ASSERT(cur[i+2].toLatin1() >= '0' && cur[i+2].toLatin1() <= '9');
					i+=2;
				} else if (cur[i] == 'Q') { //  \Q ... \E literal quotation
					if (!merged) multiplyLists(lists);
					BlockString temp;
					for (i++; cur[i] != '\\' || cur[i+1] != 'E'; i++)
						temp << createBlock(""+cur[i]);
					i++;
					lists.append(BlockStringList() << temp);
					merged=false;
					break;
				} else Q_ASSERT(escapes.contains(cur[i]));
				if (!merged) multiplyLists(lists);
				if (sub!="") {
					lists.append(BlockStringList() << (BlockString() << createBlock(sub)));
					merged = false;
				} else merged = true;
				break;
			}
			case '.':
				if (!merged) multiplyLists(lists);
				lists.append(BlockStringList() << (BlockString() << createBlock(ALL_CHARACTERS)));
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
			case '(':
				if (!merged) multiplyLists(lists);
				lists.append(BlockStringList());
				lists.append(BlockStringList() << BlockString());
				merged = true;
				break;
			case ')': {
				concatLists(lists,merged);
				merged = false;
				break;
			}
			case '|':{
				concatLists(lists, merged);
				lists.append(BlockStringList() << BlockString());
				merged = true;
				break;
			//case ']': case '}': Q_ASSERT(false); return;
			} 
			case '>': Q_ASSERT(!specialCaseInsensitiveOperator); if (specialCaseInsensitiveOperator) return -1; //FALL THROUGH
			case '<': if (specialCaseInsensitiveOperator) { //FALL THROUGH
				if (!merged) multiplyLists(lists);

				int n = cur.indexOf('>',i);
				QString sub = cur.mid(i+1,n-1-i);
				i = n;

				BlockString cbs;
				for (int i=0;i<sub.length();i++)
					cbs << createBlock(QString(sub[i].toLower())+QString(sub[i].toUpper()));
				lists.append(BlockStringList() << cbs);
				merged = false;
				break;
			}
			default:
				if (!merged) multiplyLists(lists);
				lists.append(BlockStringList() << (BlockString() << createBlock(""+cur[i])));
				merged = false;
			}
		}

		concatLists(lists,merged);

		if (expandOnly) {
			for (int i=0;i<lists.first().length();i++) {
				const BlockString& bs = lists.first()[i];
				foreach (const CharBlock& cb, bs)
					if (cb.type == CharBlock::CBT_FIXED)
						printf("%c", cb.slowData[0]);
					else if (cb.type == CharBlock::CBT_CHOOSE)
						printf("[%s]", qPrintable(QString(cb.slowData).replace('\\', "\\\\").replace('[', "\\[").replace(']', "\\]")));
					else
						break;
				printf("\n");
			}
		} else for (int i=0;i<lists.first().length();i++) {
			if (printProgress) fprintf(stderr, "    Printing subregex %i/%i of %i:%s\n",  i+1, lists.first().length(), loopCount, qPrintable(cur));
			printPossibilities(lists.first()[i],chooseRandomized,maxExpandLines);
		}
		Q_ASSERT(lists.size()==1);
	}
	return 0;
}


