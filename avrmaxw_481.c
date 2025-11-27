/***************************************************************************/
/*                               micro-Max,                                */
/* A chess program smaller than 2KB (of non-blank source), by H.G. Muller  */
/* AVR port, iterative version, by Andre Adrian                            */
/***************************************************************************/
/* version 4.81 (1953+ characters) features:                               */
/* - recursive negamax search                                              */
/* - all-capture MVV/LVA quiescence search                                 */
/* - (internal) iterative deepening                                        */
/* - best-move-first 'sorting'                                             */
/* - a hash table storing score and best move                              */
/* - futility pruning                                                      */
/* - king safety through magnetic, frozen king in middle-game              */
/* - R=2 null-move pruning                                                 */
/* - keep hash and repetition-draw detection                               */
/* - better defense against passers through gradual promotion              */
/* - extend check evasions in inner nodes                                  */
/* - reduction of all non-Pawn, non-capture moves except hash move (LMR)   */
/* - full FIDE rules (expt under-promotion) and move-legality checking     */

/* 5dez2008 adr: D() local i, s to global */
/* 5dez2008 adr: add xboard/winboard interface                             */
/*                xboard -fcp /home/adrian/schach/avrmaxw_481 &            */
/* 20dez2008 adr: penalize early Q move                                    */
/* 22dez2008 adr: random play of equal moves for less draws                */

/* run a match with Crafty on search depth 2 (Brute Force Sockel)
xboard -depth 2 -mg 6 -fcp /home/adrian/schach/avrmaxw_481 &

or with GNU Chess 5 on 10 seconds time control
xboard -tc 0:10 -mg 6 -scp gnuchessx -fcp /home/adrian/schach/avrmaxw_481 &
*/

// #define DEBUG

#define M 0x88                              /* Unused bits in valid square */  
#define S 128                               /* Sign bit of char            */
#define I 8000                              /* Infinity score              */
#define NN 5000                             /* nodes counter start value   */
#define U 20                                /* D() Stack array size        */

struct {
short q,l,e;          /* Args: (q,l)=window, e=current eval. score         */
short m,v,            /* m=value of best move so far, v=current evaluation */
 V,P;
unsigned char E,z,n;  /* Args: E=e.p. sqr.; z=level 1 flag; n=depth        */          
signed char r;        /* step vector current ray                           */
unsigned char j,      /* j=loop over directions (rays) counter             */ 
 B,d,                 /* B=board scan start, d=iterative deepening counter */ 
 h,C,                 /* h=new ply depth?, C=ply depth?                    */
 u,p,                 /* u=moving piece, p=moving piece type               */
 x,y,                 /* x=origin square, y=target square of current move  */
 F,                   /* F=e.p., castling skipped square                   */
 G,                   /* G=castling R origin (corner) square               */
 H,t,                 /* H=capture square, t=piece on capture square       */
 X,Y,                 /* X=origin, Y=target square of best move so far     */
 a;                   /* D() return address state                          */
} _, A[U],*J=A+U;     /* _=working set, A=stack array, J=stack pointer     */

long N;               /* nodes counter, count down                         */
short Q,              /* pass updated eval. score    */
K,                    /* input move, origin square or Infinity as flag     */
i,                                          /* temp. evaluation term       */
s,
Dq,Dl,De,                         /* D() arguments */
DD;                               /* D() return value */

const short w[] = {
  0,2*37,2*37,7*37,-1,8*37,12*37,23*37};       /* relative piece values    */

const signed char o[] = {
  -16,-15,-17,0,1,16,0,1,16,15,17,0,14,18,31,33,0,    /* step-vector lists */
  7,-1,11,6,8,3,6,                             /* 1st dir. in o[] per piece*/
  6,3,5,7,4,5,3,6};                            /* initial piece setup      */

unsigned char L,                               /* input move, target square*/
O,                                          /* pass e.p. flag at game level*/
k=16,                                       /* moving side 8=white 16=black*/
b[129],                                        /* board: half of 16x8+dummy*/
c[9],                                          /* input move ASCII buffer  */     
R,                                          /* captured non pawn material  */
W;                                           /* @ pseudo random number     */

unsigned char DE,Dz,Dn,                        /* D() arguments            */
Da;                                            /* D() state                */

/* better readability of AVR Program memory arrays */
#define o(ndx)	o[ndx]
#define w(ndx)	w[ndx]

void D();

// **********************************************************************
// *
// *  xboard/WinBoard interface with chess notation
// *
// *       by Andre Adrian
// **********************************************************************

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>

/* 16bit pseudo random generator */
#define MYRAND_MAX 65535

unsigned short r = 1;                     /* pseudo random generator seed */

void mysrand(unsigned short r_) {
 r = r_;
}

unsigned short myrand(void) {
 return r=((r<<11)+(r<<7)+r)>>1;
}

void new_game()
{
 J=A+U;
 Q=0;O=0;R=0;k=16;i=0;s=0;R=0;
 for(W=0;W<sizeof b;++W)b[W]=0;
 W=8;while(W--) 
 {b[W]=(b[W+112]=o(W+24)+8)+8;b[W+16]=18;b[W+96]=9;  /* initial board setup*/
  L=8;while(L--)
   b[16*L+W+8]=(W-4)*(W-4)+(L+L-7)*(L+L-7)/4;        /* center-pts table   */
 }                                                   /*(in unused half b[])*/
}

char white;			                     /* computer color flag*/
char str[100];                                       /* xboard exchange buf*/
FILE *fp;                                            /* logging file ptr.  */

void log_init()
{
#ifdef DEBUG  
  fp = fopen( "/home/adrian/schach/avrmaxrw_481.log" , "a");    
  fprintf(fp, "# AVR Max 4.81\n");
  fflush(fp);
#endif                              
}

void log_str(char *s)
{
#ifdef DEBUG  
  fprintf(fp, str);
  fflush(fp);
#endif
}

void computer_move()
{
  N=NN;                                              /* set time control   */
  K=I;                                               /* invalid move       */
  Dq=-I;Dl=I;De=Q;DE=O;Dz=1;Dn=3;                /* store arguments of D() */
  Da=0;                                          /* set state */
  D();                                                 /* do computer move */

  if (0==DD && K==0 && L==0) {
    sprintf(str, "1/2-1/2 {Patt o. Remis}\n");
  } else if (-I+1==DD) {
    if (white) {
      sprintf(str, "0-1 {Schwarz gewinnt}\n");
    } else {
      sprintf(str, "1-0 {Weiss gewinnt}\n");
    }
  } else {
    sprintf(str, "move %c%c%c%c\n", 
     'a'+(K&7),'8'-(K>>4),'a'+(L&7),'8'-(L>>4));                
  }
  printf(str);
  log_str(str);
}

int main(int argc, char *argv[])
{  
/* Tested with xboard/WinBoard version 4.2.7
 * Interface does understand the commands: xboard, new, white, black, 
 * go, quit and move in e2e4 form.
 * Interface emits Error, move in e7e5 form and result.
 * xboard communication protocol is documented in
 * http://www.tim-mann.org/xboard/engine-intf.html
 */
  
  signal(SIGINT, SIG_IGN);                     /* xboard needs this        */
  log_init();
  new_game();
  for (;;) {
    if(!fgets(str, sizeof(str), stdin)) {
      return 1;
    }
    log_str(str);
    if('\n' == str[strlen(str)-1]) {
      str[strlen(str)-1]=0;
    }
    if (!strcmp(str, "xboard")) {
      printf("feature myname=\"AVR-Max 4.81\" reuse=0 done=1\n");
    } else if (!strcmp(str, "new")) {
      new_game();
      white = 0;
    } else if (!strcmp(str, "random")) {
      mysrand(time(NULL));                   /* make myrand() calls random */
    } else if (!strcmp(str, "white")) {
      white = 1;                            /* xboard tells computer color */
    } else if (!strcmp(str, "black")) {
      white = 0;
    } else if (!strcmp(str, "go")) {
      if (white) {
        computer_move();
      }
    } else if (!strcmp(str, "quit")) {
      return 0;
    } else if ((4 == strlen(str) || 5 == strlen(str))
     && str[0] >= 'a' && str[0] <= 'h'
     && str[1] >= '1' && str[1] <= '8'
     && str[2] >= 'a' && str[2] <= 'h'
     && str[3] >= '1' && str[3] <= '8') {
      if (5 == strlen(str)) {
                 /* under-promotions: type 1,2,3 (=R,B,N) after input move */
        switch (str[4]) {
        case 'r': str[4]='1'; break;
        case 'b': str[4]='2'; break;
        case 'n': str[4]='3'; break;
        default:  str[4]=0; break;
        }
      }
      sprintf(c, "%s\n", str);
      N=NN;
      K=*c-16*c[1]+799,L=c[2]-16*c[3]+799;           /* parse entered move */
      Dq=-I;Dl=I;De=Q;DE=O;Dz=1;Dn=3;            /* store arguments of D() */
      Da=0;                                           /* state */
      D();                                            /* accept human move */
      if (I==DD) {
        computer_move();
      } else {
        printf("Illegal move: %s\n", str);
      }
    } else {
      printf("Error (unknown command): %s\n", str);
    }
    fflush(stdout);
  }
  return (0);
}

#define putled(ndx, ch) c[ndx]=ch

/* better readability of working struct variables */
#define q _.q
#define l _.l
#define e _.e
#define E _.E
#define z _.z
#define n _.n
#define m _.m
#define v _.v
#define V _.V
#define P _.P
#define r _.r
#define j _.j
#define B _.B
#define d _.d
#define h _.h
#define C _.C
#define u _.u
#define p _.p
#define x _.x
#define y _.y
#define F _.F
#define G _.G
#define H _.H
#define t _.t
#define X _.X
#define Y _.Y
#define a _.a

void D()                                       /* iterative Negamax search */
{                       
D:if (--J<A) {               /* stack pointer decrement and underrun check */
  ++J;DD=-l;goto R;                                    /* simulated return */
 } 
 q=Dq;l=Dl;e=De;E=DE;z=Dz;n=Dn;                          /* load arguments */
 a=Da;                                         /* load return address state*/

 --q;                                          /* adj. window: delay bonus */
 k^=24;                                        /* change sides             */
 d=X=Y=0;                                      /* start iter. from scratch */

 while(d++<n||d<3||                            /* iterative deepening loop */
   z&K==I&&(N>=0&d<98||                        /* root: deepen upto time   */
   (K=X,L=Y&~M,d=3)))                          /* time's up: go do best    */
 {x=B=X;                                       /* start scan at prev. best */
  h=Y&S;                                       /* request try noncastl. 1st*/
  if(d<3)P=I;else 
  {*J=_;Dq=-l;Dl=1-l;De=-e;DE=S;Dz=0;Dn=d-3;   /* save locals, arguments   */
   Da=0;goto D;                                /* Search null move         */
R0:_=*J;P=DD;                                  /* load locals, return value*/
  }
  m=-P<l|R>35?d>2?-I:e:-P;                     /* Prune or stand-pat       */
  --N;                                         /* node count (for timing)  */
  do{u=b[x];                                   /* scan board looking for   */
   if(u&k)                                     /*  own piece (inefficient!)*/
   {r=p=u&7;                                   /* p = piece type (set r>0) */
    j=o(p+16);                                 /* first step vector f.piece*/
    while(r=p>2&r<0?-r:-o(++j))                /* loop over directions o[] */
    {A:                                        /* resume normal after best */
     y=x;F=G=S;                                /* (x,y)=move, (F,G)=castl.R*/
     do{                                       /* y traverses ray, or:     */
      H=y=h?Y^h:y+r;                           /* sneak in prev. best move */
      if(y&M)break;                            /* board edge hit           */
      m=E-S&b[E]&&y-E<2&E-y<2?I:m;             /* bad castling             */
      if(p<3&y==E)H^=16;                       /* shift capt.sqr. H if e.p.*/
      t=b[H];if(t&k|p<3&!(y-x&7)-!t)break;     /* capt. own, bad pawn mode */
      i=w(t&7)+(t&192);                        /* value of capt. piece t   */
      m=i<0?I:m;                               /* K capture                */
      if(m>=l&d>1)goto J;                      /* abort on fail high       */

      v=d-1?e:i-p;                             /* MVV/LVA scoring          */
      if(d-!t>1)                               /* remaining depth          */
      {v=p-4|R>29                              /* early or mid-game K move */
        ?(p-7|R>7                            /* @ early Q move             */
         ?(p<6                                 /* crawling pieces          */
          ?b[x+8]-b[y+8]                       /* center positional pts.   */
          :0)
         :-12)                               /* @ penalize early Q move    */
        :-9;                                   /* penalize mid-game K move */
       b[G]=b[H]=b[x]=0;b[y]=u|32;             /* do move, set non-virgin  */
       if(!(G&M))b[F]=k+6,v+=30;               /* castling: put R & score  */
       if(p<3)                                 /* pawns:                   */
       {v-=9*((x-2&M||b[x-2]-u)+               /* structure, undefended    */
              (x+2&M||b[x+2]-u)-1              /*        squares plus bias */
             +(b[x^16]==k+36))                 /* kling to non-virgin King */
             -(R>>2);                          /* end-game Pawn-push bonus */
        V=y+r+1&S?647-p:2*(u&y+16&32);         /* promotion or 6/7th bonus */
        b[y]+=V;i+=V;                          /* change piece, add score  */
       }
       v+=e+i;V=m>q?m:q;                       /* new eval and alpha       */
       C=d-1-(d>5&p>2&!t&!h);
       C=R>29|d<3|P-I?C:d;                     /* extend 1 ply if in check */
       do
        if(C>2|v>V)
        {*J=_;Dq=-l;Dl=-V;De=-v;DE=F;Dz=0;Dn=C; /* save locals, arguments  */
         Da=1;goto D;                          /* iterative eval. of reply */
R1:      _=*J;s=-DD;                           /* load locals, return value*/
        }else s=v;                             /* or fail low if futile    */
       while(s>q&++C<d);v=s;
       if(z&&K-I&&v+I&&x==K&y==L)              /* move pending & in root:  */
       {Q=-e-i;O=F;                            /*   exit if legal & found  */
        R+=i>>7;++J;DD=l;goto R;               /* captured non-P material  */
       }
       b[G]=k+6;b[F]=b[y]=0;b[x]=u;b[H]=t;     /* undo move,G can be dummy */
      }
      if(v>m                                   /* new best, update max,best*/
      ||(v==m&&++W>7))                       /* @ random play for less draw*/
       m=v,X=x,Y=y|S&F,W=0;                    /* mark double move with S  */
      if(h){h=0;goto A;}                       /* redo after doing old best*/
      if(x+r-y|u&32|                           /* not 1st step,moved before*/
         p>2&(p-4|j-7||                        /* no P & no lateral K move,*/
         b[G=x+3^r>>1&7]-k-6                   /* no virgin R in corner G, */
         ||b[G^1]|b[G^2])                      /* no 2 empty sq. next to R */
        )t+=p<5;                               /* fake capt. for nonsliding*/
      else F=y;                                /* enable e.p.              */
     }while(!t);                               /* if not capt. continue ray*/
  }}}while((x=x+9&~M)-B);                      /* next sqr. of board, wrap */
J:if(m>I-M|m<M-I)d=98;                         /* mate holds to any depth  */
  m=m+I|P==I?m:0;                              /* best loses K: (stale)mate*/
   if(z&d>2)                                   
   {putled(0,'a'+(X&7));putled(1,'8'-(X>>4));  /* show Principal variation */
    putled(2,'a'+(Y&7));putled(3,'8'-(Y>>4&7)); 
#ifdef DEBUG  
    fprintf(fp,"%2d ply, %9d searched, score=%6d by %c%c%c%c\n",d-1,NN-N,m,
     'a'+(X&7),'8'-(X>>4),'a'+(Y&7),'8'-(Y>>4&7)); /* uncomment for Kibitz */
#endif     
   }
 }                                             /*    encoded in X S,8 bits */
 k^=24;                                        /* change sides back        */
 ++J;DD=m+=m<e;                                /* delayed-loss bonus       */
R:if (J!=A+U) switch(a){case 0:goto R0;case 1:goto R1;}
 else return;
}
