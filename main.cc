#include <SDL2/SDL.h>
#include <cstdint>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#define W4(x) case x:case x+16:case x+32:case x+48:om=op-x;
#define Y4(x) case x:case x+8:case x+16:case x+24:
#define X7(x) case x:case x+1:case x+2:case x+3:case x+4:case x+5:case x+7:p=R8[op&7];
#define Y7(x) Y4(x)case x+32:case x+40:case x+56:
#define Y8(x) Y4(x)Y4(x+32)om=op-x;
#define Y7P(x) Y7(x)p=R8[(op-x)/8];
#define Y49(x) Y7(x)Y7(x+1)Y7(x+2)Y7(x+3)Y7(x+4)Y7(x+5)Y7(x+7)om=op-x;
#define Y56(x) Y49(x)X7(x+48)om=op-x;
#define A9(x) case x+6:case x+70:X7(x)v=(op==x+6)?r(HL):(op==x+70)?r8():*p;
#define F5(x,y) Y4(x)case y:fc=(op==y)||(F&FM[(op-x)/8])==FE[(op-x)/8];

using u8=uint8_t; using u16=uint16_t;
u8 op, om, u, v, fc, fn, *rom0, *rom1, io[512], vram[8192], wram[16384], *eram,
    *eram1, R[]={19,0,216,0,77,1,176,1,254,255}, &F=R[6], &A=R[7],
    *R8[]={R+1,R,R+3,R+2,R+5,R+4,&F,&A}, &IF=io[271],
    &LCDC=io[320], &LY=io[324], IME=0, halt=0, *p;
u8 const *Sk;
u16 PC=256, U, *RR=(u16*)R, &HL=RR[2], &SP=RR[4], &DIV=(u16&)io[259], dot=32, *P,
    *RR2[]={RR,RR+1,&HL,&SP}, *RR3[]={RR,RR+1,&HL,&HL};
uint32_t fb[23040], pal[]={0xffffffff,0xffaaaaaa,0xff555555,0xff000000}, bcy, cy=0;
int HA[]={0,0,1,-1}, FM[]={128,128,16,16}, FE[]={0,128,0,16};

void T() { cy+=4; }
u8 mem(u16 a, u8 x, int w) { T();
  switch (a>>12) {
    default: case 2:case 3: if (w) rom1=rom0+((x?(x&63):1)<<14);
    case 0:case 1: return rom0[a];
    case 4:case 5: if (w&&x<=3) eram1=eram+(x<<13);
    case 6:case 7: return rom1[a&16383];
    case 8:case 9: a&=8191; if (w) vram[a]=x; return vram[a];
    case 10:case 11: a&=8191; if (w) eram1[a]=x; return eram1[a];
    case 15:
      if (a >= 65024) {
        if (w) {
          if (a==65350) for(int i=160;--i>=0;) io[i]=mem(x*256+i,0,0);
          io[a&511] = x;
        }
        if (a==65280) {
          if (!(io[256]&16)) return ~(16+Sk[81]*8+Sk[82]*4+Sk[80]*2+Sk[79]);
          if (!(io[256]&32)) return ~(32+Sk[40]*8+Sk[43]*4+Sk[29]*2+Sk[27]);
          return 255;
        }
        return io[a&511];
      }
    case 12 ... 14: a&=16383; if (w) wram[a]=x; return wram[a];
  }
}
u8 r(u16 a) { return mem(a,0,0); }
void w(u16 a, u8 x) { mem(a,x,1); }
void FS(u8 M,int Z,int N,int H,int C) { F=(F&M)|(!Z*128+N*64+H*32+C*16); }
u8 r8() { return r(PC++); }
u16 r16(u16&R=PC) { u=r(R++); return r(R++)*256+u; }
void push(u16 x) { w(--SP,x>>8);w(--SP,x); T(); }

int main() {
  rom1 = (rom0 = (u8*)mmap(0,1048576,PROT_READ,MAP_SHARED,open("rom.gb",O_RDONLY),0)) + 32768;
  int sav = open("rom.sav",O_CREAT|O_RDWR,0666);
  ftruncate(sav,32768);
  eram1 = eram = (u8*)mmap(0,32768,PROT_READ|PROT_WRITE,MAP_SHARED,sav,0);
  LCDC=145;DIV=44032;
  SDL_Init(SDL_INIT_EVERYTHING);
  SDL_Window* Sw = SDL_CreateWindow("pokegb",0,0,800,720, SDL_WINDOW_SHOWN);
  SDL_Renderer* Sr = SDL_CreateRenderer(Sw, -1, 0/*SDL_RENDERER_PRESENTVSYNC*/);
  SDL_Texture *St = SDL_CreateTexture(Sr, SDL_PIXELFORMAT_RGBA32,
                                      SDL_TEXTUREACCESS_STREAMING, 160, 144);
  Sk = SDL_GetKeyboardState(0);

  while (1) {
    bcy=cy;
    if (IME&IF&io[511]) { IF=halt=IME=0;cy+=8;fc=1;U=64;goto CALLU; }
    else if (halt) T();
    else switch ((op = r8())) {
      case 0: break;
      W4(1) *RR2[op>>4]=r16(); break;
      W4(2) w(*RR3[op>>4], A); HL+=HA[om/16]; break;
      W4(3) (*RR2[op>>4])++; T(); break;
      Y7P(4) u=++(*p); goto INC;
      case 52: w(HL,u=r(HL)+1); INC: FS(16,u,0,!(u&15),0); break;
      Y7P(5) u=--(*p); goto DEC;
      case 53: w(HL,u=r(HL)-1); DEC: FS(16,u,1,u%16==15,0); break;
      Y7P(6) *p=r8(); break;
      case 7: A+=A+(fc=A>>7); goto FL;
      W4(9) P=RR2[op>>4];FS(128,1,0,HL%4096+*P%4096>4095,HL+*P>65535);HL+=*P;T(); break;
      W4(10) A=r(*RR3[op>>4]); HL+=HA[om/16]; break;
      W4(11) (*RR2[op>>4])--; T(); break;
      case 15: A=(fc=A&1)*128+A/2; goto FL;
      case 23: fc=A>>7;A+=A+(F>>4)%2; FL: FS(0,1,0,0,fc); break;
      F5(32,24) u=r8(); if(fc) PC+=(int8_t)u,T(); break;
      case 39: fc=u=0;
        if (F&32||(!(F&64)&&A%15>9)) u=6;
        if (F&16||(!(F&64)&&A>153)) u|=96,fc=1;
        FS(65,A+=(F&64)?-u:u,0,0,fc);
        break;
      case 47: A=~A; FS(129,1,1,1,0); break;
      case 54: w(HL,r8()); break;
      case 55:case 63: FS(128,1,0,0,op==55?1:!(F&16)); break;
      Y7P(70) *p=r(HL); break;
      Y49(64) *R8[om/8]=*R8[op&7]; break;
      X7(112) w(HL,*p); break;
      case 118: halt=1; break;
      A9(128) fn=fc=0; goto ALU;
      A9(136) fn=0;fc=(F>>4)&1; goto ALU;
      A9(144) fc=1; goto SUB;
      A9(152) fc=!((F>>4)&1); SUB: fn=1;v=~v; ALU: FS(0,u=A+v+fc,fn,(A%16+v%16+fc>15)^fn,(A+v+fc>255)^fn);A=u; break;
      A9(160) FS(0,A&=v,0,1,0); break;
      A9(168) FS(0,A^=v,0,0,0); break;
      A9(176) FS(0,A|=v,0,0,0); break;
      A9(184) FS(0,A!=v,1,A%16<v%16,A<v); break;
      case 217: fc=IME=1; goto RET;
      F5(192,201) RET: T(); if (fc) PC=r16(SP); break;
      W4(193) RR[om>>4]=r16(SP); break;
      F5(194,195) U=r16(); if (fc) PC=U,T(); break;
      F5(196,205) U=r16(); CALLU: if (fc) push(PC),PC=U; break;
      W4(197) push(RR[om>>4]); break;
      case 203: switch((op=r8())) {
        X7(0) *p+=*p+(fc=(*p>>7)&1); goto FC;
        X7(8) *p=(fc=*p&1)*128+*p/2; goto FC;
        case 14: u=r(HL);fc=u&1;u=fc*128+u/2;w(HL,u);FS(0,u,0,0,fc); break;
        X7(16) fc=*p>>7;*p=*p*2+(F>>4)%2; goto FC;
        X7(24) fc=*p&1;*p=*p/2+((F*8)&128); goto FC;
        X7(32) fc=(*p>>7)&1;*p*=2; goto FC;
        X7(40) fc=*p&1;*p=(int8_t)*p>>1; goto FC;
        X7(48) FS(0,*p=*p*16+*p/16,0,0,0); break;
        case 54: u=r(HL);FS(0,u=u*16+u/16,0,0,0);w(HL,u); break;
        X7(56) fc=*p&1;*p/=2; FC: FS(0,*p,0,0,fc); break;
        Y8(70) u=r(HL); goto BITU;
        Y56(64) u=*p; BITU: FS(16,u&(1<<om/8),0,1,0); break;
        Y8(134) w(HL,r(HL)&~(1<<om/8)); break;
        Y56(128) *p&=~(1<<om/8); break;
        Y8(198) w(HL,r(HL)|(1<<om/8)); break;
        Y56(192) *p|=1<<om/8; break;
        default: return op;
      } break;
      case 224:case 226: w(65280+(op==224?r8():*R),A); break;
      case 233: PC=HL; break;
      case 234: w(r16(),A); break;
      case 240:case 250: A=r(op==240?65280|r8():r16()); break;
      case 243:case 251: IME=op!=243; break;
      case 248: u=r8();FS(0,1,0,(u8)SP+u>255,SP%16+u%16>15);HL=SP+(int8_t)u;T(); break;
      case 249: SP=HL;T(); break;
      default: return op;
    }
    for(DIV+=cy-bcy;bcy++!=cy;)
      if (LCDC&128) {
        if (++dot==1&&LY==144) IF|=1;
        if (dot==456) {
          if (LY<144) for (int x=160;--x>=0;) {
            u8 win=LCDC&32&&LY>=io[330]&&x>=io[331]-7,
              mx=win?x-io[331]+7:x+io[323], my=win?LY-io[330]:LY+io[322];
            u16 tile=vram[((LCDC&(win?64:8)?7:6)<<10)+my/8*32+mx/8],p=327;
            mx=(mx^7)&7;
            u8* d=&vram[(LCDC&16?tile:256+(int8_t)tile)*16+my%8*2],
              c=(d[1]>>mx)%2*2+(*d>>mx)%2;
            if (LCDC&2) for (u8* o=io; o<io+160; o+=4) {
              u8 dx=x-o[1]+8,dy=LY-*o+16;
              if (dx<8&&dy<8) {
                dx^=o[3]&32?0:7;
                d=&vram[o[2]*16+(dy^(o[3]&64?7:0))*2];
                u8 nc=(d[1]>>dx)%2*2+(*d>>dx)%2;
                if (!((o[3]&128)&&c)&&nc) { c=nc; p+=1+!!(o[3]&8); break; }
              }
            }
            fb[LY*160+x]=pal[(io[p]>>(2*c))&3];
          }
          if (LY==144) {
            void* pixels; int pitch;
            SDL_LockTexture(St, 0, &pixels, &pitch);
            for (int y=144;--y>=0;) memcpy((u8*)pixels+y*pitch, fb+y*160, 640);
            SDL_UnlockTexture(St);
            SDL_RenderCopy(Sr, St, 0, 0);
            SDL_RenderPresent(Sr);
            SDL_Delay(0);
            SDL_Event e;
            while (SDL_PollEvent(&e)) if (e.type==SDL_QUIT) return 0;
          }
          LY=(LY+1)%154; dot=0;
        }
      } else {
        LY=dot=0;
      }
  }
}
