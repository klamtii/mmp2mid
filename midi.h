
struct MIDI_BLOCK
{
 unsigned long len;
 char name[8];
 unsigned char *data;
 int _maxlen;
 int lasttime;
 int lastcommand;
};

struct MIDI_HELD_NOTE
{
 struct MIDI_HELD_NOTE *next;
 int time;
 int volume;
 int key;
 int pan;
};

struct MIDI_CHANNEL
{
 struct MIDI_BLOCK *block;
 struct MIDI_HELD_NOTE *held_note;
 int volume;
 int panning;
 int pitch;
 int patch;
 int bank;       
 int block_is_unique;
};

#define MAX_MIDICHANNELS 16


struct MIDI_CHANNEL midichannel[MAX_MIDICHANNELS];
struct MIDI_BLOCK *(midiblock[MAX_MIDICHANNELS]);
unsigned n_midichannels,n_midiblocks;
unsigned long midi_time;
int cue_happened;

int midi_running_status;

int midichannel_drum;

#define DRUMCHANNEL  9

int midi_free_block(struct MIDI_BLOCK *b);
struct MIDI_BLOCK *midi_alloc_block(const char *name);
int midi_add_bytes(struct MIDI_BLOCK *b,const unsigned char *dat,unsigned long len);
int midi_add_1byte(struct MIDI_BLOCK *b,unsigned char dat);
int midi_add_vint(struct MIDI_BLOCK *b, unsigned int dat);
int midi_write_block(FILE *file,struct MIDI_BLOCK *b);
struct MIDI_HELD_NOTE *midi_add_note(struct MIDI_CHANNEL *ch);
int midi_del_1note(struct MIDI_HELD_NOTE *note,struct MIDI_CHANNEL *ch);
int midi_del_notes(struct MIDI_CHANNEL *ch);
int midi_set_channels(int channels, int multitrack);
int midi_begin_timeslot(unsigned long time);
int midi_end_timeslot(void);
int _midi_delta_time(unsigned channel);
int midi_add_comment(unsigned channel,const char *txt,unsigned type);
int midi_set_bpm(unsigned channel,int bpm);
int midi_set_timesig(unsigned channel,unsigned num,unsigned det,unsigned quaverticks);
int midi_set_volume(unsigned channel,int vol);
int midi_set_panning(unsigned channel,int pan);  /*64 is neutral*/
int midi_set_pitch(unsigned channel,int pan);
int midi_set_instrument(unsigned channel,int bank,int patch);
int midi_add_terminator(unsigned channel);
int midi_check_notes(unsigned channel);
int midi_play_note(unsigned channel,int key,int volume,int len,int pan);
int midi_clear_channel(unsigned channel);
