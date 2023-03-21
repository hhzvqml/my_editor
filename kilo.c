#include<stdio.h>
#include<sys/types.h>
#include<termios.h>
#include<unistd.h>
#include<stdlib.h>
#include<errno.h>
#include<sys/ioctl.h>
#include<string.h>



/***mascro***/
#define CTRL_KEY(k)((k)&0x1f)
#define ABUF_INIT {NULL,0}
#define KILO_VERSION "0.0.1" 

/***struct define***/

typedef struct erow{
	int size;
	char *chars;
}erow;
struct editorConfig{
	int cx,cy;
	struct termios origal_raw;
	int screenrows;
	int screencols;
    int numrows;
    erow row;
};

struct abuf{
	char *b;
	int len;
};



enum editorKey{
	ARROW_LEFT=1001,
	ARROW_RIGHT=1002,
	ARROW_UP=1003,
	ARROW_DOWN=1004
};

/***function define***/
void enableRawMode();
void disableRawMode(void);
int editorReadKey();
void editorProcessKeypress();
void die(char *);
void editorRefreshScreen();
void editorDrawRows(struct abuf *ab);
void abAppend(struct abuf *ab,const char *s,int len);
void abFree(struct abuf *ab);
int getWindowSize(int* row,int* col);
void init();
void editorOpen();
void editorMoveCursor(int key);
/***function define***/

/***gobal varible***/
struct editorConfig E;
int main()
{
	enableRawMode();
	init();
    editorOpen();
	while(1){
		editorRefreshScreen();
		editorProcessKeypress();
	}
}




/****Init***/
void init(){
	E.cx=0;
	E.cy=0;
    E.numrows=0;
	if(getWindowSize(&E.screenrows,&E.screencols)==-1)die("getWindow wrong");
}


/***file I/O***/
void editorOpen(){
    char *line="Hello world!";
    ssize_t linelen=13;
    E.row.size=linelen;
    E.row.chars=malloc(linelen+1);
    memcpy(E.row.chars,line,linelen);
    E.row.chars[linelen]='\0';
    E.numrows=1;
}










/***screen section***/
void enableRawMode(){
	atexit(disableRawMode);
	struct termios raw;
	tcgetattr(STDIN_FILENO,&E.origal_raw);
	raw=E.origal_raw;
	raw.c_cflag |=(CS8);
	raw.c_oflag &=~(OPOST);
	raw.c_iflag &=~(ICRNL|IXON);
	raw.c_lflag &=~(ECHO|ICANON|ISIG|IEXTEN);
	raw.c_cc[VMIN]=0;
	raw.c_cc[VTIME]=1;
	tcsetattr(STDIN_FILENO,TCSAFLUSH,&raw);
}

void disableRawMode(){
	tcsetattr(STDIN_FILENO,TCSAFLUSH,&E.origal_raw);
}
int getWindowSize(int *rows,int *cols){
	struct winsize ws;
	if(ioctl(STDOUT_FILENO,TIOCGWINSZ,&ws)==-1||ws.ws_col==0){
		return -1;
	}else{
		*cols=ws.ws_col;
		*rows=ws.ws_row;
	}
	return 0;
}

void editorRefreshScreen(){
	struct abuf ab=ABUF_INIT;
	abAppend(&ab,"\x1b[?25l",6);
	abAppend(&ab,"\x1b[H",4); 
	editorDrawRows(&ab);
	char buf[32];
	snprintf(buf,sizeof(buf),"\x1b[%d;%dH",E.cy+1,E.cx+1);
	abAppend(&ab,buf,strlen(buf));
	abAppend(&ab,"\x1b[?25h",6);
	write(STDOUT_FILENO,ab.b,ab.len);
	abFree(&ab);
}


/***read key section ***/
int editorReadKey(){
	int nread;//为了进行双重检测
	char c;
	while((nread=read(STDIN_FILENO,&c,1))!=1){
		if(nread==-1&&errno!=EAGAIN)die("read");
	}
	if(c=='\x1b'){
		char seq[3];
		if(read(STDIN_FILENO,&seq[0],1)!=1)return '\x1b';
		if(read(STDIN_FILENO,&seq[1],1)!=1)return '\x1b';
		if(seq[0]=='['){
			switch(seq[1]){
				case 'A':return ARROW_UP;
				case 'B':return ARROW_DOWN;
				case 'C':return ARROW_RIGHT;
				case 'D':return ARROW_LEFT;
			}
		}
		return '\x1b';
	}else{
	return c;
	}
}

void editorProcessKeypress(){
	int c=editorReadKey();
	switch(c){
		case CTRL_KEY('q'):
			write(STDOUT_FILENO,"\x1b[H",3);
			write(STDOUT_FILENO,"\x1b[2J",4);
			exit(0);
		case ARROW_UP:
		case ARROW_LEFT:
		case ARROW_RIGHT:
		case ARROW_DOWN:
			editorMoveCursor(c);
			break;
			
	}
}

/***draw word in the screen***/
void editorDrawRows(struct abuf *ab){
	int y;
	for(y=0;y<E.screenrows;y++){
		if(y==E.screenrows/3){
			char welcome[80];
			int welcomelen =snprintf(welcome,sizeof(welcome),"Kilo editor --version %s",KILO_VERSION);
			if(welcomelen>E.screencols)welcomelen=E.screencols;
			int padding =(E.screencols -welcomelen)/2;
			if(padding){
				abAppend(ab,"~",1);
				padding--;
			}
			while(padding--)abAppend(ab," ",1);
			abAppend(ab,welcome,welcomelen);
		}else{
			abAppend(ab,"~",1);
		}

		abAppend(ab,"\x1b[K",3);
		if(y<E.screenrows-1){
			abAppend(ab,"\r\n",2);
		}
	}
}

/***Buffer ***/
void abAppend(struct abuf *ab,const char *s,int len){
	char *new =realloc(ab->b,ab->len+len);
	if(new==NULL)return ;
	memcpy(&new[ab->len],s,len);
	ab->b=new;
	ab->len+=len;
}
void abFree(struct abuf *ab){
	free(ab->b);
}

/***move*****/
void editorMoveCursor(int key)
{
	switch(key){
		case ARROW_UP:
			if(E.cy!=0)E.cy--;
			break;
		case ARROW_LEFT:
			if(E.cx!=0)E.cx--;
			break;
		case ARROW_RIGHT:
			if(E.cx!=E.screencols)E.cx++;
			break;
		case ARROW_DOWN:
			if(E.cy!=E.screenrows)E.cy++;
			break;
	}
}









/***warn ***/
void die(char* s){
	perror(s);
	exit(1);
}

