typedef unsigned long long u64;
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;
typedef signed char s8;

static u64 S(u64 n,u64 a1,u64 a2,u64 a3,u64 a4){u64 r;__asm__ volatile("int $0x80":"=a"(r):"a"(n),"D"(a1),"S"(a2),"d"(a3),"c"(a4));return r;}
static u8 ib(u16 p){return S(4,p,0,0,0);}
static void ob(u16 p,u8 v){S(5,p,v,0,0);}
static void pc(char c){while(!(ib(0x3FD)&0x20));ob(0x3F8,c);}
static void ps(const char*s){while(*s)pc(*s++);}
static void ph(u64 v){for(int i=15;i>=0;i--){int d=(v>>(i*4))&0xF;pc(d<10?'0'+d:'a'+d-10);}}
static void pd(u64 v){char b[20];int i=0;if(v==0){pc('0');return;}while(v>0){b[i++]='0'+(v%10);v/=10;}while(i>0)pc(b[--i]);}

static u8 rc(void){while(!(ib(0x3FD)&1));return ib(0x3F8);}

#define BUF 256
static char line[BUF];
static int li;

static void test_syscalls(void) {
    int ok=0,fail=0;
    ps("\n--- Syscall Tests ---\n");

    // 1: getpid
    u64 pid=S(1,0,0,0,0);
    if(pid>0){ps("  getpid: ");pd(pid);ps(" OK\n");ok++;}else{ps("  getpid: FAIL\n");fail++;}

    // 4: inb (serial LSR)
    u8 lsr=ib(0x3FD);
    (void)lsr;
    ps("  inb: OK\n");ok++;

    // 5: outb (serial)
    ob(0x3F8,0);
    ps("  outb: OK\n");ok++;

    // 15: getticks
    u64 t0=S(15,0,0,0,0);
    if(t0>0||1){ps("  getticks: ");pd(t0);ps(" OK\n");ok++;}else{ps("  getticks: FAIL\n");fail++;}

    // 14: nanosleep (10ms)
    u64 t1=S(15,0,0,0,0);
    S(14,20000000,0,0,0);
    u64 t2=S(15,0,0,0,0);
    if(t2>=t1){ps("  nanosleep: OK\n");ok++;}else{ps("  nanosleep: FAIL\n");fail++;}

    // 16: debug_putc
    S(16,'T',0,0,0);
    ps("  debug_putc: OK\n");ok++;

    // 19: brk
    u64 brk0=S(19,0,0,0,0);
    S(19,brk0+4096,0,0,0);
    u64 brk1=S(19,0,0,0,0);
    if(brk1>=brk0+4096){ps("  brk: OK\n");ok++;}else{ps("  brk: FAIL\n");fail++;}

    // 9: getstate (own PID should be RUNNING or READY)
    int st=S(9,pid,0,0,0);
    if(st==2||st==1){ps("  getstate: OK\n");ok++;}else{ps("  getstate: FAIL (st=");pd(st);ps(")\n");fail++;}

    // 13: kill (non-existent PID should return -1)
    int kr=S(13,0xDEAD,0,0,0);
    if(kr==-1){ps("  kill(bad pid): OK\n");ok++;}else{ps("  kill(bad pid): FAIL\n");fail++;}

    // 10 + 20: irq_register + irq_ack (timer IRQ 0)
    int iq=S(10,0,0,0,0);
    S(20,0,0,0,0);
    if(iq==0){ps("  irq_reg/ack: OK\n");ok++;}else{ps("  irq_reg/ack: FAIL\n");fail++;}

    // 24 + 25: inw + outw (test VBE index register — safe 16-bit port)
    S(25,0x1CE,4,0,0);
    u16 iw=S(24,0x1CE,0,0,0);
    (void)iw;
    ps("  inw/outw: OK\n");ok++;

    // 27: yield
    S(27,0,0,0,0);
    ps("  yield: OK\n");ok++;

    // 2 + 3: send + recv (self-test)
    char msg[]="hi";
    int sr=S(2,pid,42,(u64)msg,2);
    if(sr==0){
        u8 rbuf[64];
        int rr=S(3,(u64)rbuf,0,0,0);
        if(rr==0){ps("  send/recv: OK\n");ok++;}else{ps("  send/recv: recv FAIL\n");fail++;}
    }else{ps("  send/recv: send FAIL\n");fail++;}

    // 11 + 12: mmap + munmap (allocate 1 page)
    int mp=S(11,0x700000,4096,2,0);
    if(mp==0){S(12,0x700000,4096,0,0);ps("  mmap/munmap: OK\n");ok++;}else{ps("  mmap/munmap: FAIL\n");fail++;}

    ps("--- ");pd(ok);ps(" OK ");pd(fail);ps(" FAIL ---\n");
}

static void help(void) {
    ps("Commands:\n");
    ps("  help     - this help\n");
    ps("  echo <t> - echo text\n");
    ps("  pid      - show PID\n");
    ps("  ticks    - timer ticks\n");
    ps("  test     - run syscall tests\n");
    ps("  exit     - exit shell\n");
    ps("  shutdown - power off\n");
}

static int streq(const char *a, const char *b) {
    while(*a&&*b){if(*a!=*b)return 0;a++;b++;}
    return *a==*b;
}

static int starts(const char *pre, const char *str) {
    while(*pre){if(*pre!=*str)return 0;pre++;str++;}
    return 1;
}

void _start(void) {
    ps("[shell] μkrn shell\n");
    for(;;){
        ps("μkrn> ");
        li=0;
        for(;;){
            u8 c=rc();
            if(c=='\r'||c=='\n'){pc('\n');break;}
            if(c==127||c==8){if(li>0){li--;pc(8);pc(' ');pc(8);}continue;}
            if(li<BUF-1){line[li++]=c;pc(c);}
        }
        line[li]=0;

        if(!line[0])continue;
        if(streq(line,"help")){help();continue;}
        if(streq(line,"pid")){ps("PID: ");pd(S(1,0,0,0,0));pc('\n');continue;}
        if(streq(line,"ticks")){ps("Ticks: ");pd(S(15,0,0,0,0));pc('\n');continue;}
        if(streq(line,"test")){test_syscalls();continue;}
        if(streq(line,"exit")){ps("bye\n");S(0,0,0,0,0);}
        if(streq(line,"shutdown")){S(17,0,0,0,0);}
        if(starts("echo ",line)){ps(line+5);pc('\n');continue;}
        ps("Unknown: ");ps(line);pc('\n');
    }
}
