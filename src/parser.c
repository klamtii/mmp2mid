#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <mem.h>
#include <stdarg.h>

#include "xml.h"
#include "mmp.h"
#include "midi.h"

/*
-------------------------
High-Level Parser for the LMMS files.
Build in xml.c for i/o and mmp for the
data-structures
-------------------------
*/

int parser_column=0;
int parser_verbose=0;

/*If parser_verbose bigger than abs vlevel printf.
   idents. if vlevel is positive */
int parser_printf(int vlevel,const char *fstr,...)
{
  int a;
  va_list args;   
  
  if(abs(vlevel)>parser_verbose) return 0;
  if(vlevel>=0)
  {
   a=parser_column;
   while(a--) putchar(32);
  }
  va_start(args,fstr);
  vprintf(fstr,args);
  va_end(args);
}

/*allocs connetcion structure or finds structure with name
  sets iptr or fptr to automated value*/
int add_control_link(struct XML_CONTEXT *kontext,struct XML_TOKEN *token,
  int *iptr,float *fptr,float lfomul,float lfobase,const char *name,int vlevel)
{
 int r=0;
 int a,b;
 struct MMP_CONNECTION *con=NULL;

 if(token!=NULL)
 {
  parser_printf(vlevel,"%s: ",name);
  for(a=0;a<token->var_count;a++)
  {
   switch( xml_lookupname(token,"value\0id\0\0",a) )
   {
    case 1:
     if(fptr!=NULL)
      parser_printf(-1,"Value:%.02g ",(*fptr=xml_readvar_float(token,a)));
     if(iptr!=NULL)
      parser_printf(-1,"Value:%d ",(*iptr=xml_readvar_int(token,a)));
     break;
    case 2:
     parser_printf(-1,"ID:%s ",token->content[a]);     
     con=mmp_alloc_conection(token->content[a]);
     break;
   }
  }
  parser_printf(-vlevel,"\n");
  if(con==NULL) return -1;
  
  con->floatptr=fptr;
  con->intptr=iptr;
  con->lfomul=lfomul;
  con->lfobase=lfobase;
 }

 if(!(token->alone_token|token->close_token))
  xml_skiptoclose(kontext,token->variable[0]);
     
 return r;   
}

/*Read SF2-Structure or block*/
int parse_sf2player(struct XML_CONTEXT *kontext,int bassline,struct XML_TOKEN *sf2hdr,struct MMP_TRACK *track)
{
 int r=0;
 int a,b;
 float f;  

 struct XML_TOKEN *token;
 token=xml_alloc_token();
 
 track->midichannel=(midichannels_needed++);
 if(midichannels_needed==DRUMCHANNEL)
  midichannels_needed++;
  
 if(sf2hdr!=NULL)
 {
  parser_printf(1,"");
  for(a=0;a<sf2hdr->var_count;a++)
  {
   switch( xml_lookupname(sf2hdr,"patch\0bank\0gain\0\0",a) )
   {
    case 1:
     b=xml_readvar_int(sf2hdr,a);
     parser_printf(-1,"Patch:%d ",b);
     track->patch=b;
     break;
    case 2:
     b=xml_readvar_int(sf2hdr,a);
     parser_printf(-1,"Bank:%d ",b);
     track->bank=b;
     break;
    case 3:
     f=xml_readvar_float(sf2hdr,a);
     parser_printf(-1,"Gain:%.02g ",f);
     track->gain=f;
     break;
   }
  }
  parser_printf(-1,"\n");
 }

 if(token!=NULL&& ( !(sf2hdr->alone_token|sf2hdr->close_token) ))
 {
  do{
   r=xml_read_token(token,kontext,1);
   switch(xml_lookupname(token,"sf2player\0patch\0bank\0gain\0connection\0\0",0) )
   {
    case 1:
     if(token->close_token) r=1;
     break;
    case 2:
     add_control_link(kontext,token,NULL,&(track->vol),127.0,0.0,"patch",1);     
     break;
    case 3:
     add_control_link(kontext,token,NULL,&(track->pan),128.0,0.0,"bank",1);     
     break;
    case 4:
     add_control_link(kontext,token,&(track->fxch),NULL,5.0,0,"gain",1);
     break;
    case 5: /*Just ignore "connection" and treat its content like it was were*/
     if(!(token->alone_token|token->close_token))
      parser_printf(1,"V lfo V\n");
     else
      parser_printf(1,"^ lfo ^\n");
     break; 
    default:
     if(!(token->alone_token|token->close_token))
      xml_skiptoclose(kontext,token->variable[0]);
     break;
    }
   }while(!r); 
  } 

  xml_free_token(token);
 
 if(track->bank>=120)
 {
  if(midichannel_drum<0)
  {
   /*printf("Midichannel_Drum= %d\n",track->midichannel);*/
   midichannel_drum=track->midichannel;
  }
  else
  {
   track->midichannel=midichannel_drum;
   midichannels_needed--;
   if(midichannels_needed==DRUMCHANNEL)
    midichannels_needed--;    
  }
 }
 return 0;
}

/*Read instrument-block*/
int parse_instrument(struct XML_CONTEXT *kontext,int bassline,struct MMP_TRACK *track)
{
 int r=0;
 struct XML_TOKEN *token;
 token=xml_alloc_token();
 if(token!=NULL)
 {
  do{
   r=xml_read_token(token,kontext,1);
   switch(xml_lookupname(token,"instrument\0sf2player\0\0",0) )
   {
    case 1:
     if(token->close_token) r=1;
     break;
    case 2:
     parser_printf(1,"sf2player:\n");
     parser_column++;
     parse_sf2player(kontext,bassline,token,track);
     parser_column--;
     break;
    default:
     if(!(token->alone_token|token->close_token))
      xml_skiptoclose(kontext,token->variable[0]);
     break;
   } 
  } while(!r);
  xml_free_token(token);

  return r;
 }
 return -1;
}


/*Read instrumenttrack-block*/
int parse_instrumenttrack(struct XML_CONTEXT *kontext,int bassline,struct XML_TOKEN *ithdr,struct MMP_TRACK *track)
{
 int r=0;
 struct XML_TOKEN *token=NULL;
 int a,b;
 float f;  
  
 if(ithdr!=NULL)
 {
  parser_printf(1,"");
  for(a=0;a<ithdr->var_count;a++)
  {
   switch( xml_lookupname(ithdr,"vol\0pan\0fxch\0pitch\0basenote\0\0",a) )
   {
    case 1:
     f=xml_readvar_float(ithdr,a);
     parser_printf(-1,"Volume:%.02g ",f);
     track->vol=f;
     break;
    case 2:
     f=xml_readvar_float(ithdr,a);
     parser_printf(-1,"Panning:%.02g ",f);
     track->pan=f;
     break;
    case 3:
     track->fxch=b=xml_readvar_int(ithdr,a);
     parser_printf(-1,"Fxch:%d ",b);
     break;
    case 4:
     f=xml_readvar_float(ithdr,a);
     parser_printf(-1,"Pitch:%.02g ",f);
     track->pitch=f;
     break;
    case 5:
     track->key=b=xml_readvar_int(ithdr,a);
     parser_printf(-1,"Basenote:%d ",b);
     break;
   }
  }
  parser_printf(-1,"\n");
 }
 
 token=xml_alloc_token();
 if(token!=NULL)
 {
  do{
   r=xml_read_token(token,kontext,1);
   switch(xml_lookupname(token,"instrumenttrack\0instrument\0vol\0pan\0fxch\0pitch\0basenote\0connection\0\0",0))
   {
    case 1:
     if(token->close_token) r=1;
     break;
    case 2:
     parser_printf(1,"instrument\n");
     parser_column++;
     parse_instrument(kontext,bassline,track);
     parser_column--;
     break;
    case 3:
     add_control_link(kontext,token,NULL,&(track->vol),200.0,0.0,"vol",1);     
     break;
    case 4:
     add_control_link(kontext,token,NULL,&(track->pan),200.0,-100.0,"pan",1);     
     break;
    case 5:
     add_control_link(kontext,token,&(track->fxch),NULL,1.0,0,"fxch",1);     
     break;
    case 6:
     add_control_link(kontext,token,NULL,&(track->pitch),200.0,-100.0,"pitch",1);     
     break;
    case 7:
     add_control_link(kontext,token,&(track->key),NULL,48.0,48.0,"basenote",1);     
     break;
    case 8: /*Just ignore "connection" and treat its content like it was were*/
     if(!(token->alone_token|token->close_token))
      parser_printf(1,"V lfo V\n");
     else
      parser_printf(1,"^ lfo ^\n");
     break; 
    default:
     if(!(token->alone_token|token->close_token))
      xml_skiptoclose(kontext,token->variable[0]);
     break;
   } 
  } while(!r);
  xml_free_token(token);
  return r;
 }
 return -1;
}

/*read note token*/
int read_note(struct XML_TOKEN *token,struct MMP_PATTERN *pattern)
{
 int a,b;
 float f;
 struct MMP_NOTE *note=NULL;
 
 note=mmp_add_note(pattern);

 parser_printf(2,"Note ");
 for(a=0;a<token->var_count;a++)
 {   
  switch( xml_lookupname(token,"pan\0key\0vol\0pos\0len\0\0",a) )
  {
   case 1:
    b=note->pan=xml_readvar_int(token,a);
    parser_printf(-2,"panning %d ",b);
    break;
   case 2:    
    b=note->key=xml_readvar_int(token,a);
    parser_printf(-2,"key %d ",b);
    break;
   case 3:
    b=note->vol=xml_readvar_int(token,a);
    parser_printf(-2,"volume %d ",b);
    break;
   case 4:
    b=note->pos=xml_readvar_int(token,a);
    parser_printf(-2,"pos %d ",b);
    break;
   case 5:
    b=note->len=xml_readvar_int(token,a);
    parser_printf(-2,"len %d ",b);
    break;
  } 
 }
 parser_printf(-2,"\n");
 
 {
  long t;
  t=(long)note->pos+(long)note->len+(long)pattern->pos;
  if(t>end_time) end_time=t;
 }
 return 0;        
}

/*read time token (automation)*/
int read_cue(struct XML_TOKEN *token,struct MMP_PATTERN *pattern)
{
 int a,b;
 float f;
 struct MMP_NOTE *note=NULL;
 
 note=mmp_add_note(pattern);

/* parser_printf(2,"Cue ");*/
 for(a=0;a<token->var_count;a++)
 {   
  switch( xml_lookupname(token,"pos\0value\0\0",a) )
  {
   case 1:
    b=note->pos=xml_readvar_int(token,a);
    parser_printf(-2,"pos %d ",b);
    break;
   case 2:
    b=note->vol=xml_readvar_int(token,a);
    f=note->val=xml_readvar_float(token,a);
    parser_printf(-2,"value i:%d f:%.02f ",b,f);
    break;
  } 
 }
 parser_printf(-2,"\n");
 
 return 0;        
}


/*parse automationpattern bassline means 1=Baseline 0=Main*/
int parse_automationpattern(struct XML_CONTEXT *kontext,int bassline,struct XML_TOKEN *phdr,struct MMP_TRACK *track)
{
 int r=0;
 struct XML_TOKEN *token=NULL;
 int a,b;
 float f;  
 
 struct MMP_PATTERN *pattern=NULL;

 struct MMP_CONNECTION *con=NULL;
 
 pattern=mmp_alloc_pattern(&(track->pattern));
 if(!bassline) pattern->bassline=0;
  
 if(phdr!=NULL)
 {
  parser_printf(2,"");
  for(a=0;a<phdr->var_count;a++)
  {
   switch( xml_lookupname(phdr,"name\0pos\0len\0\0",a) )
   {
    case 1:
     parser_printf(-1,"name:%s ",phdr->content[a]);
     break;
    case 2:
     if(!bassline)
     {
      b=xml_readvar_int(phdr,a);
      parser_printf(-2,"pos:%d ",b);
      pattern->pos=b;
     }
     break;
    case 3:
     b=xml_readvar_int(phdr,a);
     parser_printf(-2,"len:%d ",b);
     pattern->len=b;
     break;
   }
  }
  parser_printf(-2,"\n");
 }
 
 token=xml_alloc_token();
 if(token!=NULL)
 {
  do{
   r=xml_read_token(token,kontext,1);
   switch(xml_lookupname(token,"automationpattern\0time\0object\0\0",0))
   {
    case 1:
     if(token->close_token) r=1;
     break;
    case 2:
     parser_column++;
     read_cue(token,pattern);
     parser_column--;
     if(!token->alone_token)     
      xml_skiptoclose(kontext,token->variable[0]);
     break;
    case 3:
     

     parser_printf(2,"");
     for(a=0;a<token->var_count;a++)
     {
      switch( xml_lookupname(token,"id\0\0",a) )
      {
        case 1:         
         parser_printf(1,"Object_id:%s ",token->content[a]);         
         con=mmp_alloc_conection(token->content[a]);
         pattern->connection=con;
         break;
      }
     }
  parser_printf(-2,"\n");


     
     if(!token->alone_token)     
      xml_skiptoclose(kontext,token->variable[0]);
     break;
    default:
     if(!(token->alone_token|token->close_token))
      xml_skiptoclose(kontext,token->variable[0]);
     break;
   } 
  } while(!r);
  xml_free_token(token);
  return r;
 }
 return -1;
}


/*parse pattern Bassline means: 1=bassline 0=main*/
int parse_pattern(struct XML_CONTEXT *kontext,int bassline,struct XML_TOKEN *phdr,struct MMP_TRACK *track)
{
 int r=0;
 struct XML_TOKEN *token=NULL;
 int a,b;
 float f;  
 
 struct MMP_PATTERN *pattern=NULL;
 
 pattern=mmp_alloc_pattern(&(track->pattern));
 if(!bassline) pattern->bassline=0;
  
 if(phdr!=NULL)
 {
  parser_printf(2,"");
  for(a=0;a<phdr->var_count;a++)
  {
   switch( xml_lookupname(phdr,"steps\0muted\0type\0name\0pos\0len\0frozen\0\0",a) )
   {
    case 1:
     b=xml_readvar_int(phdr,a);
     parser_printf(-2,"steps:%d ",b);
     pattern->steps=b;
     break;
    case 2:
     b=xml_readvar_int(phdr,a);
     parser_printf(-2,"muted:%d ",b);
     pattern->muted=b;
     break;
    case 3:
     b=xml_readvar_int(phdr,a);
     parser_printf(-2,"type:%d ",b);
     pattern->type=b;
     break;
    case 4:
     parser_printf(-2,"name:%s ",phdr->content[a]);
     break;
    case 5:
     if(!bassline)
     {
      b=xml_readvar_int(phdr,a);
      parser_printf(-2,"pos:%d ",b);
      pattern->pos=b;
     }
     break;
    case 6:
     b=xml_readvar_int(phdr,a);
     parser_printf(-2,"len:%d ",b);
     pattern->len=b;
     break;
    case 7:
     b=xml_readvar_int(phdr,a);
     parser_printf(-2,"frozen:%d ",b);
     pattern->frozen=b;
     break;
   }
  }
  parser_printf(-2,"\n");
 }

 {
  long t;
  t=(long)pattern->len+(long)pattern->pos;
  if(t>end_time) end_time=t;
 }
 
 if(phdr->alone_token) return 0;
 
 token=xml_alloc_token();
 if(token!=NULL)
 {
  do{
   r=xml_read_token(token,kontext,1);
   switch(xml_lookupname(token,"pattern\0note\0\0",0))
   {
    case 1:
     if(token->close_token) r=1;
     break;
    case 2:
     /*parser_printf(2,"Note\n");*/
     parser_column++;
     read_note(token,pattern);
     parser_column--;
     if(!token->alone_token)     
      xml_skiptoclose(kontext,token->variable[0]);
     break;
    default:
     if(!(token->alone_token|token->close_token))
      xml_skiptoclose(kontext,token->variable[0]);
     break;
   } 
  } while(!r);
  xml_free_token(token);
  return r;
 }
 return -1;
}

/*parse BBtrack. First contains Trackcontainer with all tracks an patterns
  following ones are Empty and only tag track as BB-Track*/
int parse_bbtrack(struct XML_CONTEXT *kontext,/*int bassline,*/struct MMP_TRACK *track)
{
 int r=0;
 struct XML_TOKEN *token=NULL;
 int a,b;
 float f;  
    
 token=xml_alloc_token();
 if(token!=NULL)
 {
  do{
   r=xml_read_token(token,kontext,1);
   switch(xml_lookupname(token,"bbtrack\0trackcontainer\0\0",0))
   {
    case 1:
     if(token->close_token) r=1;
     break;
    case 2:
     parser_printf(1,"trackcontainer (beat/bassline):\n");
     parser_column++;
     parse_trackcontainer(kontext,1,token,track);
     parser_column--;
     break;
    default:
     if(!(token->alone_token|token->close_token))
      xml_skiptoclose(kontext,token->variable[0]);
     break;
   } 
  } while(!r);
  xml_free_token(token);
  return r;
 }
 return -1;
}

/*Read Bassline track in Main tracks (Basslines cannot be nested)*/
int read_bbtco(struct XML_TOKEN *token,struct MMP_TRACK *track)
{
 int r=0;
 int a,b;
 float f;  

 struct MMP_PATTERN *pattern=NULL;
 pattern=mmp_alloc_pattern(&(track->pattern));
   
 if(token!=NULL&&pattern!=NULL)
 {
  /*parser_printf(1,"");*/
  for(a=0;a<token->var_count;a++)
  {
   switch( xml_lookupname(token,"pos\0len\0muted\0\0",a) )
   {
    case 1:
     b=xml_readvar_int(token,a);
     parser_printf(-1,"Pos:%d ",b);
     pattern->pos=b;
     break;
    case 2:
     b=xml_readvar_int(token,a);
     parser_printf(-1,"Len:%d ",b);
     pattern->len=b;
     break;
    case 3:
     b=xml_readvar_int(token,a);
     parser_printf(-1,"Muted:%d ",b);
     pattern->muted=b;
     break;
   }
  }
  parser_printf(-1,"\n");
 }

 {
  long t;
  t=(long)pattern->len+(long)pattern->pos;
  if(t>end_time) end_time=t;
 }
 
 return 0;
}


/*Read Track bassline means 1= bassline 0= main*/
int parse_track(struct XML_CONTEXT *kontext,int bassline,struct XML_TOKEN *trackhdr)
{
 int r=0;
 struct XML_TOKEN *token=NULL;
 struct MMP_TRACK *track=NULL;
 int a,b;
 float f;  
 int tracktyp=-1,muted=0;

 track=bassline?mmp_alloc_bassline_track():mmp_alloc_main_track();
  
 if(trackhdr!=NULL&&track!=NULL)
 {
  track->midichannel=-1;
  track->bassline=-1;
                                
  parser_printf(1,"");
  for(a=0;a<trackhdr->var_count;a++)
  {
   switch( xml_lookupname(trackhdr,"type\0muted\0name\0\0",a) )
   {
    case 1:
     track->typ=b=xml_readvar_int(trackhdr,a);
     parser_printf(-1,"tracktyp:%d ",b);
     break;
    case 2:
     track->mute=xml_readvar_int(trackhdr,a);
     parser_printf(-1,"muted:%d ",muted);
     break;
    case 3:
     parser_printf(-1,"name:%s ",trackhdr->content[a]);
     break;
   }
  }
  parser_printf(-1,"\n");
 }
 
 
 token=xml_alloc_token();
 if(token!=NULL&&track!=NULL)
 {
  do{
   r=xml_read_token(token,kontext,1);
   switch(xml_lookupname(token,"track\0instrumenttrack\0pattern\0automationtrack\0automationpattern\0bbtrack\0bbtco\0\0",0))
   {
    case 1:
     if(token->close_token) r=1;
     break;
    case 2:
     parser_printf(1,"instrumenttrack:\n");
     parser_column++;
     parse_instrumenttrack(kontext,bassline,token,track);
     parser_column--;
     break;
    case 3:
     if(track->midichannel<0)
     {
      parser_printf(1,"Skip pattern of non-MIDI track\n");                             
      goto SKIP;
     }
     parser_printf(1,"pattern:\n");
     parser_column++;
     parse_pattern(kontext,bassline,token,track);
     parser_column--;
     break;
    case 4:
     parser_printf(1,"automationtrack:\n");
     if(!(token->alone_token|token->close_token))
      xml_skiptoclose(kontext,token->variable[0]);
     break;
    case 5:
     parser_printf(1,"automationpattern:\n");
     parser_column++;
     parse_automationpattern(kontext,bassline,token,track);
     parser_column--;
     break;
    case 6:
     if(bassline) goto SKIP;

     parser_printf(1,"bbtrack:\n");
     parser_column++;
     if(!(token->alone_token|token->close_token))
      parse_bbtrack(kontext,/*bassline,*/track);
     parser_column--;
     track->loop_len=mmp_get_loop_len(basslines);
     track->bassline=(basslines++);
     break;
    case 7:
     parser_printf(1,"bbtco:");
     parser_column++;
     read_bbtco(token,track);
     parser_column--;
     if(!(token->alone_token|token->close_token))
      xml_skiptoclose(kontext,token->variable[0]);
     break;
    default:
     SKIP:
     if(!(token->alone_token|token->close_token))
      xml_skiptoclose(kontext,token->variable[0]);
     break;
   } 
  } while(!r);
  xml_free_token(token);
  return r;
 }
 return -1;
}


/*parse trackcontainer Bassline: 1=bassline 0=main*/
int parse_trackcontainer(struct XML_CONTEXT *kontext, int baseline)
{
 int r=0;
 struct XML_TOKEN *token;
 token=xml_alloc_token();
 if(token!=NULL)
 {
  do{
   r=xml_read_token(token,kontext,1);
   switch(xml_lookupname(token,"trackcontainer\0track\0\0",0) )
   {
    case 1:
     if(token->close_token) r=1;
     break;
    case 2:
     parser_printf(1,"track:\n");
     parser_column++;
     parse_track(kontext,baseline,token);
     parser_column--;
     break;
    default:
     if(!(token->alone_token|token->close_token))
      xml_skiptoclose(kontext,token->variable[0]);
     break;
   } 
  } while(!r);
  xml_free_token(token);
  return r;
 }
 return -1;
}

/*parse fxchannel (theese affect volume) id=0 for main*/
int parse_fxchannel(struct XML_TOKEN *token)
{
 int a;
 int num=-1,muted=0;
 float volume=0;
 
 
 parser_printf(2,"");
 for(a=0;a<token->var_count;a++)
 {   
  switch( xml_lookupname(token,"num\0muted\0volume\0\0",a) )
  {
   case 1:
    num=xml_readvar_int(token,a);
    parser_printf(-2," num %d",num);
    break;
   case 2:
    muted=xml_readvar_int(token,a);
    parser_printf(-2," muted %d",muted);
    break;
   case 3:
    volume=xml_readvar_float(token,a);
    parser_printf(-2," volume %.2f",volume);
    break;
  } 
 }
 parser_printf(-2,"\n");
 
 if(num==0) 
 {
  main_channel.volume=volume;
  main_channel.muted=muted;
 }
 if(num>0 &&num< MAX_FXCHANNELS)
 {
  mixchannel[num-1].volume=volume;         
  mixchannel[num-1].muted=muted;         
 }
 
 return 0;        
}

/*parse fxmixer (container for channels)*/
int parse_fxmixer(struct XML_CONTEXT *kontext)
{
 int r=0;
 struct XML_TOKEN *token;
 token=xml_alloc_token();
 if(token!=NULL)
 {
  do{
   r=xml_read_token(token,kontext,1);
   switch(xml_lookupname(token,"fxmixer\0fxchannel\0\0",0))
   {
    case 1:
     if(token->close_token) r=1;
     break;
    case 2:
     parser_printf(2,"fxchannel:\n");
     parser_column++;
     parse_fxchannel(token);     
     parser_column--;
     /*fall*/
    default:
     if(!(token->alone_token|token->close_token))
      xml_skiptoclose(kontext,token->variable[0]);
     break;
   } 
  } while(!r);
  xml_free_token(token);
  return r;
 }
 return -1;
}

/*parse lfocontroller*/
int read_lfocontroller(struct XML_TOKEN *token)
{
 int r=0,a,b;
 float f;
 struct MMP_LFO *lfo=NULL;

 lfo=mmp_alloc_lfo();
 if(lfo==NULL) return -1;

 parser_printf(1,"");
 for(a=0;a<token->var_count;a++)
 {             
  switch( xml_lookupname(token,"speed\0base\0syncmode\0amount\0multiplier\0speed_numerator\0type\0wave\0speed_denominator\0\0",a) )
  {
   case 1:
    lfo->speed=f=xml_readvar_float(token,a);
    parser_printf(-1," speed %.2g",f);
    break;
   case 2:
    lfo->base=f=xml_readvar_float(token,a);
    parser_printf(-1," base %.2g",f);
    break;
   case 3:
    lfo->syncmode=b=xml_readvar_float(token,a);
    parser_printf(-1," syncmode %d",b);
    break;
   case 4:
    lfo->amount=f=xml_readvar_float(token,a);
    parser_printf(-1," amount %.2g",f);
    break;
   case 5:
    lfo->multiplier=f=xml_readvar_float(token,a);
    parser_printf(-1," multiplier %.2g",f);
    break;
   case 6:
    lfo->speed_numerator=f=xml_readvar_float(token,a);
    parser_printf(-1," speed_numerator %.2g",f);
    break;
   case 7:
    lfo->type=b=xml_readvar_int(token,a);
    parser_printf(-1," type %d",b);
    break;
   case 8:
    lfo->wave=b=xml_readvar_int(token,a);
    parser_printf(-1," wave %d",b);
    break;
   case 9:
    lfo->speed_denominator=f=xml_readvar_float(token,a);
    parser_printf(-1," speed_denominator %.2g",f);
    break;
  } 
 }
 parser_printf(-1,"\n");

 return 0;
}

/*parse container for controllers. Id is entry-number*/
int parse_controllers(struct XML_CONTEXT *kontext)
{
 int r=0;
 struct XML_TOKEN *token;
 int n_lfos=0;
 token=xml_alloc_token();
 if(token!=NULL)
 {
  do{
   r=xml_read_token(token,kontext,1);
   switch(xml_lookupname(token,"controllers\0lfocontroller\0\0",0))
   {
    case 1:
     if(token->close_token) r=1;
     break;
    case 2:
     parser_column++;
     parser_printf(1,"lfocontroller:\n");
     read_lfocontroller(token);
     if(lfos!=NULL)
     {
      char buffer[12];
      sprintf(buffer,"%d",n_lfos);
      lfos->connection=mmp_alloc_conection(buffer);
      parser_printf(1,"connection lfo-controller %s\n",buffer);
      n_lfos++;
     }
     parser_column--;
     if(!token->alone_token)     
      xml_skiptoclose(kontext,token->variable[0]);
     break;
    default:
     parser_printf(1,"...:\n");
     if(!token->alone_token)     
      xml_skiptoclose(kontext,token->variable[0]);
     break;
   } 
  } while(!r);
  xml_free_token(token);
  return r;
 }
 return -1;
}

/*read begin / end of loop*/
int read_timeline(struct XML_TOKEN *token)
{
 int r=0,a,b;

 parser_printf(1,"");
 for(a=0;a<token->var_count;a++)
 {   
  switch( xml_lookupname(token,"lp1pos\0lp0pos\0lpstate\0\0",a) )
  {
   case 1:
    b=globals.lp1pos=xml_readvar_int(token,a);
    parser_printf(-1," lp1pos %d",b);
    break;
   case 2:
    b=globals.lp0pos=xml_readvar_int(token,a);
    parser_printf(-1," lp0pos %d",b);
    break;
   case 3:
    b=globals.lpstate=xml_readvar_int(token,a);
    parser_printf(-1," lpstate %d",b);
    break;
  } 
 }
 parser_printf(-1,"\n");

 return 0;
}

/*parse song structure the "body" element*/
int parse_song(struct XML_CONTEXT *kontext)
{
 int r=0;
 struct XML_TOKEN *token;
 token=xml_alloc_token();
 if(token!=NULL)
 {
  do{
   r=xml_read_token(token,kontext,1);
   switch(xml_lookupname(token,"song\0trackcontainer\0track\0fxmixer\0controllers\0timeline\0\0",0))
   {
    case 1:
     if(token->close_token) r=1;
     break;
    case 2:
     parser_printf(1,"trackcontainer (main tracks):\n");
     parser_column++;
     if(!token->alone_token)
      parse_trackcontainer(kontext,0);
     parser_column--;
     break;
    case 3:
     parser_printf(1,"track:\n");
     parser_column++;
     if(!token->alone_token)
      parse_track(kontext,0,token);
     parser_column--;
     break;
    case 4:
     parser_printf(1,"fxmixer:\n");
     parser_column++;
     if(!token->alone_token)
      parse_fxmixer(kontext);
     parser_column--;
     break;
    case 5:
     parser_printf(1,"controllers:\n");
     parser_column++;
     if(!token->alone_token)
      parse_controllers(kontext);
     parser_column--;
     break;
    case 6:
     parser_printf(1,"timeline:\n");
     parser_column++;
      read_timeline(token);
     parser_column--;
     if(!token->alone_token)
      xml_skiptoclose(kontext,token->variable[0]);
     break;
    default:
     if(!token->alone_token)
      xml_skiptoclose(kontext,token->variable[0]);
     break;
   } 
  } while(!r);
  xml_free_token(token);
  return r;
 }
 return -1;
}


/*read head element*/
int read_head(struct XML_TOKEN *token)
{
 int a,b;
 float f;
 globals.vol=100;
 globals.key=0;
 globals.bpm=120;
 globals.clk_z=4;
 globals.clk_n=4;

 for(a=0;a<token->var_count;a++)
 {   
  switch( xml_lookupname(token,"timesig_numerator\0mastervol\0timesig_denominator\0bpm\0masterpitch\0\0",a) )
  {
   case 1:
    b=globals.clk_z=xml_readvar_int(token,a);
    parser_printf(2,"clock_numerator %d\n",b);
    break;
   case 2:    
    f=globals.vol=xml_readvar_float(token,a);
    parser_printf(2,"volume %.2g\n",f);
    break;
   case 3:
    b=globals.clk_n=xml_readvar_int(token,a);
    parser_printf(2,"clock_denominator %d\n",b);
    break;
   case 4:
    b=globals.bpm=xml_readvar_int(token,a);
    parser_printf(2,"bpm %d\n",b);
    break;
   case 5:
    b=globals.key=xml_readvar_int(token,a);
    parser_printf(2,"key %d\n",b);
    break;
  } 
 }
 return 0;        
}

/*parser multimedia-project ~ XML-Root-Element*/
int parse_project(struct XML_CONTEXT *kontext)
{
 int r=0;
 struct XML_TOKEN *token;
 token=xml_alloc_token();
 if(token!=NULL)
 {
  do{
   r=xml_read_token(token,kontext,1);
   switch(xml_lookupname(token,"head\0song\0multimedia-project\0\0",0))
   {
    case 1:
     parser_printf(1,"head:\n");
     parser_column++;
      read_head(token);
     parser_column--;
     if(!token->alone_token)
      xml_skiptoclose(kontext,token->variable[0]);
     break;
    case 2:
     if(!(token->alone_token|token->close_token))
     {
      parser_printf(1,"song:\n");
      parser_column++;
      if(!token->alone_token)
       parse_song(kontext);
      parser_column--;
     }
     break;
    case 3:
     if(token->close_token) r=1;
     break;
    default:
     parser_printf(1,"+\n");
     if(!token->alone_token)
      xml_skiptoclose(kontext,token->variable[0]);
     break;
   } 
  } while(!r);
  xml_free_token(token);
  return r;
 }
 return -1;
}


/*Parse the File. Skip XML-Header.*/
int parse_mmp(struct XML_CONTEXT *kontext)
{
 int r=0;
 struct XML_TOKEN *token;
 token=xml_alloc_token();
 if(token!=NULL)
 {
  do{
   r=xml_read_token(token,kontext,1);
   if(!token->close_token)
   switch( xml_lookupname(token,"?xml\0!DOCTYPE\0multimedia-project\0",0))
   {
    case 1: case 2: break;
    case 3: 
     parser_printf(1,"project:\n");
     parser_column++;
     if(!token->alone_token)
      parse_project(kontext);
     parser_column--;
     break;         
    default:
     if(!token->alone_token)
      xml_skiptoclose(kontext,token->variable[0]);
     break;
   } 
  } while(!r);
  xml_free_token(token);
  return r;
 }
 return -1;
}


/*alloc parser contect and set source file*/
int load_mmp(FILE *file)
{
 int r=0;
 struct XML_CONTEXT *kontext;
 
 midichannel_drum=-1;
 
 kontext=xml_alloc_context();
 if(kontext!=NULL)
 {
  if(!xml_set_context_file(kontext,file))
  {
   parser_column=0;
   r=parse_mmp(kontext);
  } 
  xml_free_context(kontext);
 }else r=-1;
 return r;   
}
