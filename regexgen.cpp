#include <QTextStream>
#include <QString>
#include <QStringList>
#include <stdio.h>

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

void printPossibilities(QList<CharBlock>& blocks){
	char word[blocks.length()+1];
	CharBlock vars[blocks.size()+1];
	int actualBlockCount = 0;
	for (int i=0;i<blocks.size();i++){
		blocks[i].pos = i;
		blocks[i].choosen = 0;
		blocks[i].len = blocks[i].slowData.length();
		blocks[i].data = blocks[i].slowData.data(); //QByteArray::data keeps the data in memory (in contrast to QString::toAscii and qPrintable which DON'T WORK HERE)
		word[i] = *blocks[i].data;
		if (blocks[i].type == CharBlock::CBT_FIXED) Q_ASSERT(blocks[i].len==1);
		else vars[actualBlockCount++] = blocks[i];
	}
//	for (int i=0;i<actualBlockCount;i++)
//		printf(">%s<\n",vars[i].data);
	word[blocks.length()]=0;
	if (actualBlockCount==0) {printf("%s\n", word); return;}
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
		} else {
//			printf("%s\n", word);
			break;
		}
		vars[0].choosen+=1;
		word[vars[0].pos] = vars[0].data[vars[0].choosen];
	}
}

CharBlock createBlock(const QString& range){
	CharBlock cb;
//	qDebug("%s", qPrintable(range));
	cb.type = range.length()<=1?CharBlock::CBT_FIXED:CharBlock::CBT_CHOOSE;
	cb.len = 0;
	for (int i=0;i<range.length();i++) {
		if (range[i] != '-' || i == 0) cb.slowData += range[i];
		else {
			i++;
			while (cb.slowData.at(cb.slowData.length()-1) < range[i])
				cb.slowData += QChar(cb.slowData[cb.slowData.length()-1] + 1);
		}
	}

	return cb;
}

typedef QList<CharBlock> BlockString;
typedef QList<BlockString> BlockStringList;

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

int main(int argc, char* argv[])
{
	const int INFINITY_PLUS = 5;
	const int INFINITY_STAR = 5;
	if (argc >= 2){
		printf("RegEx-Solution-Generator\n");
		printf("This program generates to a list of simple regexs all strings matching these regex\n");
		printf("The syntax is pretty general, the only allowed expressions are:\n");
		printf("  [character set]\n");
		printf("  {min, max}\n");
		printf("  ?   equivalent to {0,1}\n");
		printf("  +   equivalent to {1,INF}\n");
		printf("  *   equivalent to {0,INF}\n");
		printf("  <case ignore>  this is not standard regex, but <xyz> will match XYZ case insensitive.\n");
		printf("INF is defined as 5, because no program can generate an infinite number of strings.\n");
		printf("(actually it could print an infinite number of strings to stdout, but quantifiers are expanded before printing)\n");
		printf("If you use nested optional brackets like (()?)? you may get duplicated strings.\n");
		return 0;
	}

	QTextStream in(stdin);
	bool merged = false;
	while (!in.atEnd()) {
		QString cur = in.readLine();

		QList< BlockStringList > lists;
		lists << (BlockStringList() << BlockString());
		for (int i=0;i<cur.length();i++){
			switch (cur[i].toAscii()) {
			case '[':{
				if (!merged) multiplyLists(lists);

				int n = cur.indexOf(']',i);
				QString sub = cur.mid(i+1,n-1-i);
				i = n;

				lists.append(BlockStringList() << (BlockString() << createBlock(sub)));
				merged = false;
				break;
			}
			case '?':
				multiplyLists(lists, 0, 1);
				merged = true;
				break;
			case '{': {
				int n = cur.indexOf('}',i);
				QString sub = cur.mid(i+1,n-1-i);
				i = n;

				QStringList temp = sub.split(',');
				Q_ASSERT(temp.size()==2 || temp.size()==1);
				multiplyLists(lists, temp.first().toInt(), temp.last().toInt());
				merged = true;
				break;
			}
			case '<': {
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
			case '+': multiplyLists(lists, 1, INFINITY_PLUS); break;
			case '*': multiplyLists(lists, 0, INFINITY_STAR); break;
			case '(':
				if (!merged) multiplyLists(lists);
				lists.append(BlockStringList() << BlockString());
				merged = true;
				break;
			case ')':
				if (!merged) multiplyLists(lists);
				merged = false;
				break;
			case ']': case '}': case '>': Q_ASSERT(false);
			default:
				if (!merged) multiplyLists(lists);
				lists.append(BlockStringList() << (BlockString() << createBlock(""+cur[i])));
				merged = false;
			}
		}

		multiplyLists(lists);

		for (int i=0;i<lists.first().length();i++)
			printPossibilities(lists.first()[i]);

		Q_ASSERT(lists.size()==1);
	}
	return 0;
}


