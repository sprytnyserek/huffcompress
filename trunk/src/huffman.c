#include<stdio.h>
#include<stdlib.h>
#include<memory.h>
#include<limits.h>
#include<string.h>

/* makra do obslugi przemieszczen w kolejce priorytetowej jako stercie */
#define ParVert(nr) (((nr-1)/2))/* numer ojca wezla o numerze nr */
#define LeftChild(nr) (((2*nr)+1))/* numer lewego dziecka wezla o numerze nr */
#define RightChild(nr) (((2*nr)+2))/* numer prawego dziecka wezla o numerze nr */

/* definicja typu struktury wezla drzewa Huffmanna */
typedef struct _TreeVert {
               int chr; /* opisany znak, pole wazne tylko wowczas, gdy sig==1 */
			   unsigned long int freq; /* czestosc znaku chr w buforze */
			   char sig;/*informuje o zastosowaniu wierzcholka jako pola znakowego(liscia)*/
			   /* sig==1 => wezel jest lisciem ; sig!=1 => wezel wewnetrzny albo korzen */
			   char weight;
			   /* weight==0 => po lewej stronie ojca ; weight==1 => po prawej stronie ojca
			    * weight==2 => polozenie nieokreslone*/
	           struct _TreeVert *left,*right,*parent; /* wskazniki do dzieci */
               } TreeVert;

/* definicja typu struktury pola tablicy czestosci */
typedef struct _Freq {
               int chr;
			   unsigned long int freq;
			   TreeVert *address;
               } Freq;

/* definicja typu struktury tablicy czestosci */
typedef struct _FreqMat {
               Freq *head;
			   unsigned int size,z;
               } FreqMat;

/* definicja typu struktury kolejki priorytetowej uzywanej w procesie budowania drzewa
 * Huffmanna; kolejka wewnetrznie jest tablica wskaznikow do korzeni drzew skladowych;
 * utrzymuje zbior czesciowo uporzadkowany w relacji <= wzgledem czestosci */
typedef struct _PriorQueue {
               TreeVert **head;
	           unsigned int size;
               } PriorQueue;

typedef struct _BstElmt {
               Freq *chr;
			   struct _BstElmt *left,*right;
               } BstElmt;

typedef struct _Bst {
               BstElmt *head;
			   unsigned int size;
               } Bst;
			   
typedef struct _CodeTableVert {
               int chr;
			   unsigned short int code;
			   unsigned int length;
               } CodeTableVert;
			   
typedef struct _CodeTable {
               CodeTableVert *head;
			   unsigned int size;
               } CodeTable;

void ShowUsing(char *command,char err) {
if (err) fprintf(stderr,"Sposob uzycia: %s plik_zrodlowy [-cu] plik_docelowy\n",command);
		 else printf("Sposob uzycia: %s plik_zrodlowy [-cu] plik_docelowy\n",command);
return;
}

int error(int id,char *name,char *command) {
switch (id) {
      case 1: fprintf(stderr,"Plik %s nie istnieje\n",name);
	  		  ShowUsing(command,1);
			  return id;
      case 2: fprintf(stderr,"Blad alokacji pamieci\n");
              return id;
	  case 3: fprintf(stderr,"Za malo parametrow\n");
	  		  ShowUsing(command,1);
			  return id;
	  case 4: fprintf(stderr,"Za duzo parametrow\n");
	  		  ShowUsing(command,1);
			  return id;
	  case 5: fprintf(stderr,"Bledne parametry\n");
	  		  ShowUsing(command,1);
			  return id;
          case 6: fprintf(stderr,"Nazwy plikow zrodlowego i docelowego musza byc rozne\n");
                  ShowUsing(command,1);
                  return id;
          case 7: fprintf(stderr,"Plik %s jest pusty\n",name);
                  ShowUsing(command,1);
                  return id;
      default:ShowUsing(command,0);
              return id;
      }
}

void BstInit(Bst *bst) {
bst->head=NULL;
bst->size=0;
return;
}

int BstVertDestroy(BstElmt *head) {
if (head==NULL) return -1;
if (head->left!=NULL) BstVertDestroy(head->left);
if (head->right!=NULL) BstVertDestroy(head->right); else free(head);
return 0;
}

int BstDestroy(Bst *bst) {
if (BstVertDestroy(bst->head)==-1) return -1;
memset(bst,0,sizeof(Bst));
return 0;
}

int AddToBst(Bst *bst,Freq *chr) {
BstElmt *newchr,*curr,*next;
if (bst->head==NULL) {
	bst->head=(BstElmt*)malloc(sizeof(BstElmt));
	bst->head->chr=chr;
	bst->head->left=bst->head->right=NULL;
	}
else {
	newchr=(BstElmt*)malloc(sizeof(BstElmt));
	newchr->chr=chr;
	newchr->left=newchr->right=NULL;
	curr=next=bst->head;
	while (next!=NULL) {
		if (curr!=next) curr=next;
		if (newchr->chr->chr<curr->chr->chr) next=curr->left; else next=curr->right;
		}
	if (newchr->chr->chr<curr->chr->chr) curr->left=newchr; else curr->right=newchr;
	}
bst->size++;
return 0;
}

Freq *BstSearch(Bst *bst,int chr) {
BstElmt *pnt=bst->head; /* punkt startowy */
while ((pnt!=NULL)&&(pnt->chr->chr!=chr)) {
	if (chr<pnt->chr->chr) pnt=pnt->left; else pnt=pnt->right;
	}
if (pnt==NULL) return NULL; else return pnt->chr;
}

/* inicjuje kolejke priorytetowa pqueue */
void PriorQueueInit(PriorQueue *pqueue) {
pqueue->head=NULL;
pqueue->size=0;
return;
} /* PriorQueueInit */

/* czysci kolejke priorytetowa pqueue (zawierajaca tylko wskazniki!) */
void PriorQueueDestroy(PriorQueue *pqueue) {
free(pqueue->head);
memset(pqueue,0,sizeof(TreeVert*));
return;
} /* PriorQueueDestroy */

/* dodaje wierzcholek drzewa (byc moze zawierajacy adresy dzieci)
 * do kolejki priorytetowej pqueue */
int AddTreeVertToQueue(PriorQueue *pqueue,TreeVert *vert) {
TreeVert **head=pqueue->head; /* tymczasowe przypisanie dla skrocenia kodu */
unsigned int curr,size=curr=pqueue->size; /* j.w. + zmienna pomocnicza curr(ent vertex) */
TreeVert *wsk;
head=(TreeVert**)realloc(head,(size+1)*sizeof(TreeVert*));
if (head==NULL) return -1;
head[size]=vert;
while ((curr>0)&&(head[curr]->freq<head[ParVert(curr)]->freq)) { /* ustawianie sterty */
	wsk=head[curr];
	head[curr]=head[ParVert(curr)];
	curr=ParVert(curr);
	head[curr]=wsk;
	}
pqueue->head=head; /* przywrocenie wlasciwej wartosci glowy sterty */
pqueue->size++;
return 0;
} /* AddTreeVertToQueue */

/* usuwa drzewo o korzeniu w adresie treehead wraz z danymi */
void TreeDestroy(TreeVert *treehead) {
if (treehead->left!=NULL) TreeDestroy(treehead->left);
if (treehead->right!=NULL) TreeDestroy(treehead->right); else free(treehead);
return;
} /* TreeDestroy */

/* usuwa wskaznik o najnizszym wartosciowo priorytecie z kolejki pqueue */
int TreeReleaseFromQueue(PriorQueue *pqueue) {
unsigned int size=pqueue->size;
unsigned int curr=0,chg;
TreeVert *chgpnt;
if (size==0) return -1;
if (size==1) {
	(pqueue->head)[0]=NULL;
	pqueue->head=(TreeVert**)realloc(pqueue->head,0);
	}
if (size>1) {
	(pqueue->head)[0]=(pqueue->head)[size-1];
	(pqueue->head)[size-1]=NULL;
	pqueue->head=(TreeVert**)realloc(pqueue->head,(--size)*sizeof(TreeVert*));
	while (((LeftChild(curr)<size)&&((pqueue->head)[curr]->freq>(pqueue->head)[LeftChild(curr)]->freq))||((RightChild(curr)<size)&&((pqueue->head)[curr]->freq>(pqueue->head)[LeftChild(curr)]->freq))) {
		/* NIE MOZE WYSTAPIC TAKA SYTUACJA, ABY ISTNIALO PRAWE DZIECKO, A NIE ISTNIALO LEWE */
        if (RightChild(curr)<size) {
			if ((pqueue->head)[LeftChild(curr)]->freq<(pqueue->head)[RightChild(curr)]->freq) {
				chg=LeftChild(curr);
				}
			else {
				chg=RightChild(curr);
				}
			} /* istnienie prawego dziecka */
		else {
			chg=LeftChild(curr);
			}
		chgpnt=(pqueue->head)[curr];
		(pqueue->head)[curr]=(pqueue->head)[chg];
		curr=chg;
		(pqueue->head)[curr]=chgpnt;
		}
	}
pqueue->size--;
return 0;
} /* TreeReleaseFromQueue */

void SetWeight(TreeVert *root) {
if (root==NULL) return;
if (root->left!=NULL) root->left->weight=0;
if (root->right!=NULL) root->right->weight=1;
SetWeight(root->left);
SetWeight(root->right);
return;
}

/* buduje drzewo Huffmanna na podstawie tablicy czestosci mat */
TreeVert *BuildTree(FreqMat *mat) {
unsigned int size=mat->size;
Freq *pnt=mat->head;
TreeVert *pnt1,*pnt2,*newv;
unsigned int i,counter=mat->size;
PriorQueue *pqueue=(PriorQueue*)malloc(sizeof(PriorQueue));
if (pqueue==NULL) return NULL;
PriorQueueInit(pqueue);
for (i=0;i<size;i++) {
	newv=(TreeVert*)malloc(sizeof(TreeVert));
	if (newv==NULL) return NULL;
	newv->chr=pnt[i].chr;
	newv->freq=pnt[i].freq;
	newv->sig=1;
	newv->left=newv->right=newv->parent=NULL;
	pnt[i].address=newv;
	if (AddTreeVertToQueue(pqueue,newv)==-1) return NULL;
	}
while ((pqueue->size)>1) {
	pnt1=*(pqueue->head); /* pierwsze drzewo skladowe */
	TreeReleaseFromQueue(pqueue);
	pnt2=*(pqueue->head); /* drugie drzewo skladowe */
	TreeReleaseFromQueue(pqueue);
	newv=(TreeVert*)malloc(sizeof(TreeVert)); /* miejsce na wezel laczacy */
	if (newv==NULL) return NULL;
	newv->chr=0;
	newv->freq=(pnt1->freq)+(pnt2->freq);
	newv->sig=0;
	newv->left=pnt1;
	newv->right=pnt2;
	pnt1->parent=newv;
	pnt2->parent=newv;
	counter++;
	if (AddTreeVertToQueue(pqueue,newv)==-1) return NULL;
	}
newv=*(pqueue->head);
PriorQueueDestroy(pqueue);
SetWeight(newv);
return newv; /* <<<--- tu bedzie jakis adres */
} /* BuildTree */

/* nie dziala, nie uzywac */
Freq *GetMax(Bst *bst) {
BstElmt *head=bst->head;
if (head==NULL) return NULL;
while (head->right!=NULL) {
	  head=head->right;
	  }
return head->chr;
}

/* ustawia wskazniki tablicy mat na elementy drzewa BST o korzeniu w head i usuwa drzewo */
void BstConvert(BstElmt *head,FreqMat *mat) {
BstElmt *pnt1,*pnt2;
if (head==NULL) return;
pnt1=head->left;
pnt2=head->right;
if (head->chr->chr==EOF) {
   free(head);
   BstConvert(pnt1,mat);
   BstConvert(pnt2,mat);
   }
   else {
   mat->head[mat->z].chr=head->chr->chr;
   mat->head[mat->z++].freq=head->chr->freq;
   free(head);
   BstConvert(pnt1,mat);
   BstConvert(pnt2,mat);
   }
return;
}

/* tworzy tablice czestosci znakow z pliku file, zwraca wskaznik do tej tablicy */
FreqMat *BuildMat(FILE *file) {
FreqMat *mat=(FreqMat*)malloc(sizeof(FreqMat));
Bst bst;
Freq *newchar;
int c;
unsigned long int counter=0;
BstInit(&bst);
rewind(file);
while (feof(file)==0) {
	c=getc(file);
	newchar=BstSearch(&bst,c);
	if (newchar==NULL) {/* jezeli znaku jeszcze ni ma w drzewie, utworz dla niego miejsce */
		newchar=(Freq*)malloc(sizeof(Freq));
		newchar->chr=c;
		newchar->freq=1;
		newchar->address=NULL;
		AddToBst(&bst,newchar);
		}
	else {
		newchar->freq++;
		}
	}
mat->size=bst.size-1;
mat->head=(Freq*)calloc(mat->size,sizeof(Freq));
mat->z=0;
BstConvert(bst.head,mat);
memset(&bst,0,sizeof(bst));
return mat;
}

void CorrectMat(FreqMat *mat) {
unsigned int i;
unsigned long int max=0;
double devicer;
for (i=0;i<mat->size;i++) {
    (max<((mat->head)[i].freq))?(max=((mat->head)[i].freq)):(max=max);
    }
if (max<=UCHAR_MAX) return;
devicer=(double)max/UCHAR_MAX;
for (i=0;i<mat->size;i++) {
	(mat->head)[i].freq=(unsigned long int)(((mat->head)[i].freq)/devicer);
	if ((mat->head)[i].freq==0) ((mat->head)[i].freq)=1;
    }
return;
}

void PrintHuffTree(TreeVert *root) {
if (root==NULL) return;
SetWeight(root);
printf("%u",root->freq);
if (root->sig==1) printf("(%c)",root->chr);
printf(" -> ");
if (root->left!=NULL) printf("%u(%d) ",root->left->freq,root->left->weight);
printf(", ");
if (root->right!=NULL) printf("%u(%d)",root->right->freq,root->right->weight);
printf("\n");
PrintHuffTree(root->left);
PrintHuffTree(root->right);
return;
} /* PrintHuffTree */

unsigned short int GetCode(int chr,TreeVert *root,FreqMat *mat) {
unsigned short int code=0,mask=1;
unsigned int i;
TreeVert *pnt=NULL;
for (i=0;i<mat->size;i++) {
    if ((mat->head)[i].address->chr==chr) {
	   pnt=(mat->head)[i].address;
	   break;
	   }
    }
if (pnt==NULL) return EOF;
while (pnt!=root) {
      if (pnt->weight==1) code=code|mask;
      mask<<=1;
      pnt=pnt->parent;
      }
return code;
}

unsigned int GetWayLength(int chr,TreeVert *root,FreqMat *mat) {
unsigned int way=0;
unsigned int i;
TreeVert *pnt=NULL;
for (i=0;i<mat->size;i++) {
    if ((mat->head)[i].address->chr==chr) {
	   pnt=(mat->head)[i].address;
	   break;
	   }
    }
while (pnt!=root) {
      way++;
      pnt=pnt->parent;
      }
return way;
}

CodeTable *CreateTable(TreeVert *root,FreqMat *mat) {
unsigned int i;
CodeTable *table=(CodeTable*)malloc(sizeof(CodeTable));
table->size=mat->size;
table->head=(CodeTableVert*)calloc(table->size+1,sizeof(CodeTableVert));
for (i=0;i<table->size;i++) {
    (table->head)[i].chr=(mat->head)[i].chr;
	(table->head)[i].code=GetCode((table->head)[i].chr,root,mat);
	(table->head)[i].length=GetWayLength((table->head)[i].chr,root,mat);
    }
return table;
}

int BitState(int chr,int nr) {
int mask=128,i;
if ((nr<0)||(nr>7)) return -1;
for (i=0;i<nr;i++) mask>>=1;
if ((chr&mask)==mask) return 1; else return 0;
}

int compress(FILE *huff_in,FILE *huff_out) {
unsigned short int rot=0,currmask;
unsigned long int datasize=0,mask,j;
unsigned int i,l;
FreqMat *mat;
int collector,chr;
unsigned char insert,chrmask;
TreeVert *root;
CodeTable *table;
mat=BuildMat(huff_in);
CorrectMat(mat);
root=BuildTree(mat);
table=CreateTable(root,mat);
rewind(huff_in);
rewind(huff_out);
fputs("huffcompressed",huff_out);
insert=mat->size;
putc(insert,huff_out);
for (i=0;i<mat->size;i++) {
    insert=(mat->head)[i].chr;
    putc(insert,huff_out);
    insert=(unsigned char)(mat->head)[i].freq;
	putc(insert,huff_out);
    }
fflush(huff_out);
while (!feof(huff_in)) if (getc(huff_in)!=EOF) datasize++;
mask=1<<31;
chrmask=128;
insert=0;
while (mask>0) {
      if ((mask&datasize)==mask) insert|=chrmask;
      chrmask>>=1;
      mask>>=1;
      if (chrmask==0) {
         putc(insert,huff_out);
         chrmask=128;
         insert=0;             
         }
      }
fflush(huff_out);
if (mat->size==1) {
   printf("Kompresja. Wykonano: 100%\nZakonczono.\n");
   return 0;
   }
rewind(huff_in);
mask=128; /* (1<<7) */
collector=0;
j=0;
printf("Kompresja. Wykonano: ");
while (!feof(huff_in)) {
      chr=getc(huff_in);
	  if (chr==EOF) break;
	  j++;
	  l=0;
	  while ((table->head)[l].chr!=chr) l++;
	  currmask=(1<<(((table->head)[l].length)-1));
	  rot=(unsigned short int)(((double)j/datasize)*100);
	  printf("%u%%",rot);
	  if ((rot/10)==0) printf("\b\b"); else printf("\b\b\b");
	  if ((rot/100)==1) printf("\b");
	  for (i=0;i<(table->head)[l].length;i++) {
	  	  if (( currmask & ((table->head)[l].code) )==currmask) collector|=mask;
	  	  currmask>>=1;
		  mask>>=1;
	  	  if (mask==0) {
	     	 mask=128;
		 	 putc(collector,huff_out);
			 collector=0;
	     	 }
		  }
      }
if (mask<128) putc(collector,huff_out);
printf("\nZakonczono.\n");
fflush(huff_out);
return 0;
}

FreqMat *RestoreMat(FILE *file) {
unsigned int n,i; /* n - liczba elementow tablicy */
FreqMat *mat=(FreqMat*)malloc(sizeof(FreqMat));
if (mat==NULL) return NULL;
fseek(file,14,0);
n=getc(file);
if (n==EOF) return NULL;
if (n==0) n=256;
mat->head=(Freq*)calloc(n,sizeof(Freq));
if (mat->head==NULL) return NULL;
for (i=0;i<n;i++) {
	(mat->head)[i].chr=getc(file);
	(mat->head)[i].freq=getc(file);
    }
mat->size=n;
return mat;
}

/* sprawdza poprawnosc zakodowanego pliku */
int CheckFile(FILE *file) {
char header[15];
rewind(file);
fgets(header,15,file);
if (strcmp(header,"huffcompressed")!=0) return -1;
return 0;     
}

/* ustawia znacznik pliku na pierwszym bajcie 4-bajtowego bloku z liczba
 * opisujaca wielkosc nieskompresowanych danych (w bajtach) */
void SetCapacity(FILE *file) {
unsigned int n,i;
rewind(file);
fseek(file,14,1);
n=getc(file);
if (n==0) n=256;
for (i=0;i<n;i++) {
    getc(file);
    getc(file);
    }
return;
}

/* ustawia znacznik pliku na pierwszym bajcie kodowym */
void SetBegin(FILE *file) {
unsigned int n,i;
rewind(file);
fseek(file,14,1);
n=getc(file);
if (n==0) n=256;
for (i=0;i<n;i++) {
    getc(file);
    getc(file);
    }
fseek(file,4,1);
return;     
}

unsigned int GetN(FILE *file) {
unsigned int n;
fseek(file,14,0);
n=getc(file);
if (n!=EOF) return (n==0)?256:n; else return 0;
}

unsigned long int GetDataSize(FILE *file) {
unsigned int i,j;
unsigned long int data=0,glob=0x80000000;
unsigned char currdata,mask;
unsigned int n=GetN(file);
for (i=0;i<n;i++) {
    getc(file);
    getc(file);
    } /* niestety trzeba iteracyjnie odczytywac, bo '\n' moze kopnac */
for (i=0;i<4;i++) {
    mask=128;
    currdata=getc(file);
    for (j=0;j<8;j++) {
        if ((mask&currdata)==mask) data|=mask;
        glob>>=1;
        mask>>=1;
        }
    }
return data;
}

unsigned long int merge(unsigned char a1,unsigned char a2,unsigned char a3,unsigned char a4) {
unsigned long int a=0,mask=(1<<31); unsigned char curr=128;
while (curr>0) { if ((a1&curr)==curr) a|=mask; mask>>=1; curr>>=1; } curr=128;
while (curr>0) { if ((a2&curr)==curr) a|=mask; mask>>=1; curr>>=1; } curr=128;
while (curr>0) { if ((a3&curr)==curr) a|=mask; mask>>=1; curr>>=1; } curr=128;
while (curr>0) { if ((a4&curr)==curr) a|=mask; mask>>=1; curr>>=1; } curr=128;
return a;
}

int uncompress(FILE *huff_in,FILE *huff_out) {
unsigned int n;
unsigned short int rot;
int insert,hugo,mask;
unsigned long int datasize,j,z=0;
unsigned char a1,a2,a3,a4;
TreeVert *root,*curr;
FreqMat *mat;
rewind(huff_in);
if (CheckFile(huff_in)==-1) return -2;
n=GetN(huff_in);
mat=RestoreMat(huff_in);
if (mat==NULL) return -1;
root=BuildTree(mat);
if (root==NULL) return -1;
curr=root;
SetCapacity(huff_in);
a1=getc(huff_in); a2=getc(huff_in); a3=getc(huff_in); a4=getc(huff_in);
datasize=merge(a1,a2,a3,a4);
rewind(huff_out);
if (mat->size==1) {
   printf("Dekompresja. Wykonano: 0%\b\b");
   for (j=1;j<=datasize;j++) {
       putc((mat->head)[0].chr,huff_out);
       rot=((unsigned int)((((double)j/datasize))*100));
       printf("%u%%\b\b",rot);
       if (rot/10>0) printf("\b");
       if (rot/100==1) printf("\b");
       }
   printf("\nZakonczono.\n");
   return 0;
   }
j=datasize;
mask=128;
SetBegin(huff_in);
printf("Dekompresja. Wykonano: ");
while (j>0) {/*zmieniony warunek j>0 */
    hugo=getc(huff_in);
    mask=128;
    while ((mask>0)&&(j>0)) {/*  ...&&(j>0) */
        if ((mask&hugo)==mask) curr=curr->right; else curr=curr->left;
        mask>>=1;
        if (curr->sig==1) {
           insert=curr->chr;
           putc(insert,huff_out);
           z++;
		   rot=(unsigned short int)(((double)z/datasize)*100);
           printf("%u%%",rot);
           if (rot/10==0) printf("\b\b"); else printf("\b\b\b");
           if (rot/100==1) printf("\b");
           j--;
           curr=root;         
           }
        }
    }
printf("\nZakonczono.\n");
fflush(huff_out);
return 0;   
}

int main(int argc,char *argv[]) {
char *infile,*outfile,*param;
FILE *huff_in,*huff_out;
if (argc<4) return error(3,"",argv[0]);
if (argc>4) return error(4,"",argv[0]);
infile=argv[1];
param=argv[2];
outfile=argv[3];
if ((param[0]!='-')||(strlen(param)!=2)) return error(5,"",argv[0]);
if ((param[1]!='c')&&(param[1]!='u')) return error(5,"",argv[0]);
if (strcmp(argv[1],argv[3])==0) return error(6,"",argv[0]);
huff_in=fopen(infile,"rb");
if (huff_in==NULL) return error(1,infile,argv[0]);
rewind(huff_in);
if (getc(huff_in)==EOF) {
   fclose(huff_in);
   return error(7,infile,argv[0]);
   }
/* test na istnienie pliku wyjsciowego */
huff_out=fopen(outfile,"rb");
if (huff_out!=NULL) {
   fprintf(stderr,"Plik %s juz istnieje. Czy nadpisac?(y,n)",outfile);
   if (getchar()!='y') {
      fclose(huff_out);
      fprintf(stderr,"Przerwano\n");
      return 0;
      }
   fprintf(stderr,"Nadpisano\n");
   }
if (huff_out!=NULL) fclose(huff_out);
/* koniec testu */
huff_out=fopen(outfile,"wb");
if (huff_out==NULL) return error(1,outfile,argv[0]);
if (param[1]=='c') {
   if (compress(huff_in,huff_out)==-1) {
      fclose(huff_in);
      fclose(huff_out);
      return error(2,"",argv[0]);
      }
   }
   else {
   if (uncompress(huff_in,huff_out)!=0) {
      fclose(huff_in);
      fclose(huff_out);
      return error(2,"",argv[0]);
      }/*UNCOMPRESS*/
   }
fclose(huff_in);
fclose(huff_out);
return 0;
} /* main */
