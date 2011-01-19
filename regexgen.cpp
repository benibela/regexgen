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

void addToLists(QList< QList<CharBlock> > & lists, const QString& range){
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

	for (int i=0;i<lists.length();i++)
		lists[i].append(cb);
}

void multiplyLists(QList<QList<CharBlock> >& lists, int minRep, int maxRep){
	QList<QList<CharBlock> > old = lists;
	CharBlock last = old[0].last();
	QList<CharBlock> add;
	for (int r = 0; r < minRep; r++) add << last;
	for (int i=0;i<old.length();i++)
		old[i].removeLast();
	lists.clear();
	for (int r = minRep; r<=maxRep; r++) {
		for (int i=0;i<old.length();i++){
			lists.append(old[i]);
			lists.last().append(add);
		}
		add << last;
	}
}

int main(int argc, char* argv[])
{
	if (argc >= 2){
		printf("RegEx-Solution-Generator\n");
		printf("This program generates to a list of simple regexs all strings matching these regex\n");
		printf("The syntax is very limited, the only allowed expressions are:\n");
		printf("  [character set]\n");
		printf("  {min, max}\n");
		printf("  ?   equivalent to {0,1}\n");
		printf("  <case ignore>  this is not standard regex, but <xyz> will match XYZ case insensitive\n");
		printf("Brackets and escape characters are currently not allowed and of course infinite operators like * + also not\n");
		return;
	}

	QTextStream in(stdin);
	while (!in.atEnd()) {
		QString cur = in.readLine();

		QList< QList<CharBlock> > lists;
		lists << QList<CharBlock>();
		for (int i=0;i<cur.length();i++){
			switch (cur[i].toAscii()) {
			case '[':{
				int n = cur.indexOf(']',i);
				QString sub = cur.mid(i+1,n-1-i);
				i = n;

				addToLists(lists, sub);
				break;
			}
			case '?':
				multiplyLists(lists, 0, 1);
				break;
			case '{': {
				int n = cur.indexOf('}',i);
				QString sub = cur.mid(i+1,n-1-i);
				i = n;

				QStringList temp = sub.split(',');
				Q_ASSERT(temp.size()==2);
				multiplyLists(lists, temp[0].toInt(), temp[1].toInt());
				break;
			}
			case '<': {
				int n = cur.indexOf('>',i);
				QString sub = cur.mid(i+1,n-1-i);
				i = n;

				for (int i=0;i<sub.length();i++)
					addToLists(lists, QString(sub[i].toLower())+QString(sub[i].toUpper()));
				break;
			}
			case ']': case '}': case '>': Q_ASSERT(false);
			default:
				addToLists(lists, ""+cur[i]);
			}
		}

		for (int i=0;i<lists.length();i++)
			printPossibilities(lists[i]);
	}
	return 0;
}


