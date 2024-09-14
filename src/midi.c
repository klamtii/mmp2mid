#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <mem.h>

#include "midi.h"

/*
----------
Low-level MIDI functions. Stores Data in Tracks and can read tracks to file.
----------
*/

#ifndef BUFSIZ
#define BUFSIZ 512
#endif

/*state of MIDIchannels 0-15 (note, channel 9 is Drums)*/
struct MIDI_CHANNEL midichannel[MAX_MIDICHANNELS];
/*Buffer for MIDI-tracks might be the number of channels or a single one*/
struct MIDI_BLOCK *(midiblock[MAX_MIDICHANNELS]);
unsigned n_midichannels,n_midiblocks;

/*current time in ticks (48 ticks pert quaver for lmms-projects)*/
unsigned long midi_time=0;
/*should repetive MIDI-commands be omited ?*/
int midi_running_status=1;
/*should reduntant controller settings be omitted (like setting volume twice to same value)*/
int midi_avoid_repeat=1;
/*has an automationtrack changed something this tick*/
int cue_happened=0;
/*wich midichannel in mmp data will be mapped to 9*/
int midichannel_drum=-1;


/*Wrappers for allocation*/
static void _midi_free(void *ptr)
{
 free(ptr);
}

static void *_midi_alloc(size_t len)
{
 return malloc(len);
}

static void *_midi_calloc(size_t len)
{
 return calloc(len,1);
}

static void *_midi_realloc(void *ptr,size_t len)
{
 return realloc(ptr,len);
}


/*Free Midi Block storing track*/
int midi_free_block(struct MIDI_BLOCK *b)
{
 if(b==NULL) return -1;
 if(b->data!=NULL)
  _midi_free(b->data);
 b->data=NULL;  
 _midi_free(b);
 return 0;    
}

/*Alloc Midi Block storing track*/
struct MIDI_BLOCK *midi_alloc_block(const char *name)
{
 int tmp;
 struct MIDI_BLOCK *b;
 b=_midi_calloc(sizeof(struct MIDI_BLOCK));
 if(b==NULL) return NULL;
 
 tmp=BUFSIZ;
 b->data=_midi_alloc(tmp);
 if(b->data!=NULL)
  b->_maxlen=tmp;
 
 if(name!=NULL)
 { 
  for(tmp=0;(tmp<4)&&name[tmp];tmp++) 
   b->name[tmp]=name[tmp];  
 }
 
 return b;    
}

/*add raw bytes to Midi Track*/
int midi_add_bytes(struct MIDI_BLOCK *b,const unsigned char *dat,unsigned long len)
{
 int tmp;
 if(b==NULL) return -1;
 if(len<1) return 0;
 if(b->len+len>=b->_maxlen)
 {
  tmp=b->_maxlen+len;
  tmp=(tmp/BUFSIZ+1)*BUFSIZ;
  b->data=_midi_realloc(b->data,tmp);
  b->_maxlen=tmp;
 }
 memcpy( &(b->data[b->len]) ,dat,len);
 b->len+=len;
 return 0;
}

/*add single byte to track*/
int midi_add_1byte(struct MIDI_BLOCK *b,unsigned char dat)
{
 return midi_add_bytes(b,&dat,1);   
}


static int _midi_add_vint2(struct MIDI_BLOCK *b, unsigned int dat)
{
 unsigned int tmp;
 
 tmp=dat>>7;
 dat&=0x7F;    
 if(tmp)
  _midi_add_vint2(b,tmp); 
 return midi_add_1byte(b,dat|0x80);
}


/*add variable-with intteger to MIDI-track*/
int midi_add_vint(struct MIDI_BLOCK *b, unsigned int dat)
{
 unsigned int tmp;
 
 tmp=dat>>7;
 dat&=0x7F;    
 if(tmp)
  _midi_add_vint2(b,tmp); 
 return midi_add_1byte(b,dat);
}

/*Write MIDI-Track to File (plus trackheader)*/
int midi_write_block(FILE *file,struct MIDI_BLOCK *b)
{
 int a;
 unsigned long temp;
 if(file==NULL||b==NULL) return -1;
 fwrite(b->name,1,4,file);
 temp=b->len;
 for(a=24;a>=0;a-=8)
  putc((temp>>a)&0xFF,file);
 if(b->len>0)
  fwrite(b->data,1,b->len,file);  
 return 0;    
}


/*add held-note structure to MIDI-Channel (to keep track of note currently playing)*/
struct MIDI_HELD_NOTE *midi_add_note(struct MIDI_CHANNEL *ch)
{
 struct MIDI_HELD_NOTE *r;
 if(ch==NULL) return NULL;
 
 r=_midi_calloc(sizeof(struct MIDI_HELD_NOTE));
 
 if(r==NULL) return NULL;
 r->next=ch->held_note;
 ch->held_note=r;

 return r;  
}

/*remove Held-Note structure from channel*/
int midi_del_1note(struct MIDI_HELD_NOTE *note,struct MIDI_CHANNEL *ch)
{
 struct MIDI_HELD_NOTE *note2=NULL;
 if(note==NULL) return -1;
 
 if(ch!=NULL)
 {
  if(ch->held_note==note)
   ch->held_note=note->next;
  else
  {
   for(note2=ch->held_note;note2!=NULL;note2=note2->next)
    if(note2->next==note) break;                  
   if(note2!=NULL)
    note2->next=note->next;
  }
 }
 _midi_free(note);

 return 0;  
}

/*free all held notes from channel*/
int midi_del_notes(struct MIDI_CHANNEL *ch)
{
 if(ch==NULL) return -1;
 while(ch->held_note!=NULL)
  midi_del_1note(ch->held_note,ch);

 return 0;  
}

/*set number of MIDI-Channels. Clears them all. Set to 0 to clean up*/
int midi_set_channels(int channels, int mtrack)
{
 int a;

 if(channels<0) channels=0;   
 if(channels>16) return -1;
 
 for(a=0;a<n_midiblocks;a++)
 {
  if(midiblock[a]!=NULL)
  {
   midi_free_block(midiblock[a]);
   midiblock[a]=NULL;
  }
 }  
 n_midiblocks=0;

 for(a=0;a<n_midichannels;a++)
 {
  if(midichannel[a].held_note!=NULL)
   midi_del_notes(&(midichannel[a]));   
 }  
 n_midichannels=0;

 n_midiblocks=mtrack?channels:1;

 for(a=0;a<n_midiblocks;a++)
  midiblock[a]=midi_alloc_block("MTrk");
 
 n_midichannels=channels;
 for(a=0;a<n_midichannels;a++)
 {
  memset(&(midichannel[a]),0,sizeof(struct MIDI_CHANNEL));
  midichannel[a].volume=-1;
  midichannel[a].panning=-1;
  midichannel[a].pitch=-1;
  midichannel[a].patch=-1;
  midichannel[a].bank=-1;    

  midichannel[a].block=midiblock[mtrack?a:0];
 } 

 return 0;
}

/*theese two actually don't do anything yet ...*/

/*Betgin MIDI-timeslot*/
int midi_begin_timeslot(unsigned long time)
{
 midi_time=time;
}

/*End Midi-timeslot*/
int midi_end_timeslot(void)
{
 ;
}

/* Map drums to channel 9. and channel number 9 to the channel with the drums*/
static int midi_translate_channel(int channel)
{
 if(channel==midichannel_drum) return DRUMCHANNEL;
 if(midichannel[channel].bank>=120) return DRUMCHANNEL;
 if(channel==DRUMCHANNEL) return midichannel_drum;
 return channel;      
}

/*encode time passed since last command in this track.
  every command begins with this*/
int _midi_delta_time(unsigned channel)
{
 int temp;
 struct MIDI_BLOCK *blk=NULL;
 if(channel>=n_midichannels) return -1;
 blk=midichannel[channel].block;
 if(blk==NULL) return -1;
 
 temp=midi_time-blk->lasttime;

 midi_add_vint(blk,temp); 
 blk->lasttime=midi_time;
 
 return 0;
}


/* type: 1 - text, 2 - Copyright, 3 - trackname, 4 - Instrument name,
 5 - lyric, 6 - marker, 7 - cue point*/
int midi_add_comment(unsigned channel,const char *txt,unsigned type)
{
 struct MIDI_BLOCK *blk=NULL;
 unsigned len=0;
 if(channel>=n_midichannels) return -1;
 if(txt==NULL) return -1;
 if(type<1||type>7) return -1;
 blk=midichannel[channel].block;
 if(blk==NULL) return -1;

 _midi_delta_time(channel);
 midi_add_1byte(blk,0xFF);
 midi_add_1byte(blk,type);
 blk->lastcommand=0xFF;
 
 len=strlen(txt);
 
 midi_add_vint(blk,len);
 midi_add_bytes(blk,txt,len);
 
 return 0;
}

/*set bpm*/
int midi_set_bpm(unsigned channel,int bpm)
{
 struct MIDI_BLOCK *blk=NULL;
 if(channel>=n_midichannels) return -1;
 if(bpm<1) return -1;
 blk=midichannel[channel].block;
 if(blk==NULL) return -1;

 _midi_delta_time(channel);
 midi_add_bytes(blk,"\xFF\x51\x3",3);
 
 /*to microseconds per quarternote ...*/
 bpm=(int)  ( 60000000. / ((float)bpm) );
 midi_add_1byte(blk, (bpm>>16) & 0xFF );
 midi_add_1byte(blk, (bpm>>8) & 0xFF );
 midi_add_1byte(blk, (bpm) & 0xFF );
 blk->lastcommand=0xFF;

 return 0;
}

/*time-signature. Note: denominator must be power of 2*/
int midi_set_timesig(unsigned channel,unsigned num,unsigned det,unsigned quaverticks)
{
 struct MIDI_BLOCK *blk=NULL;
 unsigned log_det=0;
 
 if(channel>=n_midichannels) return -1;
 if(det<1||num<1) return -1;
 blk=midichannel[channel].block;
 if(blk==NULL) return -1;
 
 while( (det>>=1) ) log_det++;

 _midi_delta_time(channel);
 midi_add_bytes(blk,"\xFF\x58\x4",3);
 midi_add_1byte(blk, num );
 midi_add_1byte(blk, log_det );
 midi_add_1byte(blk, quaverticks /*24 normal 48 LMMS*/ );
 midi_add_1byte(blk, 8 );
 blk->lastcommand=0xFF;

 return 0;
}

/* set volume 0-127 */
int midi_set_volume(unsigned channel,int vol)
{
 struct MIDI_BLOCK *blk=NULL;
 if(channel>=n_midichannels) return -1;
 blk=midichannel[channel].block;
 if(blk==NULL) return -1;
 
 if(vol<0) vol=0;
 if(vol>127) vol=127;

 if( (!midi_avoid_repeat) || (midichannel[channel].volume!=vol) )
 {
  _midi_delta_time(channel);
  midi_add_1byte(blk, blk->lastcommand=(0xB0+midi_translate_channel(channel)) );
  midi_add_1byte(blk, 0x07 );
  midi_add_1byte(blk, vol );
  midichannel[channel].volume=vol;
 } 

 return 0;
}

/*Set panning*/
int midi_set_panning(unsigned channel,int pan)  /*64 is neutral*/
{
 struct MIDI_BLOCK *blk=NULL;
 if(channel>=n_midichannels) return -1;
 blk=midichannel[channel].block;
 if(blk==NULL) return -1;
 
 if(pan<0) pan=0;
 if(pan>127) pan=127;

 if( (!midi_avoid_repeat) || (midichannel[channel].panning!=pan) )
 {
  _midi_delta_time(channel);
  midi_add_1byte(blk, blk->lastcommand=(0xB0+midi_translate_channel(channel)) );
  midi_add_1byte(blk, 0x0A );
  midi_add_1byte(blk, pan );
  midichannel[channel].panning=pan;
 }
 
 return 0;
}

/*Set pitch*/
int midi_set_pitch(unsigned channel,int pitch)  
{
 struct MIDI_BLOCK *blk=NULL;
 int temp;
 if(channel>=n_midichannels) return -1;
 blk=midichannel[channel].block;
 if(blk==NULL) return -1;
 
 if(pitch<0) pitch=0;
 if(pitch>0x3FFF) pitch=0x3FFF;


 if( (!midi_avoid_repeat) || (midichannel[channel].pitch!=pitch) )
 {
  _midi_delta_time(channel);
  temp=0xE0+midi_translate_channel(channel);
  if(blk->lastcommand!=temp)
   midi_add_1byte(blk, (blk->lastcommand=temp) );
  midi_add_1byte(blk, pitch & 0x7F);
  midi_add_1byte(blk, pitch >>  7 );
  midichannel[channel].pitch=pitch;
 }
 
 return 0;
}

/*set intrument to patch/bank*/
int midi_set_instrument(unsigned channel,int bank,int patch)
{
 struct MIDI_BLOCK *blk=NULL;
 if(channel>=n_midichannels) return -1;
 blk=midichannel[channel].block;
 if(blk==NULL) return -1;

 if(patch<0||patch>127) return -1; 

 if(bank>=0)
 {
  _midi_delta_time(channel);
  midi_add_1byte(blk, 0xB0+midi_translate_channel(channel) );
  midi_add_1byte(blk, 0x00 );
  midi_add_1byte(blk, bank>>7 );  

  _midi_delta_time(channel);
  /*midi_add_1byte(blk, 0xB0+midi_translate_channel(channel) );*/
  midi_add_1byte(blk, 0x20 );
  midi_add_1byte(blk, bank&0x7F );  
  midichannel[channel].bank=bank;
 }
 
 _midi_delta_time(channel);
 midi_add_1byte(blk, blk->lastcommand=(0xC0+midi_translate_channel(channel)) );
 midi_add_1byte(blk, patch );
 midichannel[channel].patch=patch;

 return 0;
}

/*add track terminator*/
int midi_add_terminator(unsigned channel)
{
 struct MIDI_BLOCK *blk=NULL;
 
 if(channel>=n_midichannels) return -1;
 blk=midichannel[channel].block;
 if(blk==NULL) return -1;
 
/* _midi_delta_time(channel);*/
 midi_add_bytes(blk,"\x00\xFF\x2F\x00",4);

 blk->lastcommand=0xFF;

 return 0;
}

/*check if notes have timed out, and send stop commands if needed*/
int midi_check_notes(unsigned channel)
{
 struct MIDI_BLOCK *blk=NULL;
 struct MIDI_HELD_NOTE *note=NULL;
 int temp;

 if(channel>=n_midichannels) return -1;
 blk=midichannel[channel].block;
 if(blk==NULL) return -1;
  
 for(note=midichannel[channel].held_note;note!=NULL;note=note->next)
  if(note->time<=midi_time)
  {

   _midi_delta_time(channel);
/*   temp=0x80 +midi_translate_channel(channel); */
   temp=0x90 +midi_translate_channel(channel); 
   if( (!midi_running_status) || (blk->lastcommand!=temp) )                          
    midi_add_1byte(blk, blk->lastcommand=temp);
   midi_add_1byte(blk, note->key);
/*   midi_add_1byte(blk, 64);  */
   midi_add_1byte(blk, 0);  
   
   midi_del_1note(note,midichannel+channel);
   note=midichannel[channel].held_note;
   if(note==NULL) break;
  }
 
 return 0;    
};

/*PLay a note*/
int midi_play_note(unsigned channel,int key,int volume,int len,int pan)
{
 struct MIDI_BLOCK *blk=NULL;
 struct MIDI_HELD_NOTE *note=NULL;
 int temp;

 if(channel>=n_midichannels) return -1;
 blk=midichannel[channel].block;
 if(blk==NULL) return -1;

 if(key<0||key>127||volume<0||volume>127) return -1; 

 
 for(note=midichannel[channel].held_note;note!=NULL;note=note->next)
  if(note->key==key) break;
 if(note==NULL)
  note=midi_add_note(midichannel+channel);
 note->key=key;
 note->volume=volume;
 note->time=midi_time+len;
 note->pan=pan;

 midi_check_notes(channel);

 _midi_delta_time(channel);
 temp=0x90 +midi_translate_channel(channel); 
 if( (!midi_running_status) || (blk->lastcommand!=temp) )
  midi_add_1byte(blk, blk->lastcommand=temp);
 midi_add_1byte(blk, key);
 midi_add_1byte(blk, volume);
 
 return 0;
}

/*stop all notes in a channel*/
int midi_clear_channel(unsigned channel)
{
 struct MIDI_BLOCK *blk=NULL;
 struct MIDI_HELD_NOTE *note=NULL;
 int temp;

 if(channel>=n_midichannels) return -1;
 blk=midichannel[channel].block;
 if(blk==NULL) return -1;
  
 for(note=midichannel[channel].held_note;note!=NULL;note=note->next)
  {
   _midi_delta_time(channel);
   temp=0x80 +midi_translate_channel(channel);
   if( (!midi_running_status) || (blk->lastcommand!=temp) )
    midi_add_1byte(blk, blk->lastcommand=temp);
   midi_add_1byte(blk, note->key);
   midi_add_1byte(blk, 64);  
  }

 midi_del_notes(midichannel+channel);
 
 return 0;        
};


