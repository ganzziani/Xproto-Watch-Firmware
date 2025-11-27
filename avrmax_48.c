/***************************************************************************/
/*                               AVR-Max,                                  */
/* A chess program smaller than 2KB (of non-blank source), by H.G. Muller  */
/* AVR ATMega88V port, iterative Negamax, by Andre Adrian                  */
/***************************************************************************/
/* 14dez2008 adr: merge avr_c_io and uMax                                  */
/* 18dez2008 adr: return -l if Negamax stack underrun                      */
/* 24dez2008 adr: add functions 1=new game, 2=set level, 3=show PV         */
/* 26dez2008 adr: bug fix, undo modifications on uMax 4.8                  */
/* 29dez2009 adr: Key Debouncing with inhibit (Sperrzeit nach Loslassen)   */

/* Stufe (Level)
   1 Blitz      = min. 0.5s, typ. 7s
   2 2*Blitz    = min. 1s,   typ. 15s    
   3 4*Blitz    = min. 2s,   typ. 22s
   4 Aktiv/2    = min. 4s,   typ. 
   5 Aktiv      = min. 8s,   typ. 30s
   6 2*Aktiv    = min. 16s,  typ.
   7 Turnier/2  = min. 32s,  typ. 
   8 Turnier    = min. 64s,  typ. 
  FN 2*Turnier  = min. 128s, typ.
  CL 4*Turnier  = min. 256s, typ.
*/
#ifndef __unix__
  unsigned char st=2;           /* Spielstufe (Level)                      */
#else
  // UNIX st > AVR st, z.B. AVR(2) ist UNIX(4), AVR(4) ist UNIX(6)
  unsigned char st=4+2;         /* Spielstufe (Level)                      */
#endif

/* Attention: union TIMER assumes little endian CPU or compiler!           */
typedef union {
  long w;
  unsigned char c[4];
} TIMER;

TIMER timer;                    // wird alle 4.096ms erhoeht

#define BLINKOFF  0
#define BLINKALL  1
#define BLINKDOT  2
#define BLINKRUN  4

#ifndef __unix__

/* This is the AVR ATMega88 Selbstbau Schachcomputer specific part         */

#include <avr/io.h>       
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>

#define WIEDERHOLMS	330
#define INHIBITMS   100     // Sperrzeit nach Loslassen

#define WIEDERHOL	((WIEDERHOLMS+8)/16)		// Einheit 16ms
#define INHIBIT     ((INHIBITMS+8)/16)

#define SPACE	36

#define FN  '9'
#define CL  ':'
#define GO  ';'

/* better readability of AVR Program memory arrays */
#define o(ndx)	(signed char)pgm_read_byte(o+(ndx))
#define w(ndx)	(signed char)pgm_read_word(w+(ndx))

// Multiplex Interrupt Variables
// Timer 2  Overflow Interrupt 244Hz
// PC0..PC3 Lines Output, Low activ, 1 out of N
// PD0..PD7 Segment Output, Low active
// PB0..PB2 Keyboard Input, Low is Key pressed

volatile unsigned char keycode; // 0 fuer keine Taste, 1..11 fuer Taste
unsigned char led[4];		// Bitmuster LED Anzeige
unsigned char fblink;		// Blink Flag LED Anzeige

// Aufruf: 8MHz / 128 / 256 = 244Hz = 4.096ms
ISR(TIMER2_OVF_vect)
{
  static unsigned char portcmask;
  static unsigned char newkey;
  static unsigned char key;
  static signed char   keycnt;
  register unsigned char ndx;

  SMCR = 0;
  ndx = timer.c[0] & 0x03;

  // Lese Matrixtasten
  if (0x07 != (PINB & 0x07)) {
    // Taste gedrueckt
    newkey = ndx + 1;
    if (!(PINB & 2))
      newkey += 4;
    if (!(PINB & 4))
      newkey += 8;
  }
  if (0x03 == ndx) {            // letzte Scan Line
	if (keycnt >= 0) {          // keine Sperrzeit
      if (newkey) {
	    if (!key) {				// ansteigende Flanke Erkennung
		  keycode = newkey;
		}
		if (key == newkey) {	// Pegelerkennung gleiche Taste
		  if (++keycnt >= WIEDERHOL) {
		    keycode = newkey;
			keycnt = 0;
		  }
        }
      } else {                  // keine Taste (mehr) gedrueckt
        if (key) {				// abfallende Flanke Erkennung
	      keycnt = -INHIBIT;    // Sperrzeit zum Entprellen
        }
      }
	} else {
	  ++keycnt;					// Sperrzeit ablaufen lassen
	}
    key = newkey;
    newkey = 0;
  }

  ++timer.w;
  ndx = timer.c[0] & 0x03;

  // schreibe 7-Segment
  // Low Ausgang Port C Bit 0, 1, 2, 3, 0, .. reihum auf Low
  // Open Collector Prinzip: Immer nur ein Ausgang aktiv damit ueber
  // mehrere Tasten gleichzeitig gedrueckt kein Kurzschluss moeglich ist

  // 1.) Alle Multiplex Lines auf High (Pull-up oder High Ausgang)
  PORTC = (PORTC & 0xF0) | 0x0F;

  // 2.) neue Multiplex Line festlegen
  if (ndx) {
    portcmask <<= 1;
  } else {
    portcmask = 1;              // Sync portcmask und timer            
  }
  DDRC = (DDRC & 0xF0) | portcmask;

  // 3.) Ausgabe festlegen  
  if ((fblink & BLINKALL) && (timer.c[0] & 0x80)) {
    PORTD = 0xFF;               // dunkel
  } else {
    PORTD = led[ndx];
    if ((fblink & BLINKRUN) && ((timer.c[1] & 0x03) == ndx)
    || (fblink & BLINKDOT) && (timer.c[0] & 0x40)) {
      PORTD &= 0x7F;            // Dot blinken oder laufen
    }
  }

  // 4.) neue Multiplex Line auf Low
  PORTC = (PORTC & 0xF0) | (~portcmask & 0x0F);
}

const unsigned char charRom[] PROGMEM = {
  0b11000000,                  // 0: a, b, c, d, e, f
  0b11111001,                  // 1: b, c
  0b10100100,                  // 2: a, b, d, e, g
  0b10110000,                  // 3: a, b, c, d, g
  0b10011001,                  // 4: b, c, f, g
  0b10010010,                  // 5: a, c, d, f, g
  0b10000010,                  // 6: a, c, d, e, f, g
  0b11111000,                  // 7: a, b, c
  0b10000000,                  // 8: a, b, c, d, e, f, g
  0b10010000,                  // 9: a, b, c, d, f, g 
  0b10001000,                  // A: a, b, c, e, f, g
  0b10000011,                  // b: c, d, e, f, g
  0b11000110,                  // C: a, d, e, f
  0b10100001,                  // d: b, c, d, e, g
  0b10000110,                  // E: a, d, e, f, g
  0b10001110,                  // F: a, e, f, g
  0b11000010,                  // G: a, c, d, e, f
  0b10001001,                  // H: b, c, e, f, g
  0b11001111,                  // I: e, f
  0b11100001,                  // J: b, c, d, e
  0b10001111,                  // k: e, f, g
  0b11000111,                  // L: d, e, f
  0b11001000,                  // M: a, b, c, e, f
  0b10101011,                  // n: c, e, g
  0b10100011,                  // o: c, d, e, g
  0b10001100,                  // P: a, b, e, f, g
  0b10011000,                  // q: a, b, c, f, g
  0b10101111,                  // r: e, g
  0b10010010,                  // S=5: a, c, d, f, g
  0b10000111,                  // t: d, e, f, g
  0b11000001,                  // U: b, c, d, e, f
  0b11100011,                  // v: c, d, e
  0b10000001,                  // W: b, c, d, e, f, g
  0b10001001,                  // X=H: b, c, e, f, g
  0b10010001,                  // Y: b, c, d, f, g
  0b10100100,                  // Z=2: a, b, d, e, g
  0b11111111,                  // Space
};

void putled(unsigned char ndx, unsigned char c)
{
  // ASCII to charRom Code
  if (c >= 'a' && c <= 'z') {
    c -= ('a' - 10);
  } else if (c >= 'A' && c <= 'Z') {
    c -= ('A' - 10);
  } else if (c >= '0' && c <= '9') {
    c -= '0';
  } else {
    c = SPACE;
  }
  led[ndx] = pgm_read_byte(charRom + c);
}

unsigned char getkey(void)
{
  // Erzeuger-Verbraucher Synchronisation ueber Flag+Variable 
  // Interrupt setzt Flag-Variable keycode auf != 0
  // getkey loescht Flag-Variable wieder
  unsigned char ch;
  
  while (!keycode) {
    // Strom sparen: MCU geht zum Warten in Power Save Modus
    // Timer2 Interrupt weckt MCU auf
    SMCR = 1 << SM1 | 1 << SM0 | 1 << SE;
    sleep_cpu();
  }
  // keycode to ASCII
  ch = keycode + '0';
  keycode = 0;
  return ch;
}

void init(void)
{
  // set clock from 1MHz to 8MHz (from Atmel)
  // 1. Write the Clock Prescaler Change Enable (CLKPCE) bit to one and 
  //    all other bits in CLKPR to zero.
  // 2. Within four cycles, write the desired value to CLKPS while 
  //    writing a zero to CLKPCE.
  cli();                                    // disable Interrupts
  CLKPR = 1 << CLKPCE;
  CLKPR = 0;

  // Power reduction: Two-Wire-Interface, ADC, USART off
  ADCSRA = 0;
  PRR = (1 << PRTWI)|(1 << PRUSART0)|(1 << PRADC);

  // Port Pins B0 to B2 are Matrix Keys Inputs
  DDRB = 0;                                             // as Input
  PORTB = (1 << PB2)|(1 << PB1)|(1 << PB0);             // Pullups on

  // Port Pins C0 to C3 are Line (Digit) Outputs, Open Collector Low active
  // (1 of N active, see Interrupt handler)
  DDRC = 0;
  PORTC = (1 << PC3)|(1 << PC2)|(1 << PC1)|(1 << PC0);

  // Port Pins D0 to D7 are Segment Outputs, Low active
  PORTD = 0xff;                 // set to High
  DDRD = 0xff;                  // as Output

  // Timer 2
  TCCR2B |= (1 << CS22) | (1 << CS20);      // Preselector=128
  TIMSK2 |= (1 << TOIE2);                   // Overflow Interrupt

  sei();                                    // enable Interrupts
}

void clean()
{
  putled(0, ' ');putled(1, ' ');putled(2, ' ');putled(3, ' ');
}

void bold()
{
  led[0]&=0x7F;led[1]&=0x7F;led[2]&=0x7F;led[3]&=0x7F;
}

void blink(unsigned char mode)
{
  fblink = mode;
}

#else

#include <stdio.h>
#include <stdlib.h>

#define PROGMEM

#define FN  0x1B
#define CL  '\b'
#define GO  '\n'

/* better readability of AVR Program memory arrays */
#define o(ndx)	o[ndx]
#define w(ndx)	w[ndx]

#define putled(ndx,ch)
#define getkey()
#define clean()
#define bold()
#define sei()
#define cli()

void blink(unsigned char mode)
{
}

void init()
{
}

#endif

/***************************************************************************/
/*                               micro-Max,                                */
/* A chess program smaller than 2KB (of non-blank source), by H.G. Muller  */
/***************************************************************************/
/* version 4.8 (1953 characters) features:                                 */
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

#define M 0x88                              /* Unused bits in valid square */  
#define S 128                               /* Sign bit of char            */
#define I 8000                              /* Infinity score              */
#define U 18                                /* D() Stack array size        */

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

short Q,                                    /* pass updated eval. score    */
K,                                          /* input move, origin square   */
i,s,                                        /* temp. evaluation term       */
Dq,Dl,De,                         /* D() arguments */
DD;                               /* D() return value */

const signed char w[] PROGMEM = {
  0,2,2,7,-1,8,12,23};                         /* relative piece values    */

const signed char o[] PROGMEM = {
  -16,-15,-17,0,1,16,0,1,16,15,17,0,14,18,31,33,0,    /* step-vector lists */
  7,-1,11,6,8,3,6,                             /* 1st dir. in o[] per piece*/
  6,3,5,7,4,5,3,6};                            /* initial piece setup      */
  
unsigned char L,                               /* input move, target square*/
b[129],                                        /* board: half of 16x8+dummy*/
c[5],                                          /* input move ASCII buffer  */     
k,                                          /* moving side 8=white 16=black*/
O,                                          /* pass e.p. flag at game level*/
R,                                          /* captured non pawn material  */
DE,Dz,Dn,                                      /* D() arguments            */
Da,                                            /* D() state                */
W,                                           /* @ temporary                */
hv;                                            /* zeige Hauptvariante      */

void D();

main()
{
 init();
 for(;;) { 
A:blink(BLINKOFF);
  putled(0, 'S');putled(1, 'H');putled(2, 'A');putled(3, 'H');
  k=16;O=Q=R=0;
  for(W=0;W<sizeof b;++W)b[W]=0;
  W=8;while(W--) 
  {b[W]=(b[W+112]=o(W+24)+8)+8;b[W+16]=18;b[W+96]=9; /* initial board setup*/
   L=8;while(L--)
    b[16*L+W+8]=(W-4)*(W-4)+(L+L-7)*(L+L-7)/4;       /* center-pts table   */
  }                                                  /*(in unused half b[])*/

  do{                                                /* play loop          */
#ifndef __unix__
   W = 4;
   for(;;) {
    switch (c[W] = getkey()) {
    case FN:
      putled(0, 'F');putled(1, 'N');putled(2, ' ');putled(3, ' ');
      switch (L = getkey()) {
      case '1': goto A; break;      // Neues Spiel (new game)
      case '2':                     // Spielstufe (set level)
        for(;;) {
          putled(0, 'S');putled(1, 'T');putled(3, st+'1');
          if (GO == (L = getkey())) break;
          st=L-'1';
        }
        break;
      case '3':                     // Hauptvariante (show PV)
        for(;;) {
          putled(0, 'H');putled(1, 'V');putled(3, hv+'0');
          if (GO == (L = getkey())) break;
          hv=L&1;
        }
        break;
      }
      // no break
    case CL: W=5; break;
    case GO: goto B; break;
    }
    if (!(W&1)) {          // wahr bei W ist gerade (0, 2, 4)
      c[W] += 'a' - '1';
    }
    switch (W) {
    default:
	  putled(W, c[W]);
	  ++W;
      break;
    case 4:                 // getkey() anzeigen
	  clean();
	  c[0]=c[4];
	  putled(0, c[0]);
	  W=1;
      break;
    case 5:                 // getkey() nicht anzeigen
	  clean();
      W=0;
      break;
    }
   }   
B: 
#else
   K=-1;while(++K<121)                               /* print board        */
    printf(" %c",K&8&&(K+=7)?10:".?inkbrq?I?NKBRQ"[b[K]&15]);
   K=0;while((c[K++]=getchar())>10);                 /* read input line    */
#endif
   if(*c-GO)K=*c-16*c[1]+799,L=c[2]-16*c[3]+799;     /* parse entered move */
   else {
     K=I;
     cli();                   // Interrupts sperren
     timer.w= -128<<st;                              /* set time control   */
     sei();                   // Interrupts erlauben
     clean();
     if (hv) { bold(); blink(BLINKALL);}
     else blink(BLINKRUN);
   }
   Dq=-I;Dl=I;De=Q;DE=O;Dz=1;Dn=3;               /* store arguments of D() */
   Da=0;                                         /* state */
   D();           
   blink(BLINKOFF);
   if(*c-GO) {          // Zugeingabe vorhanden
    if(I==DD) {         // Zug ist gueltig
     bold();
    } else {            // .. ungueltig
     putled(0, 'Z');putled(1, 'U');putled(2, 'G');putled(3, ' ');
    } 	
    *c=GO;
   } else {             // Computerzug
    putled(0,'a'+(K&7));putled(1,'8'-(K>>4));  
    putled(2,'a'+(L&7));putled(3,'8'-(L>>4&7)); 
   }   
  }while(DD>-I+1);
  putled(0, 'M');putled(1, 'A');
  putled(2, 'T');putled(3, 'T');
  getkey();
#ifdef __unix__
  break;
#endif
 }
}

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
   z&K==I&&(timer.w<0&d<98||                  /* root: deepen upto time   */
   (K=X,L=Y&~M,d=3)))                          /* time's up: go do best    */
 {x=B=X;                                       /* start scan at prev. best */
  h=Y&S;                                       /* request try noncastl. 1st*/
  if(d<3)P=I;else 
  {*J=_;Dq=-l;Dl=1-l;De=-e;DE=S;Dz=0;Dn=d-3;   /* save locals, arguments   */
   Da=0;goto D;                                /* Search null move         */
R0:_=*J;P=DD;                                  /* load locals, return value*/
  }
  m=-P<l|R>35?d>2?-I:e:-P;                     /* Prune or stand-pat       */
#ifdef __unix__
  ++timer.w;                                   /* node count (for timing)  */
#endif  
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
      i=37*w(t&7)+(t&192);                     /* value of capt. piece t   */
      m=i<0?I:m;                               /* K capture                */
      if(m>=l&d>1)goto J;                      /* abort on fail high       */

      v=d-1?e:i-p;                             /* MVV/LVA scoring          */
      if(d-!t>1)                               /* remaining depth          */
      {v=p<6?b[x+8]-b[y+8]:0;                  /* center positional pts.   */
       b[G]=b[H]=b[x]=0;b[y]=u|32;             /* do move, set non-virgin  */
       if(!(G&M))b[F]=k+6,v+=50;               /* castling: put R & score  */
       v-=p-4|R>29?0:20;                       /* penalize mid-game K move */
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
      if(v>m)                                  /* new best, update max,best*/
       m=v,X=x,Y=y|S&F;                        /* mark double move with S  */
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
#ifndef __unix__  
   if(z&hv&d>2)                                   
   {putled(0,'a'+(X&7));putled(1,'8'-(X>>4));  /* show Principal variation */
    putled(2,'a'+(Y&7));putled(3,'8'-(Y>>4&7)); 
   }
#else   
   if(z&d>2)
   {printf("%2d ply, %9ld searched, score=%6d by %c%c%c%c\n",
     d-1,timer.w-(-128<<st),m,
     'a'+(X&7),'8'-(X>>4),'a'+(Y&7),'8'-(Y>>4&7)); /* uncomment for Kibitz */
   }
#endif     
 }                                             /*    encoded in X S,8 bits */
 k^=24;                                        /* change sides back        */
 ++J;DD=m+=m<e;                                /* delayed-loss bonus       */
R:if (J!=A+U) switch(a){case 0:goto R0;case 1:goto R1;}
 else return;
}

