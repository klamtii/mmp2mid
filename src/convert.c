#include <stdlib.h>
#include <stdio.h>

#include "midi.h"
#include "mmp.h"
/*
---------------
High-Level Functions for MIDI-Output.
uses midi.c for i/o and mmp.c for the data-structures.
---------------
*/

/* is it a multitrack (type 1) MIDI file or singletrack (type 0)*/
int multitrack=1;

/*write changes for Volume, Panning an Pitch if they changed
  this is called before writing notes, or when notes are held
  , so we don't bloat the file with cues that don't affect 
   anything*/
int check_controls(struct MMP_TRACK *track,int note_pan)
{
 int temp;
 struct MIDI_CHANNEL *channel;
 if(track==NULL) return -1;
 if(track->midichannel<0) return -1;
 channel=&(midichannel[track->midichannel]);
   
 temp=mmp_track_final_volume(track);
 if(temp!=channel->volume)
  midi_set_volume(track->midichannel,temp);

 temp=mmp_track_final_panning(track,note_pan);
 if(temp!=channel->panning)
  midi_set_panning(track->midichannel,temp);

 temp=mmp_track_final_pitch(track);
/* printf("pitch %g %d ",track->pitch,temp);*/
 if(temp!=channel->pitch)
  midi_set_pitch(track->midichannel,temp);

 if(track->bank!=channel->bank|| track->patch!=channel->patch)
  midi_set_instrument(track->midichannel,channel->bank,channel->patch);
 
 return 0;    
}

/*do MIDI-Pattern*/
int proc_pattern(struct MMP_PATTERN *pattern, struct MMP_TRACK *track,long reltime)
{
 int a,b;
 struct MMP_NOTE *note=NULL;
 if(pattern==NULL|track==NULL) return -1;
 if(track->midichannel<0) return -1;
 note=pattern->note;
 for(a=pattern->entries;a>0;a--)
 {
  if(note->pos==reltime)
  {
   if(cue_happened)
    check_controls(track, note->pan);
                        
   /*printf("Note at %d\n",reltime);*/
   midi_play_note(track->midichannel,note->key+12+57-track->key,note->vol,note->len,note->pan);
  }
  note++;
 }  
}

/*Do automatisation-pattern*/
int proc_autopattern(struct MMP_PATTERN *pattern, struct MMP_TRACK *track,long reltime)
{
 int a,b;
 struct MMP_NOTE *note=NULL;
 struct MMP_CONNECTION *connection=NULL;
 if(pattern==NULL|track==NULL) return -1;
 if(track->midichannel>=0) return -1;
/* printf("autotrack...\n");*/
 connection=pattern->connection;
 if(connection==NULL) return -1;
 note=pattern->note;
 for(a=pattern->entries;a>0;a--)
 {
  if(note->pos==reltime)
  {
   printf("Cue %g ",note->val);
   if(connection->intptr!=NULL)
    *(connection->intptr)=note->vol;
   if(connection->floatptr!=NULL)
    *(connection->floatptr)=note->val;
   if((connection->intptr!=NULL) || (connection->floatptr!=NULL))
   {
    cue_happened=1;
    /*printf("cue at %d\n",reltime);*/
   }
  }
  note++;
 }  
}

/*do MIDI-track basline is 0-basedbassline-number (always 0 for main)*/
int proc_notetrack(struct MMP_TRACK *track,long reltime,int bassline)
{
 int a,b;
 struct MMP_PATTERN *pattern=NULL;
 if(track==NULL) return -1;
 if(track->midichannel<0) return -1;
/* printf("Processing Notetrack...\n");*/
 for(pattern=track->pattern;pattern!=NULL;pattern=pattern->next)
  if(pattern->bassline==bassline)
   if(pattern->pos<=reltime)
    if(pattern->pos+pattern->len>reltime)
     proc_pattern(pattern,track,reltime-pattern->pos);
}

/*do automatisation-track basline is 0-basedbassline-number (always 0 for main)*/
int proc_autotrack(struct MMP_TRACK *track,long reltime,int bassline)
{
 int a,b;
 struct MMP_PATTERN *pattern=NULL;
 if(track==NULL) return -1;
 if(track->midichannel>=0) return -1;
/* printf("Processing Autotrack...\n");*/
 for(pattern=track->pattern;pattern!=NULL;pattern=pattern->next)
  if(pattern->bassline==bassline)
   if(pattern->pos<=reltime)
    if(pattern->pos+pattern->len>reltime)
     proc_autopattern(pattern,track,reltime-pattern->pos);
}

/*do beat-bassline "pattern"*/
int proc_bbfco(struct MMP_PATTERN *pattern, struct MMP_TRACK *bbtrack,long reltime)
{
  struct MMP_TRACK *track;
  /*Automation in BB-Track*/  
  
  if(bbtrack->loop_len)
   reltime %= bbtrack->loop_len;
  
  for(track=bassline_track;track!=NULL;track=track->next)
  {
   if(track->midichannel>=0) continue;
   if(track->bassline>=0) continue;
   
   if(!track->mute)
    proc_autotrack(track,reltime,bbtrack->bassline);
  }

  /*Notes in BB-Track*/  
  for(track=bassline_track;track!=NULL;track=track->next)
  {
   if(track->midichannel<0) continue;
   if(track->bassline>=0) continue;
   
   if(!track->mute)
    proc_notetrack(track,reltime,bbtrack->bassline);
  }
 return 0;    
}
/*Do beat/Bassline track*/

int proc_bbtrack(struct MMP_TRACK *track,long reltime)
{
 int a,b;
 struct MMP_PATTERN *pattern=NULL;
 if(track==NULL) return -1;
 if(track->midichannel>=0) return -1;
/* printf("Processing Autotrack...\n");*/
 for(pattern=track->pattern;pattern!=NULL;pattern=pattern->next)
  if(pattern->pos<=reltime)
   if(pattern->pos+pattern->len>reltime)
    proc_bbfco(pattern,track,reltime-pattern->pos);
}


/*
....
    The Main Workhorse Routine
....
*/

int convert_mmp2mid(const char *destname,const char *srcname)
{
 int a,b,c;
 float f,seconds;
 FILE *file=NULL;
 unsigned long time;
 struct MMP_TRACK *track;
 struct MMP_LFO *lfo;
 
 /*free everything...*/
 mmp_free_all();
 midi_set_channels(0,0);

 file=fopen(srcname,"rt");
 if(file!=NULL) /*success*/
 {
  a=load_mmp(file);  
  fclose(file);
  if(a<0)
  {
   fprintf(stderr,"Error: Parsing of %s failed.\n",srcname);
   mmp_free_all();
   return -1;             
  }
 }
 else /*failure*/
 {
  fprintf(stderr,"Error: cannot open %s.\n",srcname);
  return -1;     
 }  
 
 if(midichannels_needed<1)
 {
   fprintf(stderr,"Error: Project %s has no MIDI-Tracks (SF2-Player).\n",srcname);
   mmp_free_all();
   return -1;                                       
 }
   
 midi_set_channels(midichannels_needed,multitrack);
 
 /*printf("end_time: %d\n",end_time);*/
 
 for(time=0;time<=end_time;time++)
 { 
  midi_begin_timeslot(time);
  
  cue_happened=0;
  
  if(!time)
  {
   /*init stuff*/
   
   /*Global*/
   midi_add_comment(0,"Converted with MMP2MID from",1);
   midi_add_comment(0,srcname,3);

   midi_set_timesig(0,globals.clk_z,globals.clk_n,48);
   midi_set_bpm(0,globals.bpm);

   for(a=0;a<midichannels_needed;a++)
   {
    track=mmp_track_from_midichan(a);
    if(track!=NULL)
     midi_set_instrument(a,track->bank,track->patch);
   }
   for(a=0;a<midichannels_needed;a++)
    midi_set_volume(a,mmp_track_final_volume(mmp_track_from_midichan(a)));
   for(a=0;a<midichannels_needed;a++)
    midi_set_panning(a,mmp_track_final_panning(mmp_track_from_midichan(a),0));
   for(a=0;a<midichannels_needed;a++)
    midi_set_pitch(a,mmp_track_final_pitch(mmp_track_from_midichan(a)));
  }
  
  /* Process LFOs*/
  seconds= (((float) time ) / ((float) (globals.bpm)) )*(60.0/48.0 ) ;
  for(lfo=lfos;lfo!=NULL;lfo=lfo->next)
  {
   struct MMP_CONNECTION *connection;
   f=mmp_read_lfo(lfo,seconds);
   connection=lfo->connection;
   if(connection)
   {
    if(connection->intptr!=NULL)
     *(connection->intptr)=(int) (f * ((float)connection->lfomul) + connection->lfobase);
    if(connection->floatptr!=NULL)
     *(connection->floatptr)=(f * ((float)connection->lfomul) + connection->lfobase);
    if((connection->intptr!=NULL) || (connection->floatptr!=NULL))
    {
     cue_happened=1;    
     /*printf("process LFO %g->%g",seconds,f);*/
    }
   }
  }
  
  /*Automation in Main Track*/  
  for(track=main_track;track!=NULL;track=track->next)
  {
   if(track->midichannel>=0) continue;
   if(track->bassline>=0) continue;
   
   if(!track->mute)
    proc_autotrack(track,time,0);
  }

  /*Beat/Bassline*/  
  for(track=main_track;track!=NULL;track=track->next)
  {
   if(track->midichannel>=0) continue;
   if(track->bassline<0) continue;
   
   if(!track->mute)
    proc_bbtrack(track,time);
  }
    
  /*Notes in Main Track*/  
  for(track=main_track;track!=NULL;track=track->next)
  {
   if(track->midichannel<0) continue;
   if(track->bassline>=0) continue;
   
   if(!track->mute)
    proc_notetrack(track,time,0);
  }
  
  /* Stop notes*/
  for(a=0;a<n_midichannels;a++)
  {
   if(midichannel[a].held_note!=NULL)
    midi_check_notes(a);      
  }
  if(!(time%6))
   for(a=0;a<n_midichannels;a++)
   {
     if(midichannel[a].held_note!=NULL)
      check_controls(mmp_track_from_midichan(a),midichannel[a].held_note->pan);
   }
  midi_end_timeslot();
 }
 
 
  for(a=0;a<n_midichannels;a++)
   midi_clear_channel(a);
 
 /*Write Terminators on tracks*/
 for(a=0;a<n_midiblocks;a++)
  midi_add_terminator(a);


 /*Write the File*/
 file=fopen(destname,"wb");
 if(file==NULL)
 {
  fprintf(stderr,"Error: cannot create %s.\n",destname);
  mmp_free_all();
  midi_set_channels(0,0);
  return -1;     
 }  
 else
 {
  fwrite("MThd\0\0\0\6\0",1,9,file);  
  putc(multitrack?1:0,file);
  putc(0,file);
  putc(n_midiblocks,file);
  putc(0,file);
  putc(/*89*/ 48,file);

  for(a=0;a<n_midiblocks;a++)
   midi_write_block(file,midiblock[a]);
 
  fclose(file);
 }
 return 0;   
}
