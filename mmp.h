
struct MMP_NOTE{
 long pos,len;
 int key,pan,vol;
 float val;
};

struct MMP_CONNECTION
{
 struct MMP_CONNECTION *next;
 int *intptr;
 float *floatptr,lfomul,lfobase;
 char *name;
};


struct MMP_PATTERN{
 struct MMP_PATTERN *next;
 unsigned long pos,len;
 int bassline;
 int active;
 int entries,_max_entries;
 int muted,type,frozen,steps;
 struct MMP_CONNECTION *connection;
 struct MMP_NOTE *note;
};

struct MMP_LFO
{
 struct MMP_LFO *next;
 struct MMP_CONNECTION *connection;
 float speed,base,syncmode,phase,amount,speed_numerator,speed_denominator;
 int type,wave,multiplier;
};


struct MMP_TRACK{
 struct MMP_TRACK *next;
 struct MMP_PATTERN *pattern;
 float vol,pan;
 float gain;       
 float pitch;
 unsigned long loop_len;
 int typ;
 int fxch;
 int midichannel;
 int bassline;
 int patch,bank;
 int key;
 int mute,solo;
};

struct MMP_GLOBALS
{
 float vol;
 int key;
 int bpm;
 int clk_z,clk_n;   
 int lp1pos,lp0pos,lpstate;
};

struct MMP_CHANNEL
{
 float volume;
 int muted;
};


#define MAX_FXCHANNELS 64


struct MMP_TRACK *main_track;
struct MMP_TRACK *bassline_track;

struct MMP_GLOBALS globals;
struct MMP_CHANNEL main_channel,mixchannel[MAX_FXCHANNELS];
struct MMP_CONNECTION *connections;
struct MMP_LFO *lfos;
unsigned midichannels_needed;
unsigned basslines;
long end_time;


struct MMP_PATTERN *mmp_alloc_pattern(struct MMP_PATTERN **list);
struct MMP_NOTE *mmp_add_note(struct MMP_PATTERN *p);
struct MMP_TRACK *_mmp_alloc_track(struct MMP_TRACK **list);
struct MMP_TRACK *mmp_alloc_main_track(void);
struct MMP_TRACK *mmp_alloc_bassline_track(void);
int mmp_free_pattern(struct MMP_PATTERN *p);
int mmp_free_track(struct MMP_TRACK *t);
struct MMP_CONNECTION *mmp_alloc_conection(const char *name);
int mmp_free_connection(struct MMP_CONNECTION *c);
int mmp_free_all(void);
int mmp_track_final_volume(struct MMP_TRACK *t);
int mmp_track_final_panning(struct MMP_TRACK *t,int note_pan);
int mmp_track_final_pitch(struct MMP_TRACK *t);
struct MMP_TRACK *mmp_track_from_midichan(int chan);
unsigned long mmp_get_loop_len(int bassline);


struct MMP_LFO *mmp_alloc_lfo(void);
int mmp_free_lfos(void);
float mmp_read_lfo(struct MMP_LFO *lfo,float seconds);
