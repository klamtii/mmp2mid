#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <mem.h>

#include "mmp.h"

struct MMP_TRACK *main_track=NULL;
struct MMP_TRACK *bassline_track=NULL;

struct MMP_GLOBALS globals;
struct MMP_CHANNEL main_channel,mixchannel[MAX_FXCHANNELS];

struct MMP_CONNECTION *connections=NULL;
struct MMP_LFO *lfos=NULL;
unsigned midichannels_needed=0;
long end_time=0;
unsigned basslines=0;

/*Wrappers for allocation*/
static void _mmp_free(void *ptr)
{
 free(ptr);
}

static void *_mmp_alloc(size_t len)
{
 return malloc(len);
}

static void *_mmp_calloc(size_t len)
{
 return calloc(len,1);
}

static void *_mmp_realloc(void *ptr,size_t len)
{
 return realloc(ptr,len);
}


static char *_mmp_strdup(const char *ptr)
{
 return strdup(ptr);
}



/*alloc pattern structture, and add it LAST to linked list pointed to by **list*/
struct MMP_PATTERN *mmp_alloc_pattern(struct MMP_PATTERN **list)
{
 struct MMP_PATTERN *r=NULL,*tmp;
 r=_mmp_calloc( sizeof(struct MMP_PATTERN) );
 if(r==NULL) return NULL;
 r->next=NULL;
 r->note=NULL;
 
 if(list==NULL) return r;

 if(*list==NULL) *list=r;
 else
 {
  tmp=*list;
  while(tmp->next!=NULL) 
   tmp=tmp->next;
  tmp->next=r;
  r->bassline=tmp->bassline+1;
 }     
 return r;
}


#define NOTE_TO_REALOC 10

/*add NOTE to note-array in pattern pointerd to by *p, and update pattern*/
struct MMP_NOTE *mmp_add_note(struct MMP_PATTERN *p)
{
 if(p==NULL) return NULL;
 if(p->note==NULL)
 {
  p->note=_mmp_alloc( sizeof(struct MMP_NOTE) * NOTE_TO_REALOC);
  if(p->note==NULL) return NULL;
  p->_max_entries=NOTE_TO_REALOC;
  p->entries=0;
 }
 
 if(p->entries>=p->_max_entries)
 {
  struct MMP_NOTE *tmp= _mmp_realloc(p->note, sizeof(struct MMP_NOTE) * (p->entries + NOTE_TO_REALOC));
  if(tmp==NULL) return NULL;
  p->note=tmp;
  p->_max_entries= p->entries + NOTE_TO_REALOC-1;
 }
 
 memset(&(p->note[p->entries]),0,sizeof(struct MMP_NOTE) );
 
 return ( &(p->note[ (p->entries) ++ ]) );
}

/*alloc track-structure, and add it LAST to linked **list */
struct MMP_TRACK *_mmp_alloc_track(struct MMP_TRACK **list)
{
 struct MMP_TRACK *r=NULL,*tmp;
 r=_mmp_calloc( sizeof(struct MMP_TRACK) );
 if(r==NULL) return NULL;
 r->next=NULL;
 r->pattern=NULL;
 
 if(list==NULL) return r;

 if(*list==NULL) *list=r;
 else
 {
  tmp=*list;
  while(tmp->next!=NULL) 
   tmp=tmp->next;
  tmp->next=r;
 }     
 return r;
}

/*wrappers to directly att track to main/bassline */
struct MMP_TRACK *mmp_alloc_main_track(void)
{
 return _mmp_alloc_track(&main_track);
}

struct MMP_TRACK *mmp_alloc_bassline_track(void)
{
 return _mmp_alloc_track(&bassline_track);
}

/*free pattern chain pointed to by *p (deletes tail of list !) */
int mmp_free_pattern(struct MMP_PATTERN *p)
{
 if(p==NULL) return -1;
 if(p->next!=NULL) mmp_free_pattern(p->next);
 if(p->note!=NULL) _mmp_free(p->note);
 _mmp_free(p);
 return 0;   
}

/*free track-chaein pointed to by *p (deletes tail of list !)*/
int mmp_free_track(struct MMP_TRACK *t)
{
 if(t==NULL) return -1;
 if(t->next!=NULL) mmp_free_track(t->next);
 if(t->pattern!=NULL) mmp_free_pattern(t->pattern);
 _mmp_free(t);
 return 0;   
}

/*alloc new connection and adds it to global list
  connections are used for both LFO's and Automation-Tracks*/
struct MMP_CONNECTION *mmp_alloc_conection(const char *name)
{
 struct MMP_CONNECTION *r=NULL;
 if(name==NULL) return NULL;
 
 r=connections; 
 while(r!=NULL)
 {
  if(!strcmp(r->name,name)) break;
  r=r->next;
 }             
 if(r==NULL)
 {
  if(connections==NULL) r=connections=_mmp_calloc(sizeof(struct MMP_CONNECTION));
  else
  {
   r=connections; 
   while(r->next!=NULL) 
    r=r->next;
   r->next=_mmp_calloc(sizeof(struct MMP_CONNECTION));
   r=r->next;
  }
  if(r!=NULL)
  {
   r->name=_mmp_strdup(name);
  }
 }

 return r; 
}

/*free pattern chain pointed to by *p (deletes tail of list !) */
int mmp_free_connection(struct MMP_CONNECTION *c)
{
 if(c==NULL) return -1;
 if(c->next!=NULL) mmp_free_connection(c->next);
 if(c->name!=NULL) _mmp_free(c->name);
 _mmp_free(c);
 return 0;   
}


/* 0-127, 100 is normal */
/*get volume for track, takes many automatable knobs into account*/
int mmp_track_final_volume(struct MMP_TRACK *t)
{
 float r=100.0;
 if(t==NULL) return r;
 
 r=t->vol*t->gain;
 r*=mixchannel[t->fxch].volume;
 r*=main_channel.volume;
 if(r<0.0) r=0.0;
 if(r>110.0) r=(r/2.0)+55.0;
 if(r>127.0) r=127.0;

 return (int)r;    
}

/*get panning for track. Takes note's individual pan into account */
/*Note, individual panning notes is a LMMS feature not supported by MIDI.
 wich keeps Paning as a channel setting*/
int mmp_track_final_panning(struct MMP_TRACK *t,int note_pan)
{
 float r=64.0;
 if(t==NULL) return (int)r;
 
 note_pan &= ~(int)3;
 
 r=t->pan + (float) note_pan;
 
 r=r*64.0/100.0;
 if(r<-63.0) r=-63.0;
 if(r>63.0) r=63.0;
 r+=64.0;

 return  (((int)r)&0x7F);    
}

/*Gets pitch from channel. Like panning, this is a channel-setting.
 but is this in both MMP and MIDI, so microtonal music needs
 some workaround (but is possible)*/
int mmp_track_final_pitch(struct MMP_TRACK *t)
{
 float r=8192.0;
 if(t==NULL) return (int)r;
  
 r=t->pitch;
 
 r=r*8192.0/100.0;
 if(r<-8192.0) r=-8192.0;
 if(r>8191.0) r=8191.0;
 r+=8192.0;

 return  (int) r;    
}

/*recovers MMP-track from MIDI-CHannel (NULL on failure)*/
struct MMP_TRACK *mmp_track_from_midichan(int chan)
{
 struct MMP_TRACK *r=NULL;
 for(r=main_track;r!=NULL;r=r->next)
  if(r->midichannel==chan) return r;
 for(r=bassline_track;r!=NULL;r=r->next)
  if(r->midichannel==chan) return r;
 return NULL; 
}

/*alloc LFO-Structure*/
struct MMP_LFO *mmp_alloc_lfo(void)
{
 struct MMP_LFO *r=NULL;
 r=_mmp_calloc(sizeof(struct MMP_LFO));
 if(r==NULL) return NULL;

 r->next=lfos;
 lfos=r;

 return r;      
}

/*frees LFO and deletes chain's tails*/
int mmp_free_lfos(void)
{
 struct MMP_LFO *lfo1=NULL;
 while(lfos!=NULL)
 {
  lfo1=lfos;
  lfos=lfo1->next;
  _mmp_free(lfo1);
 }  
 return 0;
}

static int _lfo_rand(void)
{
 volatile static int a,b,c;
 a++;
 b+=a;
 c+=b;
 return (((a*3+b*5+c*7)>>6) & 0xFF);
}


static float _lfo_func(float x,int wave)
{
 switch(wave)
 {
  case 0:  /*sine  (actually parabolic to avoid linking to math.h)*/
   if(x>0.5) return -_lfo_func(x-0.5,wave);
   x-=0.25;
   x*=4.0;
   return 1.0-(x*x);
  case 1: /*triangle*/
   if(x>0.5) return -_lfo_func(x-0.5,wave);
   x-=0.25;
   x*=4.0;
   return 1.0-abs(x);
  case 2:  /*saw*/
   return (x-0.5)*2;
  case 3:  /*square*/
   if(x>0.5) return -_lfo_func(x-0.5,wave);
   return 1.0;
  case 4:  /*Moog (triangle with jump)*/
   if(x>0.25) x+=0.25;
   x=(x*4.0)/5.0;
   return _lfo_func(x,1);
  case 5:  /*exponetian puls (actually x^3 so avoid linking to math.h)*/
   if(x>0.51) return _lfo_func(1.0-x,wave);
   x*=2.0;
   x*=x; x*=x; x*=x;
   x=(x*2)-1.0;
   return x;
  case 6:  /*Random (is this actually usefull?)*/
   return  (((float)(_lfo_rand())/127.5)-1.0);
  default:
   return 0;       
 }     
}

/*get LFO-Val after n seconds (based on time, not ticks !)*/
float mmp_read_lfo(struct MMP_LFO *lfo,float seconds)
{
 if(lfo==NULL) return 0.0;
 float x,y;
 
 x=seconds/lfo->speed;
 switch(lfo->multiplier)
 {
  case 1: x*=100.0; break;
  case -1: x/=100.0; break;
 }
 x+=lfo->phase/360.0;
 x-= ((float)( (unsigned long) x  ));
 
 y=_lfo_func(x,lfo->wave);

 y*=lfo->amount;

 y+=lfo->base;
 
 return y;      
}


/*clean up all data used by mmp datastructures*/
int mmp_free_all(void)
{
 mmp_free_track(main_track);
 mmp_free_track(bassline_track);
 mmp_free_connection(connections);    
 mmp_free_lfos();
 midichannels_needed=0;
 basslines=0;
 end_time=0;
}

/*find longest pattern in a bassline (slow)*/
unsigned long mmp_get_loop_len(int bassline)
{
 int a,b;
 struct MMP_TRACK *track=NULL;
 struct MMP_PATTERN *pattern=NULL;
 unsigned long r=192; 
 
 if(globals.clk_z&&globals.clk_n)
  r=(192*globals.clk_z)/globals.clk_n;

 for(track=bassline_track;track!=NULL;track=track->next)
 {   
  for(pattern=track->pattern;pattern!=NULL;pattern=pattern->next)
  {
   if(pattern->bassline==bassline)
   {
    if(pattern->len>r) r=pattern->len;
   }
  }
 }       

 return r;         
}


